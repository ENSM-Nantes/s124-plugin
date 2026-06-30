#ifndef INFO_DIALOG_H
#define INFO_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>

class InfoDialog : public wxDialog
{
public:
    InfoDialog(wxWindow *parent, const wxString &title,
               const wxString &info, double lat, double lon);
};

#endif // INFO_DIALOG_H
