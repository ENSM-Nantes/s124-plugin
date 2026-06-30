#ifndef POINTS_LAYER_H
#define POINTS_LAYER_H

#include <wx/wx.h>
#include <vector>
#include <string>
#include "ocpn_plugin.h"

struct XmlPoint {
    double latitude;
    double longitude;
    wxString information;
};

class PointsLayer
{
public:
    PointsLayer();
    ~PointsLayer();

    bool LoadFromFile(const wxString &filePath);
    void Render(wxDC &dc, PlugIn_ViewPort *vp);
    void RenderGL(PlugIn_ViewPort *vp);

    // Returns index of point near screen coords, or -1
    int HitTest(int screenX, int screenY, PlugIn_ViewPort *vp);
    const XmlPoint &GetPoint(int index) const { return m_points[index]; }
    size_t GetPointCount() const { return m_points.size(); }
    void Clear() { m_points.clear(); }

    wxString GetLastError() const { return m_lastError; }

private:
    std::vector<XmlPoint> m_points;
    wxString m_lastError;

    // Convert lat/lon to screen pixels
    wxPoint LatLonToScreen(double lat, double lon, PlugIn_ViewPort *vp);
};

#endif // POINTS_LAYER_H
