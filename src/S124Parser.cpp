#include "S124Parser.h"
#include <wx/log.h>
#include <wx/sstream.h>
#include <map>
#include <functional>
#include <sstream>

// ── helpers ──────────────────────────────────────────────────────────────────

wxString S124Parser::LocalName(const wxString &qname)
{
    int c = qname.Find(':');
    return (c == wxNOT_FOUND) ? qname : qname.Mid(c + 1);
}

wxString S124Parser::GetGmlId(wxXmlNode *node)
{
    wxString id = node->GetAttribute(wxT("gml:id"));
    if (!id.IsEmpty()) return id;
    return node->GetAttribute(wxT("id"));
}

wxXmlNode *S124Parser::FindChild(wxXmlNode *parent, const wxString &localName)
{
    if (!parent) return nullptr;
    wxXmlNode *c = parent->GetChildren();
    while (c) {
        if (c->GetType() == wxXML_ELEMENT_NODE &&
            LocalName(c->GetName()).IsSameAs(localName, false))
            return c;
        c = c->GetNext();
    }
    return nullptr;
}

// ── coordinate parsing ────────────────────────────────────────────────────────

void S124Parser::ParsePosList(const wxString &s,
                               std::vector<std::pair<double,double>> &out)
{
    std::istringstream ss(s.ToStdString());
    double v;
    std::vector<double> vals;
    while (ss >> v) vals.push_back(v);
    // S-100 uses (lat lon) pairs
    for (size_t i = 0; i + 1 < vals.size(); i += 2)
        out.push_back({vals[i], vals[i + 1]});
}

void S124Parser::ParsePos(const wxString &s,
                           std::vector<std::pair<double,double>> &out)
{
    std::istringstream ss(s.ToStdString());
    double lat, lon;
    if (ss >> lat >> lon)
        out.push_back({lat, lon});
}

void S124Parser::CollectPosList(wxXmlNode *node,
                                 std::vector<std::pair<double,double>> &out)
{
    // Direct posList child
    wxXmlNode *plN = FindChild(node, wxT("posList"));
    if (plN) { ParsePosList(plN->GetNodeContent().Strip(), out); return; }

    // Walk children for pos nodes or nested posList (Curve segments)
    wxXmlNode *c = node->GetChildren();
    while (c) {
        if (c->GetType() == wxXML_ELEMENT_NODE) {
            wxString ln = LocalName(c->GetName());
            if (ln == wxT("pos")) {
                ParsePos(c->GetNodeContent().Strip(), out);
            } else {
                wxXmlNode *nested = FindChild(c, wxT("posList"));
                if (nested) ParsePosList(nested->GetNodeContent().Strip(), out);
            }
        }
        c = c->GetNext();
    }
}

// ── geometry node ─────────────────────────────────────────────────────────────

