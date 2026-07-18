#include "SecomListDialog.h"
#include "SecomDialog.h"
#include "Utf8Text.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/button.h>

SecomListDialog::SecomListDialog(wxWindow *parent,
                                  const std::vector<SecomConfig> &connections,
                                  bool autoRefreshEnabled, int autoRefreshMinutes)
    : wxDialog(parent, wxID_ANY, _("SECOM Connections"),
               wxDefaultPosition, wxSize(480, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_connections(connections)
{
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *listRow = new wxBoxSizer(wxHORIZONTAL);

    m_list = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            0, nullptr, wxLB_SINGLE);
    m_list->Bind(wxEVT_LISTBOX_DCLICK, &SecomListDialog::OnListDClick, this);
    m_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) {
        bool has = m_list->GetSelection() != wxNOT_FOUND;
        m_editBtn->Enable(has);
        m_duplicateBtn->Enable(has);
        m_removeBtn->Enable(has);
    });
    listRow->Add(m_list, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer *btnCol = new wxBoxSizer(wxVERTICAL);
    wxButton *addBtn = new wxButton(this, wxID_ANY, _U("Add…"));
    addBtn->Bind(wxEVT_BUTTON, &SecomListDialog::OnAdd, this);
    btnCol->Add(addBtn, 0, wxEXPAND | wxBOTTOM, 6);

    m_editBtn = new wxButton(this, wxID_ANY, _U("Edit…"));
    m_editBtn->Bind(wxEVT_BUTTON, &SecomListDialog::OnEdit, this);
    btnCol->Add(m_editBtn, 0, wxEXPAND | wxBOTTOM, 6);

    m_duplicateBtn = new wxButton(this, wxID_ANY, _("Duplicate"));
    m_duplicateBtn->Bind(wxEVT_BUTTON, &SecomListDialog::OnDuplicate, this);
    btnCol->Add(m_duplicateBtn, 0, wxEXPAND | wxBOTTOM, 6);

    m_removeBtn = new wxButton(this, wxID_ANY, _("Remove"));
    m_removeBtn->Bind(wxEVT_BUTTON, &SecomListDialog::OnRemove, this);
    btnCol->Add(m_removeBtn, 0, wxEXPAND);

    listRow->Add(btnCol, 0, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 10);
    top->Add(listRow, 1, wxEXPAND);

    top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    wxBoxSizer *refreshRow = new wxBoxSizer(wxHORIZONTAL);
    m_autoRefresh = new wxCheckBox(this, wxID_ANY, _("Automatically refresh every"));
    m_autoRefresh->SetValue(autoRefreshEnabled);
    m_autoRefresh->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) {
        m_intervalMinutes->Enable(m_autoRefresh->GetValue());
    });
    refreshRow->Add(m_autoRefresh, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

    m_intervalMinutes = new wxSpinCtrl(this, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, -1),
                                       wxSP_ARROW_KEYS, 1, 1440,
                                       autoRefreshMinutes > 0 ? autoRefreshMinutes : 15);
    m_intervalMinutes->Enable(autoRefreshEnabled);
    refreshRow->Add(m_intervalMinutes, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    refreshRow->Add(new wxStaticText(this, wxID_ANY, _("minute(s)")),
                    0, wxALIGN_CENTER_VERTICAL);
    top->Add(refreshRow, 0, wxALL, 10);

    top->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(top);
    Centre();

    RebuildListBox();
    bool has = m_list->GetSelection() != wxNOT_FOUND;
    m_editBtn->Enable(has);
    m_duplicateBtn->Enable(has);
    m_removeBtn->Enable(has);
}

wxString SecomListDialog::LabelFor(const SecomConfig &cfg) const
{
    if (!cfg.name.IsEmpty()) return cfg.name;
    if (!cfg.baseUrl.IsEmpty()) return cfg.baseUrl;
    return _U("(unnamed connection)");
}

void SecomListDialog::RebuildListBox()
{
    int sel = m_list->GetSelection();
    m_list->Clear();
    for (const auto &cfg : m_connections)
        m_list->Append(LabelFor(cfg));
    if (sel != wxNOT_FOUND && sel < (int)m_connections.size())
        m_list->SetSelection(sel);
}

void SecomListDialog::OnAdd(wxCommandEvent &)
{
    SecomConfig cfg;
    SecomDialog dlg(this, cfg);
    if (dlg.ShowModal() != wxID_OK) return;

    m_connections.push_back(dlg.GetConfig());
    RebuildListBox();
    m_list->SetSelection((int)m_connections.size() - 1);
    m_editBtn->Enable(true);
    m_duplicateBtn->Enable(true);
    m_removeBtn->Enable(true);
}

void SecomListDialog::OnEdit(wxCommandEvent &)
{
    int sel = m_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    SecomDialog dlg(this, m_connections[sel]);
    if (dlg.ShowModal() != wxID_OK) return;

    m_connections[sel] = dlg.GetConfig();
    RebuildListBox();
}

void SecomListDialog::OnDuplicate(wxCommandEvent &)
{
    int sel = m_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    SecomConfig copy = m_connections[sel];
    if (!copy.name.IsEmpty())
        copy.name = wxString::Format(_("%s (copy)"), copy.name);
    m_connections.insert(m_connections.begin() + sel + 1, copy);
    RebuildListBox();
    m_list->SetSelection(sel + 1);
}

void SecomListDialog::OnRemove(wxCommandEvent &)
{
    int sel = m_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    m_connections.erase(m_connections.begin() + sel);
    RebuildListBox();

    bool has = m_list->GetSelection() != wxNOT_FOUND;
    m_editBtn->Enable(has);
    m_duplicateBtn->Enable(has);
    m_removeBtn->Enable(has);
}

void SecomListDialog::OnListDClick(wxCommandEvent &event)
{
    OnEdit(event);
}
