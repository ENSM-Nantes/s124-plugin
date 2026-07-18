#include "SecomDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/button.h>

SecomDialog::SecomDialog(wxWindow *parent, const SecomConfig &cfg)
    : wxDialog(parent, wxID_ANY, _("SECOM Endpoint Configuration"),
               wxDefaultPosition, wxSize(520, -1),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    auto addRow = [&](const wxString &label, wxWindow *ctrl) {
        wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *lbl = new wxStaticText(this, wxID_ANY, label,
                                             wxDefaultPosition, wxSize(130, -1));
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        row->Add(ctrl, 1, wxEXPAND);
        top->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    };

    m_name = new wxTextCtrl(this, wxID_ANY, cfg.name);
    addRow(_("Name:"), m_name);

    m_url = new wxTextCtrl(this, wxID_ANY, cfg.baseUrl);
    addRow(_("URL:"), m_url);

    m_dataRef = new wxTextCtrl(this, wxID_ANY, cfg.dataReference);
    addRow(_("Data reference:"), m_dataRef);

    m_apiKey = new wxTextCtrl(this, wxID_ANY, cfg.apiKey,
                              wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    addRow(_("API key:"), m_apiKey);

    top->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 8);
    top->Add(new wxStaticText(this, wxID_ANY, _("Client certificate (optional):")),
             0, wxLEFT, 10);

    m_certFile = new wxFilePickerCtrl(this, wxID_ANY, cfg.certFile,
                                      _("Select client certificate"),
                                      _("PEM files (*.pem)|*.pem|All files|*.*"));
    addRow(_("Cert (PEM):"), m_certFile);

    m_keyFile = new wxFilePickerCtrl(this, wxID_ANY, cfg.keyFile,
                                     _("Select client key"),
                                     _("PEM files (*.pem)|*.pem|All files|*.*"));
    addRow(_("Key (PEM):"), m_keyFile);

    m_caBundle = new wxFilePickerCtrl(this, wxID_ANY, cfg.caBundle,
                                      _("Select CA bundle"),
                                      _("PEM files (*.pem;*.crt)|*.pem;*.crt|All files|*.*"));
    addRow(_("CA bundle:"), m_caBundle);

    top->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 8);

    wxBoxSizer *row2 = new wxBoxSizer(wxHORIZONTAL);
    m_sslVerify = new wxCheckBox(this, wxID_ANY, _("Verify SSL certificate"));
    m_sslVerify->SetValue(cfg.sslVerify);
    row2->Add(m_sslVerify, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 20);
    row2->Add(new wxStaticText(this, wxID_ANY, _("Timeout (s):")),
              0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_timeout = new wxSpinCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(70, -1),
                                wxSP_ARROW_KEYS, 5, 300, cfg.timeoutSec);
    row2->Add(m_timeout, 0, wxALIGN_CENTER_VERTICAL);
    top->Add(row2, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    top->Add(CreateButtonSizer(wxOK | wxCANCEL), 0,
             wxEXPAND | wxALL, 10);

    SetSizerAndFit(top);
    Centre();
}

SecomConfig SecomDialog::GetConfig() const
{
    SecomConfig cfg;
    cfg.name          = m_name->GetValue().Trim();
    cfg.baseUrl       = m_url->GetValue().Trim();
    cfg.dataReference = m_dataRef->GetValue().Trim();
    cfg.apiKey        = m_apiKey->GetValue();
    cfg.certFile      = m_certFile->GetPath();
    cfg.keyFile       = m_keyFile->GetPath();
    cfg.caBundle      = m_caBundle->GetPath();
    cfg.sslVerify     = m_sslVerify->GetValue();
    cfg.timeoutSec    = m_timeout->GetValue();
    return cfg;
}
