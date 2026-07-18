#ifndef SECOM_LIST_DIALOG_H
#define SECOM_LIST_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/spinctrl.h>
#include <vector>
#include "SecomClient.h"

// Manages the list of SECOM connections (add/edit/remove/duplicate) plus the
// global automatic-refresh setting (enable + interval in minutes) shared by
// all of them.
class SecomListDialog : public wxDialog
{
public:
    SecomListDialog(wxWindow *parent,
                     const std::vector<SecomConfig> &connections,
                     bool autoRefreshEnabled, int autoRefreshMinutes);

    std::vector<SecomConfig> GetConnections() const { return m_connections; }
    bool GetAutoRefreshEnabled() const { return m_autoRefresh->GetValue(); }
    int  GetAutoRefreshMinutes() const { return m_intervalMinutes->GetValue(); }

private:
    void OnAdd(wxCommandEvent &event);
    void OnEdit(wxCommandEvent &event);
    void OnDuplicate(wxCommandEvent &event);
    void OnRemove(wxCommandEvent &event);
    void OnListDClick(wxCommandEvent &event);

    wxString LabelFor(const SecomConfig &cfg) const;
    void RebuildListBox();

    wxListBox  *m_list;
    wxButton   *m_editBtn;
    wxButton   *m_duplicateBtn;
    wxButton   *m_removeBtn;
    wxCheckBox *m_autoRefresh;
    wxSpinCtrl *m_intervalMinutes;

    std::vector<SecomConfig> m_connections;
};

#endif // SECOM_LIST_DIALOG_H
