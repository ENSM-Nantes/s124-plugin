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

    } else if (ln == wxT("Polygon")) {
        S124Geometry g; g.type = S124Geometry::SURFACE;
        wxXmlNode *ext  = FindChild(node, wxT("exterior"));
        wxXmlNode *ring = ext ? FindChild(ext, wxT("LinearRing"))
                              : FindChild(node, wxT("LinearRing"));
        wxXmlNode *plN  = ring ? FindChild(ring, wxT("posList")) : nullptr;
        if (plN) ParsePosList(plN->GetNodeContent().Strip(), g.coords);
        // pos-list fallback
        if (g.coords.empty() && ring) CollectPosList(ring, g.coords);
        if (!g.coords.empty()) geoms.push_back(g);

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
        wxString cv = c->GetNodeContent();

        if (ln == wxT("warningNumber")) {
            long v = 0; cv.ToLong(&v); w.warningNumber = (int)v;
        } else if (ln == wxT("year")) {
            long v = 0; cv.ToLong(&v); w.year = (int)v;
        } else if (ln == wxT("nameOfSeries")) {
            w.seriesName = cv;
        } else if (ln == wxT("warningType")) {
            long v = 0; cv.ToLong(&v); w.warningType = (int)v;
        } else if (ln == wxT("warningTypeDetails") || ln == wxT("navwarnTypeDetails")) {
            long v = 0; cv.ToLong(&v); w.warningTypeDetail = (int)v;
        } else if (ln == wxT("publicationTime")) {
            w.publicationTime = cv;
        } else if (ln == wxT("cancellationDate")) {
            w.cancellationDate = cv;
        } else if (ln == wxT("header")) {
            wxXmlNode *hc = c->GetChildren();
            while (hc) {
                if (hc->GetType() == wxXML_ELEMENT_NODE) {
                    wxString hln = LocalName(hc->GetName());
                    if (hln == wxT("text"))     w.headerText = hc->GetNodeContent();
                    else if (hln == wxT("language")) w.language = hc->GetNodeContent();
                }
                hc = hc->GetNext();
            }
        } else if (ln == wxT("theWarningPart")) {
            // Reference to NwPart or inline NwPart
            wxString href = c->GetAttribute(wxT("xlink:href"));
            if (href.IsEmpty()) href = c->GetAttribute(wxT("href"));
            if (!href.IsEmpty()) {
                if (href.StartsWith(wxT("#"))) href = href.Mid(1);
                partRefs.push_back(href);
            } else {
                wxXmlNode *gc = c->GetChildren();
                while (gc) {
                    if (gc->GetType() == wxXML_ELEMENT_NODE &&
                        LocalName(gc->GetName()) == wxT("NwPart"))
                        ExtractNwPartGeoms(gc, w.geometries);
                    gc = gc->GetNext();
                }
            }
        } else if (ln == wxT("geometry")) {
            // Inline geometry or reference
            wxString href = c->GetAttribute(wxT("xlink:href"));
            if (href.IsEmpty()) href = c->GetAttribute(wxT("href"));
            if (!href.IsEmpty()) {
                if (href.StartsWith(wxT("#"))) href = href.Mid(1);
                partRefs.push_back(href);
            } else {
                wxXmlNode *gc = c->GetChildren();
                while (gc) {
                    if (gc->GetType() == wxXML_ELEMENT_NODE)
                        ParseGeomNode(gc, w.geometries);
                    gc = gc->GetNext();
                }
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

    std::map<wxString, std::vector<S124Geometry>> partGeoms;

    struct Rec { S124Warning warn; std::vector<wxString> partRefs; };
    std::vector<Rec> recs;

    // Two-pass: collect all features regardless of container element
    std::function<void(wxXmlNode*)> walk = [&](wxXmlNode *n) {
        while (n) {
            if (n->GetType() == wxXML_ELEMENT_NODE) {
                wxString ln = LocalName(n->GetName());
                if (ln == wxT("NavwarnTypeGeneral")) {
                    Rec r;
                    ExtractNavwarn(n, r.warn, r.partRefs);
                    recs.push_back(std::move(r));
                } else if (ln == wxT("NwPart")) {
                    wxString id = GetGmlId(n);
                    if (!id.IsEmpty()) {
                        std::vector<S124Geometry> geoms;
                        ExtractNwPartGeoms(n, geoms);
                        if (!geoms.empty())
                            partGeoms[id] = std::move(geoms);
                    }
                } else {
                    walk(n->GetChildren());
                }
            }
            n = n->GetNext();
        }
    };
    walk(root->GetChildren());

    for (auto &r : recs) {
        for (const auto &ref : r.partRefs) {
            auto it = partGeoms.find(ref);
            if (it != partGeoms.end())
                for (const auto &g : it->second)
                    r.warn.geometries.push_back(g);
        }
        r.warn.computeCentroid();
        out.push_back(std::move(r.warn));
    }

    if (out.empty()) {
        err = _("No S-124 navigational warnings found in the document.\n\n"
                "The file must be a valid S-124 GML dataset containing\n"
                "NavwarnTypeGeneral features.");
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
        err = _("Failed to parse S-124 GML data.");
        return false;
    }
    return ParseDoc(doc, out, err);
}
