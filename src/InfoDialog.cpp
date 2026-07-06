#include "InfoDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/button.h>

InfoDialog::InfoDialog(wxWindow *parent, const S124Warning &w)
    : wxDialog(parent, wxID_ANY, _("S-124 Navigational Warning"),
               wxDefaultPosition, wxSize(460, 380),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    auto addMeta = [&](const wxString &label, const wxString &value) {
        if (value.IsEmpty()) return;
        wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *lbl = new wxStaticText(this, wxID_ANY, label,
                                             wxDefaultPosition, wxSize(130, -1));
        wxFont bold = lbl->GetFont();
        bold.SetWeight(wxFONTWEIGHT_BOLD);
        lbl->SetFont(bold);
        row->Add(lbl, 0, wxALIGN_TOP | wxRIGHT, 6);
        row->Add(new wxStaticText(this, wxID_ANY, value), 1, wxEXPAND);
        top->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    };

    // Build title: "NAVAREA I / 2024 – Warning #42"
    wxString title;
    if (!w.seriesName.IsEmpty())
        title = w.seriesName;
    if (w.warningNumber > 0) {
        if (!title.IsEmpty()) title += wxT(" / ");
        if (w.year > 0) title += wxString::Format(wxT("%d – "), w.year);
        title += wxString::Format(_("Warning #%d"), w.warningNumber);
    }
    if (!title.IsEmpty()) {
        wxStaticText *titleLbl = new wxStaticText(this, wxID_ANY, title);
        wxFont f = titleLbl->GetFont();
        f.SetPointSize(f.GetPointSize() + 1);
        f.SetWeight(wxFONTWEIGHT_BOLD);
        titleLbl->SetFont(f);
        top->Add(titleLbl, 0, wxALL, 10);
        top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    }

    wxString category = w.warningCategory;
    if (!w.warningSubject.IsEmpty()) {
        if (!category.IsEmpty()) category += wxT(" – ");
        category += w.warningSubject;
    }

    wxString effective;
    if (!w.effectiveStart.IsEmpty() || !w.effectiveEnd.IsEmpty()) {
        effective = w.effectiveStart;
        if (!w.effectiveEnd.IsEmpty()) {
            if (!effective.IsEmpty()) effective += wxT("  →  ");
            effective += w.effectiveEnd;
        }
    }

    addMeta(_("Type:"),        w.warningTypeLabel());
    addMeta(_("Category:"),    category);
    addMeta(_("Area:"),        w.areaText);
    addMeta(_("Effective:"),   effective);
    addMeta(_("Published:"),   w.publicationTime);
    addMeta(_("Cancelled:"),   w.cancellationDate);
    addMeta(_("Language:"),    w.language);

    // Position (centroid)
    if (w.centroidLat != 0.0 || w.centroidLon != 0.0) {
        wxString pos = wxString::Format(wxT("%.5f°  %.5f°"),
                                        w.centroidLat, w.centroidLon);
        addMeta(_("Position:"), pos);
    }

    top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    top->Add(new wxStaticText(this, wxID_ANY, _("Warning text:")),
             0, wxLEFT | wxTOP, 10);

    wxTextCtrl *txt = new wxTextCtrl(
        this, wxID_ANY,
        w.warningText.IsEmpty() ? _("(no text)") : w.warningText,
        wxDefaultPosition, wxSize(-1, 140),
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    top->Add(txt, 1, wxALL | wxEXPAND, 10);

    wxButton *ok = new wxButton(this, wxID_OK, _("Close"));
    ok->SetDefault();
    top->Add(ok, 0, wxALL | wxALIGN_RIGHT, 10);

    SetSizerAndFit(top);
    Centre();
}
