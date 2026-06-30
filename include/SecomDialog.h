#ifndef SECOM_DIALOG_H
#define SECOM_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>
#include "SecomClient.h"

class SecomDialog : public wxDialog
{
public:
    SecomDialog(wxWindow *parent, const SecomConfig &cfg);
    SecomConfig GetConfig() const;

private:
    wxTextCtrl     *m_url;
    wxTextCtrl     *m_dataRef;
    wxTextCtrl     *m_apiKey;
    wxFilePickerCtrl *m_certFile;
    wxFilePickerCtrl *m_keyFile;
    wxFilePickerCtrl *m_caBundle;
    wxCheckBox     *m_sslVerify;
    wxSpinCtrl     *m_timeout;
};

#endif // SECOM_DIALOG_H
