#include "xmlpoints_pi.h"
#include "Utf8Text.h"
#include "InfoDialog.h"
#include "SecomListDialog.h"
#include "SecomClient.h"
#include "S124Parser.h"
#include "ToolbarMenuDialog.h"
#include "WarningColorDialog.h"
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/dir.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/region.h>
#include <cstring>

extern "C" DECL_EXP opencpn_plugin *create_pi(void *ppimgr) { return new xmlpoints_pi(ppimgr); }
extern "C" DECL_EXP void destroy_pi(opencpn_plugin *p) { delete p; }

namespace {
// Routes wxTimer notifications to the plugin. A plain member wxTimer can't do
// this itself because xmlpoints_pi (an opencpn_plugin_116) is not a
// wxEvtHandler, so there is no window to bind wxEVT_TIMER to.
class SecomRefreshTimer : public wxTimer
{
public:
    explicit SecomRefreshTimer(xmlpoints_pi *owner) : m_owner(owner) {}
    void Notify() override { m_owner->OnAutoRefreshTimer(); }

private:
    xmlpoints_pi *m_owner;
};

wxString ColorToString(const wxColour &c)
{
    return c.GetAsString(wxC2S_HTML_SYNTAX);
}

wxColour ColorFromConfig(wxFileConfig *cfg, const wxString &key, const wxColour &fallback)
{
    wxString s;
    if (cfg->Read(key, &s, wxEmptyString) && !s.IsEmpty()) {
        wxColour c;
        if (c.Set(s)) return c;
    }
    return fallback;
}
} // namespace

// ── icon ───────────────────────────────────────────────────────────────────────
// Warning-triangle icon: black-outlined amber triangle with an exclamation
// mark. Built by testing exact polygon/rect membership per pixel (no
// anti-aliased wxDC drawing), so there is no blended-colour fringe left
// over at the edges — pixels are either fully opaque foreground or fully
// transparent background. Used for both the toolbar tool and the plugin
// list entry in Options, so the two stay visually identical.
static wxBitmap MakeWarningTriangleBitmap()
{
    const int iconSize = 32;

    const wxPoint outerTri[3] = {
        wxPoint(16, 2), wxPoint(30, 28), wxPoint(2, 28)
    };
    const wxPoint innerTri[3] = {
        wxPoint(16, 5), wxPoint(28, 27), wxPoint(4, 27)
    };
    wxRegion outerRegion(3, outerTri);
    wxRegion borderRegion(outerRegion);
    borderRegion.Subtract(wxRegion(3, innerTri));

    wxRegion exclRegion(wxRect(14, 12, 4, 10));
    exclRegion.Union(wxRect(14, 24, 4, 4));

    wxImage img(iconSize, iconSize);
    img.InitAlpha();
    memset(img.GetAlpha(), 0, iconSize * iconSize);

    for (int y = 0; y < iconSize; ++y) {
        for (int x = 0; x < iconSize; ++x) {
            if (outerRegion.Contains(x, y) == wxOutRegion)
                continue;

            const bool isBlack = borderRegion.Contains(x, y) != wxOutRegion ||
                                  exclRegion.Contains(x, y) != wxOutRegion;
            if (isBlack)
                img.SetRGB(x, y, 0, 0, 0);
            else
                img.SetRGB(x, y, 255, 193, 7);
            img.SetAlpha(x, y, 255);
        }
    }

    return wxBitmap(img);
}

// ── constructor / destructor ──────────────────────────────────────────────────

xmlpoints_pi::xmlpoints_pi(void *ppimgr)
    : opencpn_plugin_116(ppimgr)
    , m_toolbar_item_id(-1)
    , m_layer(nullptr)
    , m_parent_window(nullptr)
    , m_pluginBitmap(MakeWarningTriangleBitmap())
    , m_autoRefreshEnabled(false)
    , m_autoRefreshMinutes(15)
    , m_refreshTimer(nullptr)
{
    m_layer = new PointsLayer();
}

xmlpoints_pi::~xmlpoints_pi()
{
    delete m_refreshTimer;
    delete m_layer;
}

// ── Init / DeInit ─────────────────────────────────────────────────────────────

int xmlpoints_pi::Init(void)
{
    LoadConfig();
    m_parent_window = GetOCPNCanvasWindow();

    m_toolbar_item_id = InsertPlugInTool(
        _("S-124 Warnings"), &m_pluginBitmap, &m_pluginBitmap, wxITEM_NORMAL,
        _("S-124 Navigational Warnings"), _("S-124 Navigational Warnings"),
        nullptr, -1, 0, this);

    // Reload last file if it still exists
    if (!m_lastFilePath.IsEmpty() && wxFileExists(m_lastFilePath)) {
        if (!m_layer->LoadFromFile(m_lastFilePath))
            wxLogWarning("S-124: %s", m_layer->GetLastError());
    }

    UpdateAutoRefreshTimer();

    return WANTS_OVERLAY_CALLBACK        |
           WANTS_OPENGL_OVERLAY_CALLBACK |
           WANTS_TOOLBAR_CALLBACK        |
           INSTALLS_TOOLBAR_TOOL         |
           WANTS_MOUSE_EVENTS            |
           WANTS_CONFIG;
}

