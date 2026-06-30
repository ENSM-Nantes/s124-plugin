#include "XmlLoader.h"
#include <wx/xml/xml.h>
#include <wx/log.h>

bool XmlLoader::LoadPoints(const wxString &filePath,
                            std::vector<XmlPoint> &points,
                            wxString &errorMsg)
{
    wxXmlDocument doc;
    if (!doc.Load(filePath))
    {
        errorMsg = wxString::Format(
            _("Failed to parse XML file:\n%s\n\n"
              "Make sure the file is valid XML."),
            filePath);
        return false;
    }

    wxXmlNode *root = doc.GetRoot();
    if (!root)
    {
        errorMsg = _("XML file has no root element.");
        return false;
    }

    // Accept root names: <points>, <waypoints>, <locations>, or anything
    wxXmlNode *child = root->GetChildren();
    int parsed = 0;
    int errors = 0;

    while (child)
    {
        // Accept <point>, <waypoint>, <location>, <item>
        wxString nodeName = child->GetName().Lower();
        if (nodeName == wxT("point")    ||
            nodeName == wxT("waypoint") ||
            nodeName == wxT("location") ||
            nodeName == wxT("item"))
        {
            XmlPoint pt;
            bool hasLat = false, hasLon = false;
            double lat = 0.0, lon = 0.0;
            wxString info;

            // Try attributes first: <point lat="..." lon="..." info="..."/>
            wxString attrLat = child->GetAttribute(wxT("lat"),
                               child->GetAttribute(wxT("latitude"), wxEmptyString));
            wxString attrLon = child->GetAttribute(wxT("lon"),
                               child->GetAttribute(wxT("longitude"), wxEmptyString));
            wxString attrInfo = child->GetAttribute(wxT("info"),
                                child->GetAttribute(wxT("information"), wxEmptyString));

            if (!attrLat.IsEmpty() && attrLat.ToDouble(&lat)) hasLat = true;
            if (!attrLon.IsEmpty() && attrLon.ToDouble(&lon)) hasLon = true;
            if (!attrInfo.IsEmpty()) info = attrInfo;

            // Then scan child elements (take precedence over attributes)
            wxXmlNode *field = child->GetChildren();
            while (field)
            {
                wxString fn = field->GetName().Lower();
                wxString fv = field->GetNodeContent();

                if (fn == wxT("latitude") || fn == wxT("lat"))
                {
                    if (fv.ToDouble(&lat)) hasLat = true;
                }
                else if (fn == wxT("longitude") || fn == wxT("lon"))
                {
                    if (fv.ToDouble(&lon)) hasLon = true;
                }
                else if (fn == wxT("information") || fn == wxT("info") ||
                         fn == wxT("description")  || fn == wxT("name") ||
                         fn == wxT("comment")       || fn == wxT("text"))
                {
                    if (!fv.IsEmpty())
                        info = fv;
                }
                field = field->GetNext();
            }

            if (hasLat && hasLon)
            {
                pt.latitude    = lat;
                pt.longitude   = lon;
                pt.information = info.IsEmpty() ? wxString::Format(
                    wxT("Lat: %.6f\nLon: %.6f"), lat, lon) : info;
                points.push_back(pt);
                ++parsed;
            }
            else
            {
                ++errors;
            }
        }
        child = child->GetNext();
    }

    if (parsed == 0)
    {
        errorMsg = wxString::Format(
            _("No valid points found in file:\n%s\n\n"
              "Expected XML structure:\n"
              "<points>\n"
              "  <point>\n"
              "    <latitude>48.8566</latitude>\n"
              "    <longitude>2.3522</longitude>\n"
              "    <information>Paris</information>\n"
              "  </point>\n"
              "</points>\n\n"
              "(%d entries had missing lat/lon and were skipped.)"),
            filePath, errors);
        return false;
    }

    if (errors > 0)
    {
        // Partial success – not a fatal error
        wxLogWarning("XMLPoints: %d entries skipped due to missing lat/lon.", errors);
    }

    return true;
}
