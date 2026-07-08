#ifndef TOOLBAR_MENU_DIALOG_H
#define TOOLBAR_MENU_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>

// Stands in for the toolbar button's action menu. A native wxMenu shown via
// wxWindow::PopupMenu() (Windows: ::TrackPopupMenu()) is unreliable inside
// OpenCPN on Windows: the OS auto-cancels it the instant its owner window
// loses activation, and OpenCPN's own periodic chart/instrument refresh does
// that every second or two - closing the menu before the user can click
// anything, with no click involved. A regular dialog is a real window and
// isn't subject to that activation-loss auto-cancel.
class ToolbarMenuDialog : public wxDialog
{
public:
    enum Action {
        ACTION_NONE = 0,
        ACTION_OPEN_FILE,
        ACTION_OPEN_FOLDER,
        ACTION_SECOM_CFG,
        ACTION_SECOM_REFRESH,
        ACTION_CLEAR
    };

    ToolbarMenuDialog(wxWindow *parent, bool secomConfigured);

    Action GetAction() const { return m_action; }

private:
    void OnButton(Action action);

    Action m_action = ACTION_NONE;
};

#endif // TOOLBAR_MENU_DIALOG_H