wxBitmap *xmlpoints_pi::GetPlugInBitmap()
{
    return &m_pluginBitmap;
}

bool xmlpoints_pi::DeInit(void)
{
    if (m_refreshTimer)
        m_refreshTimer->Stop();
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

// ── toolbar callback → action dialog ──────────────────────────────────────────

void xmlpoints_pi::OnToolbarToolCallback(int /*id*/)
{
    ToolbarMenuDialog dlg(m_parent_window, !m_secomConnections.empty());
    if (dlg.ShowModal() != wxID_OK) return;

    switch (dlg.GetAction()) {
        case ToolbarMenuDialog::ACTION_OPEN_FILE:     OnOpenLocalFile();   break;
        case ToolbarMenuDialog::ACTION_OPEN_FOLDER:   OnOpenFolder();      break;
        case ToolbarMenuDialog::ACTION_SECOM_CFG:     OnOpenSecomDialog(); break;
        case ToolbarMenuDialog::ACTION_SECOM_REFRESH: OnRefreshSecom();    break;
        case ToolbarMenuDialog::ACTION_WARNING_COLORS: OnOpenWarningColorDialog(); break;
        case ToolbarMenuDialog::ACTION_CLEAR:
            m_layer->Clear();
            RequestRefresh(m_parent_window);
            break;
        default: break;
    }
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
                     _U("S-124 – Load Error"),
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
                     _U("S-124 – Error"), wxOK | wxICON_ERROR, m_parent_window);
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
    SecomListDialog dlg(m_parent_window, m_secomConnections,
                        m_autoRefreshEnabled, m_autoRefreshMinutes);
    if (dlg.ShowModal() != wxID_OK) return;

    m_secomConnections   = dlg.GetConnections();
    m_autoRefreshEnabled = dlg.GetAutoRefreshEnabled();
    m_autoRefreshMinutes = dlg.GetAutoRefreshMinutes();
    SaveConfig();
    UpdateAutoRefreshTimer();

    if (!m_secomConnections.empty())
        OnRefreshSecom();
}

void xmlpoints_pi::OnOpenWarningColorDialog()
{
    WarningColorDialog dlg(m_parent_window, m_layer->GetWarningColors());
    if (dlg.ShowModal() != wxID_OK) return;

    m_layer->SetWarningColors(dlg.GetColors());
    SaveConfig();
    RequestRefresh(m_parent_window);
}

// Fetches every configured connection and concatenates the results. Returns
// false (with 'errors' populated) if any connection failed; connections that
// did succeed are still included in 'combined'.
bool xmlpoints_pi::FetchAllSecom(std::vector<S124Warning> &combined, wxArrayString &errors)
{
    for (const auto &cfg : m_secomConnections) {
        std::vector<S124Warning> warnings;
        wxString err;
        SecomClient client(cfg);

        if (client.Fetch(warnings, err)) {
            combined.insert(combined.end(), warnings.begin(), warnings.end());
        } else {
            wxString label = cfg.name.IsEmpty() ? cfg.baseUrl : cfg.name;
            errors.Add(label + _U(": ") + err);
        }
    }
    return errors.IsEmpty();
}

void xmlpoints_pi::OnRefreshSecom()
{
    if (m_secomConnections.empty()) {
        wxMessageBox(_U("No SECOM connections configured.\n"
                        "Use 'SECOM connections…' to add one."),
                     _("S-124"), wxOK | wxICON_INFORMATION, m_parent_window);
        return;
    }

    wxProgressDialog progress(_U("S-124 – Fetching from SECOM"),
                              _U("Connecting to SECOM endpoint(s)…"),
                              100, m_parent_window,
                              wxPD_APP_MODAL | wxPD_AUTO_HIDE);
    progress.Pulse();

    std::vector<S124Warning> combined;
    wxArrayString errors;
    FetchAllSecom(combined, errors);

    m_layer->SetWarnings(combined);
    RequestRefresh(m_parent_window);

    wxString msg = wxString::Format(
        _("Received %zu navigational warning(s) from %zu SECOM connection(s)."),
        combined.size(), m_secomConnections.size());
    if (!errors.IsEmpty())
        msg += _("\n\nErrors:\n") + wxJoin(errors, '\n');

    wxMessageBox(msg, _U("S-124 – SECOM"),
                 wxOK | (errors.IsEmpty() ? wxICON_INFORMATION : wxICON_WARNING),
                 m_parent_window);
}

