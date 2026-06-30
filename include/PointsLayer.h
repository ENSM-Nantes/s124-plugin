#ifndef POINTS_LAYER_H
#define POINTS_LAYER_H

#include <wx/wx.h>
#include <vector>
#include "ocpn_plugin.h"
#include "S124Warning.h"

class PointsLayer
{
public:
    PointsLayer();
    ~PointsLayer();

    bool LoadFromFile(const wxString &filePath);
    void SetWarnings(const std::vector<S124Warning> &warnings);

    void Render(wxDC &dc, PlugIn_ViewPort *vp);
    void RenderGL(PlugIn_ViewPort *vp);

    // Returns warning index at screen position, or -1
    int HitTest(int screenX, int screenY, PlugIn_ViewPort *vp);
    const S124Warning &GetWarning(int index) const { return m_warnings[index]; }
    size_t GetWarningCount() const { return m_warnings.size(); }
    void Clear() { m_warnings.clear(); }

    wxString GetLastError() const { return m_lastError; }

private:
    std::vector<S124Warning> m_warnings;
    wxString m_lastError;

    wxPoint LatLonToScreen(double lat, double lon, PlugIn_ViewPort *vp);
    wxColour WarningColor(int warningType) const;
};

#endif // POINTS_LAYER_H