void S124Parser::ParseGeomNode(wxXmlNode *node, std::vector<S124Geometry> &geoms)
{
    if (!node) return;
    wxString ln = LocalName(node->GetName());

    if (ln == wxT("Point")) {
        S124Geometry g; g.type = S124Geometry::POINT;
        wxXmlNode *posN = FindChild(node, wxT("pos"));
        if (posN) ParsePos(posN->GetNodeContent().Strip(), g.coords);
        else {
            wxXmlNode *plN = FindChild(node, wxT("posList"));
            if (plN) ParsePosList(plN->GetNodeContent().Strip(), g.coords);
        }
        if (!g.coords.empty()) geoms.push_back(g);

    } else if (ln == wxT("LineString") || ln == wxT("Curve")) {
        S124Geometry g; g.type = S124Geometry::CURVE;
        CollectPosList(node, g.coords);
        if (!g.coords.empty()) geoms.push_back(g);

    } else if (ln == wxT("Surface")) {
        // S-100 Surface: patches > PolygonPatch > exterior > LinearRing
        wxXmlNode *patches = FindChild(node, wxT("patches"));
        if (patches) {
            wxXmlNode *c = patches->GetChildren();
            while (c) {
                if (c->GetType() == wxXML_ELEMENT_NODE)
                    ParseGeomNode(c, geoms);
                c = c->GetNext();
            }
        }

    } else if (ln == wxT("Polygon") || ln == wxT("PolygonPatch")) {
        S124Geometry g; g.type = S124Geometry::SURFACE;
        wxXmlNode *ext  = FindChild(node, wxT("exterior"));
        wxXmlNode *ring = ext ? FindChild(ext, wxT("LinearRing"))
                              : FindChild(node, wxT("LinearRing"));
        wxXmlNode *plN  = ring ? FindChild(ring, wxT("posList")) : nullptr;
        if (plN) ParsePosList(plN->GetNodeContent().Strip(), g.coords);
        if (g.coords.empty() && ring) CollectPosList(ring, g.coords);
        if (!g.coords.empty()) geoms.push_back(g);

    } else if (ln.EndsWith(wxT("Property"))) {
        // Unwrap S-100 property containers: pointProperty, curveProperty, surfaceProperty
        wxXmlNode *c = node->GetChildren();
        while (c) {
            if (c->GetType() == wxXML_ELEMENT_NODE)
                ParseGeomNode(c, geoms);
            c = c->GetNext();
        }

    } else if (ln.StartsWith(wxT("Multi")) || ln == wxT("GeometryCollection")) {
        // Recurse into member elements
        wxXmlNode *c = node->GetChildren();
        while (c) {
            if (c->GetType() == wxXML_ELEMENT_NODE) {
                wxXmlNode *gc = c->GetChildren();
                while (gc) {
                    if (gc->GetType() == wxXML_ELEMENT_NODE)
                        ParseGeomNode(gc, geoms);
                    gc = gc->GetNext();
                }
            }
            c = c->GetNext();
        }
    }
}

// ── feature extraction ────────────────────────────────────────────────────────

void S124Parser::ExtractNwPartGeoms(wxXmlNode *node, std::vector<S124Geometry> &geoms)
{
    wxXmlNode *c = node->GetChildren();
    while (c) {
        if (c->GetType() == wxXML_ELEMENT_NODE &&
            LocalName(c->GetName()) == wxT("geometry")) {
            wxXmlNode *gc = c->GetChildren();
            while (gc) {
                if (gc->GetType() == wxXML_ELEMENT_NODE)
                    ParseGeomNode(gc, geoms);
                gc = gc->GetNext();
            }
        }
        c = c->GetNext();
    }
}