// Timer-driven refresh: same fetch as the manual 'Refresh from SECOM' button,
// but silent (no modal progress dialog or success popup) so it doesn't
// interrupt the user every time the interval elapses. Failures are logged
// rather than shown, for the same reason.
void xmlpoints_pi::OnAutoRefreshTimer()
{
    if (m_secomConnections.empty()) return;

    std::vector<S124Warning> combined;
    wxArrayString errors;
    FetchAllSecom(combined, errors);

    m_layer->SetWarnings(combined);
    RequestRefresh(m_parent_window);

    if (!errors.IsEmpty())
        wxLogWarning("S-124 SECOM auto-refresh: %s", wxJoin(errors, ';'));
}

void xmlpoints_pi::UpdateAutoRefreshTimer()
{
    if (m_refreshTimer) {
        m_refreshTimer->Stop();
        delete m_refreshTimer;
        m_refreshTimer = nullptr;
    }

    if (m_autoRefreshEnabled && m_autoRefreshMinutes > 0 && !m_secomConnections.empty()) {
        m_refreshTimer = new SecomRefreshTimer(this);
        m_refreshTimer->Start(m_autoRefreshMinutes * 60 * 1000, wxTIMER_CONTINUOUS);
    }
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

    // Don't pop the modal dialog synchronously from inside the mouse-down
    // hook: the chart canvas hasn't seen the matching mouse-up yet, so it
    // still believes the button is held down. Blocking the event loop here
    // with ShowModal() swallows that mouse-up (it goes to the dialog
    // instead), leaving the canvas stuck thinking it's panning once the
    // dialog closes. Deferring the dialog until after the current event
    // (and the mouse-up right behind it) has been dispatched lets the
    // canvas finish its click/drag bookkeeping first.
    wxWindow *parent = m_parent_window;
    S124Warning warning = m_layer->GetWarning(idx);
    parent->CallAfter([parent, warning]() {
        InfoDialog dlg(parent, warning);
        dlg.ShowModal();

        // The click that dismisses the dialog (e.g. "Close") happens
        // entirely inside the modal dialog, so its LeftUp never reaches
        // the chart canvas. Once the dialog is destroyed and the canvas
        // regains input focus, it can be left believing the left button
        // is still down and start panning on the very next mouse move.
        // Explicitly release any stale capture and feed the canvas a
        // LeftUp so it ends whatever click/drag it thinks is in progress.
        if (parent->HasCapture())
            parent->ReleaseMouse();

        wxMouseEvent up(wxEVT_LEFT_UP);
        up.SetPosition(parent->ScreenToClient(wxGetMousePosition()));
        parent->GetEventHandler()->ProcessEvent(up);
    });
    return true;
}

// ── config ────────────────────────────────────────────────────────────────────

void xmlpoints_pi::LoadConfig()
{
    wxFileConfig *cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath(_("/Plugins/S124Warnings"));
    cfg->Read(_("LastFilePath"), &m_lastFilePath, wxEmptyString);

    m_secomConnections.clear();

    int count = 0;
    if (cfg->Read(_("SecomCount"), &count, 0) && count > 0) {
        for (int i = 0; i < count; ++i) {
            wxString prefix = wxString::Format(_("Secom%d"), i);
            SecomConfig sc;
            cfg->Read(prefix + _("Name"),      &sc.name,          wxEmptyString);
            cfg->Read(prefix + _("Url"),       &sc.baseUrl,       wxEmptyString);
            cfg->Read(prefix + _("DataRef"),   &sc.dataReference, wxEmptyString);
            cfg->Read(prefix + _("ApiKey"),    &sc.apiKey,        wxEmptyString);
            cfg->Read(prefix + _("CertFile"),  &sc.certFile,      wxEmptyString);
            cfg->Read(prefix + _("KeyFile"),   &sc.keyFile,       wxEmptyString);
            cfg->Read(prefix + _("CaBundle"),  &sc.caBundle,      wxEmptyString);
            int verify = 1, timeout = 30;
            cfg->Read(prefix + _("SslVerify"), &verify,  1);
            cfg->Read(prefix + _("Timeout"),   &timeout, 30);
            sc.sslVerify  = (verify != 0);
            sc.timeoutSec = timeout;
            m_secomConnections.push_back(sc);
        }
    } else {
        // Migrate the pre-multi-connection single-endpoint config, if present.
        wxString url;
        cfg->Read(_("SecomUrl"), &url, wxEmptyString);
        if (!url.IsEmpty()) {
            SecomConfig sc;
            sc.baseUrl = url;
            cfg->Read(_("SecomDataRef"),  &sc.dataReference, wxEmptyString);
            cfg->Read(_("SecomApiKey"),   &sc.apiKey,        wxEmptyString);
            cfg->Read(_("SecomCertFile"), &sc.certFile,      wxEmptyString);
            cfg->Read(_("SecomKeyFile"),  &sc.keyFile,       wxEmptyString);
            cfg->Read(_("SecomCaBundle"), &sc.caBundle,      wxEmptyString);
            int verify = 1, timeout = 30;
            cfg->Read(_("SecomSslVerify"), &verify,  1);
            cfg->Read(_("SecomTimeout"),   &timeout, 30);
            sc.sslVerify  = (verify != 0);
            sc.timeoutSec = timeout;
            m_secomConnections.push_back(sc);
        }
    }

    int autoRefresh = 0;
    cfg->Read(_("SecomAutoRefresh"),       &autoRefresh,          0);
    cfg->Read(_("SecomAutoRefreshMinutes"), &m_autoRefreshMinutes, 15);
    m_autoRefreshEnabled = (autoRefresh != 0);
    if (m_autoRefreshMinutes <= 0) m_autoRefreshMinutes = 15;

    WarningColors defaults;
    WarningColors colors;
    colors.local   = ColorFromConfig(cfg, _("WarningColorLocal"),   defaults.local);
    colors.coastal = ColorFromConfig(cfg, _("WarningColorCoastal"), defaults.coastal);
    colors.subArea = ColorFromConfig(cfg, _("WarningColorSubArea"), defaults.subArea);
    colors.navarea = ColorFromConfig(cfg, _("WarningColorNavarea"), defaults.navarea);
    colors.other   = ColorFromConfig(cfg, _("WarningColorOther"),   defaults.other);
    m_layer->SetWarningColors(colors);
}

