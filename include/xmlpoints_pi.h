#ifndef XMLPOINTS_PI_H
#define XMLPOINTS_PI_H

#include <wx/wx.h>
#include <wx/fileconf.h>
#include "ocpn_plugin.h"
#include "PointsLayer.h"

#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0
#define MY_API_VERSION_MAJOR 1
#define MY_API_VERSION_MINOR 16

class xmlpoints_pi : public opencpn_plugin_116
{
public:
    xmlpoints_pi(void *ppimgr);
    ~xmlpoints_pi();

    // Required OpenCPN plugin methods
    int Init(void) override;
    bool DeInit(void) override;

    int GetAPIVersionMajor() override { return MY_API_VERSION_MAJOR; }
    int GetAPIVersionMinor() override { return MY_API_VERSION_MINOR; }
    int GetPlugInVersionMajor() override { return PLUGIN_VERSION_MAJOR; }
    int GetPlugInVersionMinor() override { return PLUGIN_VERSION_MINOR; }

    wxString GetCommonName() override { return _("XML Points"); }
    wxString GetShortDescription() override { return _("Plot points from XML file"); }
    wxString GetLongDescription() override;

    // Rendering – DC path (non-GL mode)
    bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp) override;
    bool RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp, int canvasIndex) override;
    // Rendering – GL path (GL mode, used when OpenGL is active)
    bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp) override;
    bool RenderGLOverlayMultiCanvas(wxGLContext *pcontext, PlugIn_ViewPort *vp, int canvasIndex) override;

    // Mouse events
    bool MouseEventHook(wxMouseEvent &event) override;

    // Toolbar
    int GetToolbarToolCount(void) override { return 1; }
    void OnToolbarToolCallback(int id) override;
    void SetColorScheme(PI_ColorScheme cs) override {}

    // Config
    void LoadConfig();
    void SaveConfig();

    wxString m_xmlFilePath;

private:
    int m_toolbar_item_id;
    PointsLayer *m_layer;
    wxWindow *m_parent_window;
};

#endif // XMLPOINTS_PI_H
