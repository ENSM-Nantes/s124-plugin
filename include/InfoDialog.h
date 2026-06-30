#ifndef INFO_DIALOG_H
#define INFO_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>
#include "S124Warning.h"

class InfoDialog : public wxDialog
{
public:
    InfoDialog(wxWindow *parent, const S124Warning &warning);
};

#endif // INFO_DIALOG_H
