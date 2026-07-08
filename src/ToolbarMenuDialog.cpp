#include "ToolbarMenuDialog.h"
#include <wx/sizer.h>
#include <wx/button.h>

ToolbarMenuDialog::ToolbarMenuDialog(wxWindow *parent, bool secomConfigured)
    : wxDialog(parent, wxID_ANY, _("S-124 Warnings"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    auto addButton = [&](Action action, const wxString &label, bool enabled = true) {
        wxButton *btn = new wxButton(this, wxID_ANY, label);
        btn->Enable(enabled);
        btn->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { OnButton(action); });
        top->Add(btn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    };

    addButton(ACTION_OPEN_FILE,     _("Open local S-124 file…"));
    addButton(ACTION_OPEN_FOLDER,   _("Open S-124 folder…"));
    addButton(ACTION_SECOM_CFG,     _("Connect to SECOM…"));
    addButton(ACTION_SECOM_REFRESH, _("Refresh from SECOM"), secomConfigured);
    addButton(ACTION_CLEAR,         _("Clear all warnings"));

    wxButton *cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
    top->Add(cancel, 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(top);
    Centre();
}

void ToolbarMenuDialog::OnButton(Action action)
{
    m_action = action;
    EndModal(wxID_OK);
}
