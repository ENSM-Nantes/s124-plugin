#include "WarningColorDialog.h"
#include "Utf8Text.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/button.h>

WarningColorDialog::WarningColorDialog(wxWindow *parent, const WarningColors &colors)
    : wxDialog(parent, wxID_ANY, _U("Warning Colors"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    top->Add(new wxStaticText(this, wxID_ANY,
                 _U("Color used for each warning type, across every SECOM\n"
                    "connection and local file:")),
             0, wxALL, 10);

    auto addRow = [&](const wxString &label, const wxColour &initial) {
        wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *lbl = new wxStaticText(this, wxID_ANY, label,
                                             wxDefaultPosition, wxSize(160, -1));
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        wxColourPickerCtrl *picker = new wxColourPickerCtrl(this, wxID_ANY, initial);
        row->Add(picker, 0);
        top->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
        return picker;
    };

    m_local   = addRow(_U("Local (types 1, 6):"),          colors.local);
    m_coastal = addRow(_U("Coastal (types 2, 7):"),        colors.coastal);
    m_subArea = addRow(_U("Sub-area (types 3, 8):"),       colors.subArea);
    m_navarea = addRow(_U("NAVAREA (types 4, 9):"),        colors.navarea);
    m_other   = addRow(_U("Other/unrecognized types:"),    colors.other);

    top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    wxButton *resetBtn = new wxButton(this, wxID_ANY, _U("Reset to defaults"));
    resetBtn->Bind(wxEVT_BUTTON, &WarningColorDialog::OnResetDefaults, this);
    top->Add(resetBtn, 0, wxALL, 10);

    top->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(top);
    Centre();
}

void WarningColorDialog::OnResetDefaults(wxCommandEvent &)
{
    WarningColors defaults;
    m_local->SetColour(defaults.local);
    m_coastal->SetColour(defaults.coastal);
    m_subArea->SetColour(defaults.subArea);
    m_navarea->SetColour(defaults.navarea);
    m_other->SetColour(defaults.other);
}

WarningColors WarningColorDialog::GetColors() const
{
    WarningColors colors;
    colors.local   = m_local->GetColour();
    colors.coastal = m_coastal->GetColour();
    colors.subArea = m_subArea->GetColour();
    colors.navarea = m_navarea->GetColour();
    colors.other   = m_other->GetColour();
    return colors;
}
