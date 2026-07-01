#include "xmlpoints_pi.h"
#include "InfoDialog.h"
#include "SecomDialog.h"
#include "SecomClient.h"
#include "S124Parser.h"
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/dir.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/menu.h>

// Menu IDs
enum {
    ID_OPEN_FILE   = wxID_HIGHEST + 1,
    ID_OPEN_FOLDER,
    ID_SECOM_CFG,
    ID_SECOM_REFRESH,
    ID_CLEAR
};

extern "C" opencpn_plugin *create_pi(void *ppimgr) { return new xmlpoints_pi(ppimgr); }
extern "C" void destroy_pi(opencpn_plugin *p) { delete p; }

// ── constructor / destructor ──────────────────────────────────────────────────

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

// ── Init / DeInit ─────────────────────────────────────────────────────────────

int xmlpoints_pi::Init(void)
{
    LoadConfig();
    m_parent_window = GetOCPNCanvasWindow();

    wxBitmap bmp(16, 16);
    wxMemoryDC mdc(bmp);
    mdc.SetBackground(wxBrush(wxColour(220, 50, 50)));
    mdc.Clear();
    mdc.SetPen(*wxWHITE_PEN);
    mdc.SetBrush(*wxWHITE_BRUSH);
    mdc.DrawCircle(8, 8, 5);
    mdc.SelectObject(wxNullBitmap);

    m_toolbar_item_id = InsertPlugInTool(
        _("S-124 Warnings"), &bmp, &bmp, wxITEM_NORMAL,
        _("S-124 Navigational Warnings"), _("S-124 Navigational Warnings"),
        nullptr, -1, 0, this);

    // Reload last file if it still exists
    if (!m_lastFilePath.IsEmpty() && wxFileExists(m_lastFilePath)) {
        if (!m_layer->LoadFromFile(m_lastFilePath))
            wxLogWarning("S-124: %s", m_layer->GetLastError());
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
        "S-124 Navigational Warnings Plugin\n\n"
        "Displays IHO S-124 navigational warnings on the chart.\n"
        "Supports loading local S-124 GML files and fetching warnings\n"
        "from a SECOM API endpoint.\n\n"
        "Click the toolbar button to open a local file or connect to SECOM.\n"
        "Click any warning marker or area on the chart to read the warning text."
    );
}

// ── toolbar callback → popup menu ─────────────────────────────────────────────

void xmlpoints_pi::OnToolbarToolCallback(int /*id*/)
{
    wxMenu menu;
    menu.Append(ID_OPEN_FILE,   _("Open local S-124 file…"));
    menu.Append(ID_OPEN_FOLDER, _("Open S-124 folder…"));
    menu.Append(ID_SECOM_CFG,   _("Connect to SECOM…"));
    menu.AppendSeparator();
    wxMenuItem *refreshItem =
        menu.Append(ID_SECOM_REFRESH, _("Refresh from SECOM"));
    refreshItem->Enable(!m_secomCfg.baseUrl.IsEmpty());
    menu.AppendSeparator();
    menu.Append(ID_CLEAR, _("Clear all warnings"));

    menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { OnOpenLocalFile();    }, ID_OPEN_FILE);
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { OnOpenFolder();       }, ID_OPEN_FOLDER);
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { OnOpenSecomDialog();  }, ID_SECOM_CFG);
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { OnRefreshSecom();     }, ID_SECOM_REFRESH);
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) {
        m_layer->Clear();
        RequestRefresh(m_parent_window);
    }, ID_CLEAR);

    m_parent_window->PopupMenu(&menu);
}

// ── source handlers ───────────────────────────────────────────────────────────