void S124Parser::ExtractNavwarn(wxXmlNode *node,
                                 S124Warning &w,
                                 std::vector<wxString> &partRefs)
{
    w.id = GetGmlId(node);

    wxXmlNode *c = node->GetChildren();
    while (c) {
        if (c->GetType() != wxXML_ELEMENT_NODE) { c = c->GetNext(); continue; }

        wxString ln = LocalName(c->GetName());

        if (ln == wxT("messageSeriesIdentifier")) {
            // warningNumber, year, nameOfSeries, warningType live inside this container
            wxXmlNode *mc = c->GetChildren();
            while (mc) {
                if (mc->GetType() == wxXML_ELEMENT_NODE) {
                    wxString mln = LocalName(mc->GetName());
                    wxString mcv = mc->GetNodeContent();
                    if (mln == wxT("warningNumber")) {
                        long v = 0; mcv.ToLong(&v); w.warningNumber = (int)v;
                    } else if (mln == wxT("year")) {
                        long v = 0; mcv.ToLong(&v); w.year = (int)v;
                    } else if (mln == wxT("nameOfSeries")) {
                        w.seriesName = mcv;
                    } else if (mln == wxT("warningType")) {
                        // value is in the "code" attribute, not the text content
                        wxString code = mc->GetAttribute(wxT("code"));
                        long v = 0; code.ToLong(&v); w.warningType = (int)v;
                    }
                }
                mc = mc->GetNext();
            }
        } else if (ln == wxT("navwarnTypeGeneral")) {
            // code attribute carries the numeric category; element text is its label
            wxString code = c->GetAttribute(wxT("code"));
            long v = 0; code.ToLong(&v); w.warningTypeDetail = (int)v;
            w.warningCategory = c->GetNodeContent();
        } else if (ln == wxT("publicationTime")) {
            w.publicationTime = c->GetNodeContent();
        } else if (ln == wxT("cancellationDate")) {
            w.cancellationDate = c->GetNodeContent();
        } else if (ln == wxT("generalArea") || ln == wxT("locality")) {
            // Build areaText from area/locality names
            wxXmlNode *ln_node = FindChild(c, wxT("locationName"));
            if (ln_node) {
                wxXmlNode *txt = FindChild(ln_node, wxT("text"));
                wxXmlNode *lang = FindChild(ln_node, wxT("language"));
                if (txt) {
                    if (!w.areaText.IsEmpty()) w.areaText += wxT(" / ");
                    w.areaText += txt->GetNodeContent();
                }
                if (lang && w.language.IsEmpty())
                    w.language = lang->GetNodeContent();
            }
        } else if (ln == wxT("theWarningPart")) {
            // preamble-side reference to a NavwarnPart (not used in SHOM format but handle it)
            wxString href = c->GetAttribute(wxT("xlink:href"));
            if (href.IsEmpty()) href = c->GetAttribute(wxT("href"));
            if (!href.IsEmpty()) {
                if (href.StartsWith(wxT("#"))) href = href.Mid(1);
                partRefs.push_back(href);
            }
        }
        c = c->GetNext();
    }
}

// ── document parsing ──────────────────────────────────────────────────────────

