#include "InfoDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/button.h>

InfoDialog::InfoDialog(wxWindow *parent,
                       const wxString &title,
                       const wxString &info,
                       double lat,
                       double lon)
    : wxDialog(parent, wxID_ANY, title,
               wxDefaultPosition, wxSize(380, 260),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    // Coordinates label
    wxString coordStr = wxString::Format(
        _("Latitude: %.6f°   Longitude: %.6f°"), lat, lon);
    wxStaticText *coordLabel = new wxStaticText(this, wxID_ANY, coordStr);
    wxFont boldFont = coordLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    coordLabel->SetFont(boldFont);
    mainSizer->Add(coordLabel, 0, wxALL | wxEXPAND, 10);

    // Separator
    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // Information label
    mainSizer->Add(new wxStaticText(this, wxID_ANY, _("Information:")),
                   0, wxLEFT | wxTOP, 10);

    // Scrollable text area for the information field
    wxTextCtrl *infoCtrl = new wxTextCtrl(
        this, wxID_ANY, info,
        wxDefaultPosition, wxSize(-1, 130),
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    mainSizer->Add(infoCtrl, 1, wxALL | wxEXPAND, 10);

    // OK button
    wxButton *okBtn = new wxButton(this, wxID_OK, _("Close"));
    okBtn->SetDefault();
    mainSizer->Add(okBtn, 0, wxALL | wxALIGN_RIGHT, 10);

    SetSizerAndFit(mainSizer);
    Centre();
}