void xmlpoints_pi::SaveConfig()
{
    wxFileConfig *cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath(_("/Plugins/S124Warnings"));
    cfg->Write(_("LastFilePath"), m_lastFilePath);

    // Clear out any leftover entries from a previously longer list.
    int oldCount = 0;
    cfg->Read(_("SecomCount"), &oldCount, 0);
    for (int i = (int)m_secomConnections.size(); i < oldCount; ++i) {
        wxString prefix = wxString::Format(_("Secom%d"), i);
        cfg->DeleteGroup(prefix);
    }

    cfg->Write(_("SecomCount"), (int)m_secomConnections.size());
    for (size_t i = 0; i < m_secomConnections.size(); ++i) {
        const SecomConfig &sc = m_secomConnections[i];
        wxString prefix = wxString::Format(_("Secom%d"), (int)i);
        cfg->Write(prefix + _("Name"),      sc.name);
        cfg->Write(prefix + _("Url"),       sc.baseUrl);
        cfg->Write(prefix + _("DataRef"),   sc.dataReference);
        cfg->Write(prefix + _("ApiKey"),    sc.apiKey);
        cfg->Write(prefix + _("CertFile"),  sc.certFile);
        cfg->Write(prefix + _("KeyFile"),   sc.keyFile);
        cfg->Write(prefix + _("CaBundle"),  sc.caBundle);
        cfg->Write(prefix + _("SslVerify"), (int)sc.sslVerify);
        cfg->Write(prefix + _("Timeout"),   sc.timeoutSec);
    }

    // Remove the legacy single-connection keys once migrated so they don't
    // resurface if SecomCount is ever deleted.
    cfg->DeleteEntry(_("SecomUrl"),       false);
    cfg->DeleteEntry(_("SecomDataRef"),   false);
    cfg->DeleteEntry(_("SecomApiKey"),    false);
    cfg->DeleteEntry(_("SecomCertFile"),  false);
    cfg->DeleteEntry(_("SecomKeyFile"),   false);
    cfg->DeleteEntry(_("SecomCaBundle"),  false);
    cfg->DeleteEntry(_("SecomSslVerify"), false);
    cfg->DeleteEntry(_("SecomTimeout"),   false);

    cfg->Write(_("SecomAutoRefresh"),        (int)m_autoRefreshEnabled);
    cfg->Write(_("SecomAutoRefreshMinutes"), m_autoRefreshMinutes);

    const WarningColors &colors = m_layer->GetWarningColors();
    cfg->Write(_("WarningColorLocal"),   ColorToString(colors.local));
    cfg->Write(_("WarningColorCoastal"), ColorToString(colors.coastal));
    cfg->Write(_("WarningColorSubArea"), ColorToString(colors.subArea));
    cfg->Write(_("WarningColorNavarea"), ColorToString(colors.navarea));
    cfg->Write(_("WarningColorOther"),   ColorToString(colors.other));

    cfg->Flush();
}
