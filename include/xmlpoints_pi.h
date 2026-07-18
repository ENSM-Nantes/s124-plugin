#ifndef XMLPOINTS_PI_H
#define XMLPOINTS_PI_H

#include <wx/wx.h>
#include <wx/fileconf.h>
#include <vector>
#include "ocpn_plugin.h"
#include "PointsLayer.h"
#include "SecomClient.h"

#define PLUGIN_VERSION_MAJOR 4
#define PLUGIN_VERSION_MINOR 0
#define MY_API_VERSION_MAJOR 1
#define MY_API_VERSION_MINOR 16

class xmlpoints_pi : public opencpn_plugin_116
{
public:
    xmlpoints_pi(void *ppimgr);
    ~xmlpoints_pi();

    int  Init(void) override;
    bool DeInit(void) override;
    wxBitmap *GetPlugInBitmap() override;

    int GetAPIVersionMajor() override { return MY_API_VERSION_MAJOR; }
    int GetAPIVersionMinor() override { return MY_API_VERSION_MINOR; }
    int GetPlugInVersionMajor() override { return PLUGIN_VERSION_MAJOR; }
    int GetPlugInVersionMinor() override { return PLUGIN_VERSION_MINOR; }

    wxString GetCommonName() override { return _("S-124 Warnings"); }
    wxString GetShortDescription() override { return _("Display S-124 navigational warnings"); }
    wxString GetLongDescription() override;

    bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp) override;
    bool RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp, int canvasIndex) override;
    bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp) override;
    bool RenderGLOverlayMultiCanvas(wxGLContext *pcontext, PlugIn_ViewPort *vp, int canvasIndex) override;

    bool MouseEventHook(wxMouseEvent &event) override;

    int  GetToolbarToolCount(void) override { return 1; }
    void OnToolbarToolCallback(int id) override;
    void SetColorScheme(PI_ColorScheme cs) override {}

    void LoadConfig();
    void SaveConfig();

    // Invoked by the auto-refresh timer. Public so the wxTimer subclass that
    // owns the actual timer (defined in xmlpoints_pi.cpp, outside the class)
    // can call it.
    void OnAutoRefreshTimer();

private:
    void OnOpenLocalFile();
    void OnOpenFolder();
    void OnOpenSecomDialog();
    void OnRefreshSecom();
    bool FetchAllSecom(std::vector<S124Warning> &combined, wxArrayString &errors);
    void UpdateAutoRefreshTimer();

    int          m_toolbar_item_id;
    PointsLayer *m_layer;
    wxWindow    *m_parent_window;
    wxBitmap     m_pluginBitmap;

    wxString    m_lastFilePath;

    std::vector<SecomConfig> m_secomConnections;
    bool     m_autoRefreshEnabled;
    int      m_autoRefreshMinutes;
    wxTimer *m_refreshTimer;
};

#endif // XMLPOINTS_PI_H
