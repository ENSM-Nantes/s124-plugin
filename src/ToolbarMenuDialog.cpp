#include "ToolbarMenuDialog.h"
#include "Utf8Text.h"
#include <wx/sizer.h>
#include <wx/button.h>

ToolbarMenuDialog::ToolbarMenuDialog(wxWindow *parent, bool secomConfigured)
    : wxDialog(parent, wxID_ANY, _("S-124"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    auto addButton = [&](Action action, const wxString &label, bool enabled = true) {
        wxButton *btn = new wxButton(this, wxID_ANY, label);
        btn->Enable(enabled);
        btn->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { OnButton(action); });
        top->Add(btn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    };

    addButton(ACTION_SECOM_CFG,     _U("SECOM connections"));
    addButton(ACTION_SECOM_REFRESH, _U("Refresh from SECOM"), secomConfigured);
    addButton(ACTION_OPEN_FILE,     _U("Open local S-124 file"));
    addButton(ACTION_OPEN_FOLDER,   _U("Open S-124 folder"));
    addButton(ACTION_WARNING_COLORS, _U("Warning colors"));
    addButton(ACTION_CLEAR,         _U("Clear all warnings"));

    wxButton *cancel = new wxButton(this, wxID_CANCEL, _U("Close"));
    top->Add(cancel, 0, wxEXPAND | wxALL, 10);

    // Force a minimum width well past what the buttons alone need - on
    // Windows a dialog sized tightly to narrow content ends up narrower
    // than the title bar's own chrome (icon + system buttons), which
    // truncates or hides the title text entirely.
    top->SetMinSize(wxSize(280, -1));

    SetSizerAndFit(top);
    Centre();
}

void ToolbarMenuDialog::OnButton(Action action)
{
    m_action = action;
    EndModal(wxID_OK);
}
