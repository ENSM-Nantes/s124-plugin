#ifndef WARNING_COLOR_DIALOG_H
#define WARNING_COLOR_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/clrpicker.h>
#include "PointsLayer.h"

// Lets the user pick a display color for each warning-severity group. All
// SECOM connections and local files share the same set of colors - there is
// no per-connection color, only per-severity.
class WarningColorDialog : public wxDialog
{
public:
    WarningColorDialog(wxWindow *parent, const WarningColors &colors);

    WarningColors GetColors() const;

private:
    void OnResetDefaults(wxCommandEvent &event);

    wxColourPickerCtrl *m_local;
    wxColourPickerCtrl *m_coastal;
    wxColourPickerCtrl *m_subArea;
    wxColourPickerCtrl *m_navarea;
    wxColourPickerCtrl *m_other;
};

#endif // WARNING_COLOR_DIALOG_H
