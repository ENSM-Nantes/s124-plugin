#ifndef POINTS_LAYER_H
#define POINTS_LAYER_H

#include <wx/wx.h>
#include <vector>
#include "ocpn_plugin.h"
#include "S124Warning.h"

// One color per warning-severity group. The in-force-bulletin variant of
// each type (warningType 6-9) shares its base type's color (1-4) rather
// than getting its own, so there are only five colors to configure even
// though warningType has nine meaningful values.
struct WarningColors {
    wxColour local   = wxColour(200, 180,   0); // types 1, 6
    wxColour coastal = wxColour(230, 130,   0); // types 2, 7
    wxColour subArea = wxColour(220,  80,  20); // types 3, 8
    wxColour navarea = wxColour(200,  30,  30); // types 4, 9
    wxColour other   = wxColour(220,  50,  50); // any other/unrecognized type
};

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

    void SetWarningColors(const WarningColors &colors) { m_warningColors = colors; }
    const WarningColors &GetWarningColors() const { return m_warningColors; }

    wxString GetLastError() const { return m_lastError; }

private:
    std::vector<S124Warning> m_warnings;
    wxString m_lastError;
    WarningColors m_warningColors;

    wxPoint LatLonToScreen(double lat, double lon, PlugIn_ViewPort *vp);
    wxColour WarningColor(int warningType) const;
};

#endif // POINTS_LAYER_H