bool S124Parser::ParseDoc(wxXmlDocument &doc,
                           std::vector<S124Warning> &out,
                           wxString &err)
{
    wxXmlNode *root = doc.GetRoot();
    if (!root) { err = _("Empty S-124 document."); return false; }

    // Pass 1: collect NavwarnPreamble warnings and NavwarnPart geometries
    // Parts link back to their preamble via <S124:header xlink:href="#<preamble-id>"/>
    std::map<wxString, S124Warning> preambles;  // gml:id → warning

    struct PartData {
        std::vector<S124Geometry> geoms;
        wxString preambleId;
        wxString warningText;
        wxString warningSubject;
        wxString effectiveStart;
        wxString effectiveEnd;
        wxString language;
    };
    std::vector<PartData> parts;

    // dateStart/dateEnd hold an S100:date child; fall back to raw content
    // for datasets that put the date directly on the element.
    auto readDateBound = [&](wxXmlNode *bound) -> wxString {
        if (!bound) return wxEmptyString;
        wxXmlNode *dateN = FindChild(bound, wxT("date"));
        return (dateN ? dateN : bound)->GetNodeContent();
    };

    std::function<void(wxXmlNode*)> walk = [&](wxXmlNode *n) {
        while (n) {
            if (n->GetType() == wxXML_ELEMENT_NODE) {
                wxString ln = LocalName(n->GetName());
                if (ln == wxT("NavwarnPreamble")) {
                    S124Warning w;
                    std::vector<wxString> unused;
                    ExtractNavwarn(n, w, unused);
                    if (!w.id.IsEmpty())
                        preambles[w.id] = std::move(w);
                } else if (ln == wxT("NavwarnPart")) {
                    PartData pd;
                    ExtractNwPartGeoms(n, pd.geoms);
                    // find the header reference that points back to the preamble,
                    // plus the actual warning message content carried by this part
                    wxXmlNode *c = n->GetChildren();
                    while (c) {
                        if (c->GetType() == wxXML_ELEMENT_NODE) {
                            wxString cln = LocalName(c->GetName());
                            if (cln == wxT("header")) {
                                wxString href = c->GetAttribute(wxT("xlink:href"));
                                if (href.IsEmpty()) href = c->GetAttribute(wxT("href"));
                                if (href.StartsWith(wxT("#"))) href = href.Mid(1);
                                pd.preambleId = href;
                            } else if (cln == wxT("warningInformation")) {
                                wxXmlNode *infoNode = FindChild(c, wxT("information"));
                                if (infoNode) {
                                    wxXmlNode *txt = FindChild(infoNode, wxT("text"));
                                    if (txt) pd.warningText = txt->GetNodeContent();
                                    wxXmlNode *lang = FindChild(infoNode, wxT("language"));
                                    if (lang) pd.language = lang->GetNodeContent();
                                }
                                wxXmlNode *details = FindChild(c, wxT("navwarnTypeDetails"));
                                if (details) pd.warningSubject = details->GetNodeContent();
                            } else if (cln == wxT("fixedDateRange")) {
                                wxString start = readDateBound(FindChild(c, wxT("dateStart")));
                                wxString end   = readDateBound(FindChild(c, wxT("dateEnd")));
                                wxXmlNode *tStart = FindChild(c, wxT("timeOfDayStart"));
                                wxXmlNode *tEnd   = FindChild(c, wxT("timeOfDayEnd"));
                                if (tStart) start += wxT(" ") + tStart->GetNodeContent();
                                if (tEnd)   end   += wxT(" ") + tEnd->GetNodeContent();
                                pd.effectiveStart = start;
                                pd.effectiveEnd   = end;
                            }
                        }
                        c = c->GetNext();
                    }
                    parts.push_back(std::move(pd));
                } else {
                    walk(n->GetChildren());
                }
            }
            n = n->GetNext();
        }
    };
    walk(root->GetChildren());

    // Pass 2: attach part geometries and message content to their preamble warning
    for (auto &pd : parts) {
        auto it = preambles.find(pd.preambleId);
        if (it == preambles.end()) continue;
        S124Warning &w = it->second;

        for (auto &g : pd.geoms)
            w.geometries.push_back(g);

        if (!pd.warningText.IsEmpty()) {
            if (!w.warningText.IsEmpty()) w.warningText += wxT("\n\n");
            w.warningText += pd.warningText;
        }
        if (!pd.warningSubject.IsEmpty()) {
            if (!w.warningSubject.IsEmpty()) w.warningSubject += wxT(" / ");
            w.warningSubject += pd.warningSubject;
        }
        if (w.effectiveStart.IsEmpty()) w.effectiveStart = pd.effectiveStart;
        if (w.effectiveEnd.IsEmpty())   w.effectiveEnd   = pd.effectiveEnd;
        if (w.language.IsEmpty())       w.language       = pd.language;
    }

    for (auto &kv : preambles) {
        kv.second.computeCentroid();
        out.push_back(std::move(kv.second));
    }

    if (out.empty()) {
        err = _("No S-124 navigational warnings found in the document.\n\n"
                "The file must be a valid S-124 GML dataset containing\n"
                "NavwarnPreamble features.");
        return false;
    }
    return true;
}

bool S124Parser::ParseFile(const wxString &path,
                            std::vector<S124Warning> &out,
                            wxString &err)
{
    wxXmlDocument doc;
    if (!doc.Load(path)) {
        err = wxString::Format(_("Failed to parse S-124 file:\n%s"), path);
        return false;
    }
    return ParseDoc(doc, out, err);
}

bool S124Parser::ParseString(const wxString &gml,
                              std::vector<S124Warning> &out,
                              wxString &err)
{
    wxStringInputStream sis(gml);
    wxXmlDocument doc;
    if (!doc.Load(sis)) {
        wxString snippet = gml.Left(300).Trim(true);
        err = wxString::Format(
            _("Failed to parse S-124 GML data.\n\nReceived (%zu bytes):\n%s%s"),
            (size_t)gml.size(), snippet,
            gml.size() > 300 ? wxT("\n[…]") : wxT(""));
        return false;
    }
    return ParseDoc(doc, out, err);
}