void xmlpoints_pi::OnOpenLocalFile()
{
    wxFileDialog dlg(
        m_parent_window,
        _("Select S-124 GML file"),
        wxEmptyString, wxEmptyString,
        _("S-124 GML files (*.gml;*.xml)|*.gml;*.xml|All files (*.*)|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() != wxID_OK) return;

    m_lastFilePath = dlg.GetPath();
    m_layer->Clear();

    if (!m_layer->LoadFromFile(m_lastFilePath)) {
        wxMessageBox(m_layer->GetLastError(),
                     _("S-124 – Load Error"),
                     wxOK | wxICON_ERROR, m_parent_window);
    } else {
        size_t n = m_layer->GetWarningCount();
        wxMessageBox(
            wxString::Format(_("Loaded %zu navigational warning(s) from:\n%s"),
                             n, m_lastFilePath),
            _("S-124 Warnings"), wxOK | wxICON_INFORMATION, m_parent_window);
    }

    SaveConfig();
    RequestRefresh(m_parent_window);
}

void xmlpoints_pi::OnOpenFolder()
{
    wxDirDialog dlg(
        m_parent_window,
        _("Select folder with S-124 GML files"),
        wxEmptyString,
        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

    if (dlg.ShowModal() != wxID_OK) return;

    wxString folder = dlg.GetPath();
    wxDir dir(folder);
    if (!dir.IsOpened()) {
        wxMessageBox(_("Could not open the selected folder."),
                     _("S-124 – Error"), wxOK | wxICON_ERROR, m_parent_window);
        return;
    }

    std::vector<S124Warning> allWarnings;
    int fileCount = 0;
    wxArrayString errors;

    for (const wxString &pattern : { wxString("*.gml"), wxString("*.xml") }) {
        wxString filename;
        bool found = dir.GetFirst(&filename, pattern, wxDIR_FILES);
        while (found) {
            wxString path = folder + wxFILE_SEP_PATH + filename;
            std::vector<S124Warning> fileWarnings;
            wxString err;
            if (S124Parser::ParseFile(path, fileWarnings, err)) {
                allWarnings.insert(allWarnings.end(), fileWarnings.begin(), fileWarnings.end());
                fileCount++;
            } else {
                errors.Add(filename + ": " + err);
            }
            found = dir.GetNext(&filename);
        }
    }

    m_layer->SetWarnings(allWarnings);
    RequestRefresh(m_parent_window);

    wxString msg = wxString::Format(
        _("Loaded %zu navigational warning(s) from %d file(s) in:\n%s"),
        allWarnings.size(), fileCount, folder);
    if (!errors.IsEmpty())
        msg += _("\n\nErrors:\n") + wxJoin(errors, '\n');

    wxMessageBox(msg, _("S-124 Warnings"), wxOK | wxICON_INFORMATION, m_parent_window);
}

void xmlpoints_pi::OnOpenSecomDialog()
{
    SecomDialog dlg(m_parent_window, m_secomCfg);
    if (dlg.ShowModal() != wxID_OK) return;

    m_secomCfg = dlg.GetConfig();
    SaveConfig();

    if (!m_secomCfg.baseUrl.IsEmpty())
        OnRefreshSecom();
}

void xmlpoints_pi::OnRefreshSecom()
{
    if (m_secomCfg.baseUrl.IsEmpty()) {
        wxMessageBox(_("No SECOM endpoint configured.\n"
                       "Use 'Connect to SECOM…' to set the URL."),
                     _("S-124"), wxOK | wxICON_INFORMATION, m_parent_window);
        return;
    }

    wxProgressDialog progress(_("S-124 – Fetching from SECOM"),
                              _("Connecting to SECOM endpoint…"),
                              100, m_parent_window,
                              wxPD_APP_MODAL | wxPD_AUTO_HIDE);
    progress.Pulse();

    std::vector<S124Warning> warnings;
    wxString err;
    SecomClient client(m_secomCfg);

    if (!client.Fetch(warnings, err)) {
        wxMessageBox(wxString::Format(_("SECOM fetch failed:\n%s"), err),
                     _("S-124 – SECOM Error"),
                     wxOK | wxICON_ERROR, m_parent_window);
        return;
    }

    m_layer->SetWarnings(warnings);
    RequestRefresh(m_parent_window);

    wxMessageBox(
        wxString::Format(_("Received %zu navigational warning(s) from SECOM."),
                         warnings.size()),
        _("S-124 Warnings"), wxOK | wxICON_INFORMATION, m_parent_window);
}

// ── rendering ─────────────────────────────────────────────────────────────────

bool xmlpoints_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    m_layer->Render(dc, vp);
    return true;
}

bool xmlpoints_pi::RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp, int)
{
    return RenderOverlay(dc, vp);
}

bool xmlpoints_pi::RenderGLOverlay(wxGLContext *, PlugIn_ViewPort *vp)
{
    m_layer->RenderGL(vp);
    return true;
}

bool xmlpoints_pi::RenderGLOverlayMultiCanvas(wxGLContext *ctx, PlugIn_ViewPort *vp, int)
{
    return RenderGLOverlay(ctx, vp);
}

// ── mouse ─────────────────────────────────────────────────────────────────────

bool xmlpoints_pi::MouseEventHook(wxMouseEvent &event)
{
    if (!event.LeftDown()) return false;

    int idx = m_layer->HitTest(event.GetX(), event.GetY(), nullptr);
    if (idx < 0) return false;

    InfoDialog dlg(m_parent_window, m_layer->GetWarning(idx));
    dlg.ShowModal();
    return true;
}

// ── config ────────────────────────────────────────────────────────────────────

void xmlpoints_pi::LoadConfig()
{
    wxFileConfig *cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath(_("/Plugins/S124Warnings"));
    cfg->Read(_("LastFilePath"),    &m_lastFilePath,          wxEmptyString);
    cfg->Read(_("SecomUrl"),        &m_secomCfg.baseUrl,      wxEmptyString);
    cfg->Read(_("SecomDataRef"),    &m_secomCfg.dataReference, wxEmptyString);
    cfg->Read(_("SecomApiKey"),     &m_secomCfg.apiKey,       wxEmptyString);
    cfg->Read(_("SecomCertFile"),   &m_secomCfg.certFile,     wxEmptyString);
    cfg->Read(_("SecomKeyFile"),    &m_secomCfg.keyFile,      wxEmptyString);
    cfg->Read(_("SecomCaBundle"),   &m_secomCfg.caBundle,     wxEmptyString);
    int verify = 1, timeout = 30;
    cfg->Read(_("SecomSslVerify"), &verify,  1);
    cfg->Read(_("SecomTimeout"),   &timeout, 30);
    m_secomCfg.sslVerify  = (verify != 0);
    m_secomCfg.timeoutSec = timeout;
}

void xmlpoints_pi::SaveConfig()
{
    wxFileConfig *cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath(_("/Plugins/S124Warnings"));
    cfg->Write(_("LastFilePath"),   m_lastFilePath);
    cfg->Write(_("SecomUrl"),       m_secomCfg.baseUrl);
    cfg->Write(_("SecomDataRef"),   m_secomCfg.dataReference);
    cfg->Write(_("SecomApiKey"),    m_secomCfg.apiKey);
    cfg->Write(_("SecomCertFile"),  m_secomCfg.certFile);
    cfg->Write(_("SecomKeyFile"),   m_secomCfg.keyFile);
    cfg->Write(_("SecomCaBundle"),  m_secomCfg.caBundle);
    cfg->Write(_("SecomSslVerify"), (int)m_secomCfg.sslVerify);
    cfg->Write(_("SecomTimeout"),   m_secomCfg.timeoutSec);
    cfg->Flush();
}
