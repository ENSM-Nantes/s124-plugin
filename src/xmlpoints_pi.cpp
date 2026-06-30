#include "xmlpoints_pi.h"
#include "InfoDialog.h"
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

// ------------------------------------------------------------
// Mandatory plugin factory function – called by OpenCPN loader
// ------------------------------------------------------------
extern "C" opencpn_plugin *create_pi(void *ppimgr)
{
    return new xmlpoints_pi(ppimgr);
}

extern "C" void destroy_pi(opencpn_plugin *p)
{
    delete p;
}

// ------------------------------------------------------------
// Constructor / Destructor
// ------------------------------------------------------------
xmlpoints_pi::xmlpoints_pi(void *ppimgr)
    : opencpn_plugin_116(ppimgr)
    , m_toolbar_item_id(-1)
    , m_layer(nullptr)
    , m_parent_window(nullptr)
{
    m_layer = new PointsLayer();
}

xmlpoints_pi::~xmlpoints_pi()
{
    delete m_layer;
}

// ------------------------------------------------------------
// Init – called once when the plugin is loaded
// ------------------------------------------------------------
int xmlpoints_pi::Init(void)
{
    LoadConfig();

    m_parent_window = GetOCPNCanvasWindow();

    // Create a simple 16×16 bitmap for the toolbar button
    wxBitmap bmp(16, 16);
    wxMemoryDC mdc(bmp);
    mdc.SetBackground(*wxBLUE_BRUSH);
    mdc.Clear();
    mdc.SetPen(*wxWHITE_PEN);
    mdc.SetBrush(*wxWHITE_BRUSH);
    mdc.DrawCircle(8, 8, 5);
    mdc.SelectObject(wxNullBitmap);

    m_toolbar_item_id = InsertPlugInTool(
        _("XML Points"), &bmp, &bmp, wxITEM_NORMAL,
        _("Load XML Points file"), _("Load XML Points file"),
        nullptr, -1, 0, this);

    // If a file was previously saved in config, load it
    if (!m_xmlFilePath.IsEmpty() && wxFileExists(m_xmlFilePath))
    {
        wxString err;
        if (!m_layer->LoadFromFile(m_xmlFilePath))
            wxMessageBox(m_layer->GetLastError(), _("XML Points"), wxOK | wxICON_ERROR);
    }

    return WANTS_OVERLAY_CALLBACK        |
           WANTS_OPENGL_OVERLAY_CALLBACK |
           WANTS_TOOLBAR_CALLBACK        |
           INSTALLS_TOOLBAR_TOOL         |
           WANTS_MOUSE_EVENTS            |
           WANTS_CONFIG;
}

bool xmlpoints_pi::DeInit(void)
{
    SaveConfig();
    if (m_toolbar_item_id >= 0)
        RemovePlugInTool(m_toolbar_item_id);
    return true;
}

wxString xmlpoints_pi::GetLongDescription()
{
    return _(
        "XML Points Plugin\n\n"
        "Reads an XML file containing latitude, longitude and information fields "
        "and plots each point on the chart. Click a point to see its information.\n\n"
        "XML format example:\n"
        "<points>\n"
        "  <point>\n"
        "    <latitude>48.8566</latitude>\n"
        "    <longitude>2.3522</longitude>\n"
        "    <information>Paris, France</information>\n"
        "  </point>\n"
        "</points>"
    );
}

// ------------------------------------------------------------
// Toolbar button – open file dialog
// ------------------------------------------------------------
void xmlpoints_pi::OnToolbarToolCallback(int id)
{
    wxFileDialog dlg(
        m_parent_window,
        _("Select XML Points file"),
        wxEmptyString,
        wxEmptyString,
        _("XML files (*.xml)|*.xml|All files (*.*)|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() != wxID_OK)
        return;

    m_xmlFilePath = dlg.GetPath();
    m_layer->Clear();

    if (!m_layer->LoadFromFile(m_xmlFilePath))
    {
        wxMessageBox(m_layer->GetLastError(),
                     _("XML Points – Load Error"),
                     wxOK | wxICON_ERROR, m_parent_window);
    }
    else
    {
        size_t n = m_layer->GetPointCount();
        wxString coords;
        if (n == 1) {
            const XmlPoint &p = m_layer->GetPoint(0);
            coords = wxString::Format(_("\n\nPoint at: %.4f°, %.4f°"), p.latitude, p.longitude);
        }
        wxString msg = wxString::Format(
            _("Loaded %zu point(s) from:\n%s%s"),
            n, m_xmlFilePath, coords);
        wxMessageBox(msg, _("XML Points"), wxOK | wxICON_INFORMATION, m_parent_window);
    }

    SaveConfig();
    RequestRefresh(m_parent_window);
}

// ------------------------------------------------------------
// Rendering
// ------------------------------------------------------------
bool xmlpoints_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    m_layer->Render(dc, vp);
    return true;
}

bool xmlpoints_pi::RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp, int /*canvasIndex*/)
{
    return RenderOverlay(dc, vp);
}

bool xmlpoints_pi::RenderGLOverlay(wxGLContext * /*pcontext*/, PlugIn_ViewPort *vp)
{
    m_layer->RenderGL(vp);
    return true;
}

bool xmlpoints_pi::RenderGLOverlayMultiCanvas(wxGLContext *pcontext, PlugIn_ViewPort *vp, int /*canvasIndex*/)
{
    return RenderGLOverlay(pcontext, vp);
}

// ------------------------------------------------------------
// Mouse – hit-test on click
// ------------------------------------------------------------
bool xmlpoints_pi::MouseEventHook(wxMouseEvent &event)
{
    if (!event.LeftDown())
        return false;

    // We need the current viewport; OpenCPN provides it during rendering.
    // Store a pointer during last render call (see PointsLayer).
    // For now we use the stored vp from PointsLayer.
    // The hit-test is performed against cached screen positions.

    // Ask the layer to test against the last rendered positions
    int idx = m_layer->HitTest(event.GetX(), event.GetY(), nullptr);
    if (idx < 0)
        return false;

    const XmlPoint &pt = m_layer->GetPoint(idx);
    InfoDialog dlg(m_parent_window,
                   _("Point Information"),
                   pt.information,
                   pt.latitude,
                   pt.longitude);
    dlg.ShowModal();
    return true; // consumed
}

// ------------------------------------------------------------
// Config persistence
// ------------------------------------------------------------
void xmlpoints_pi::LoadConfig()
{
    wxFileConfig *cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath(_("/Plugins/XMLPoints"));
    cfg->Read(_("XMLFilePath"), &m_xmlFilePath, _(""));
}

void xmlpoints_pi::SaveConfig()
{
    wxFileConfig *cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath(_("/Plugins/XMLPoints"));
    cfg->Write(_("XMLFilePath"), m_xmlFilePath);
    cfg->Flush();
}
