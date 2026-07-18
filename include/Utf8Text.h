#ifndef XMLPOINTS_UTF8TEXT_H
#define XMLPOINTS_UTF8TEXT_H

#include <wx/intl.h>
#include <wx/string.h>

// All plugin sources are saved as UTF-8. Plain `_("...")`/wxT("...") literals
// go through wxConvLibc for their narrow->wide conversion, which on Windows
// decodes using the system ANSI codepage rather than UTF-8. Any literal with
// non-ASCII punctuation (en dashes, ellipses, arrows, degree signs) then
// shows up mangled in menus and dialogs. Decoding explicitly with
// wxString::FromUTF8() sidesteps the codepage entirely.
#define _U(s) wxGetTranslation(wxString::FromUTF8(s))
#define U8(s) wxString::FromUTF8(s)

#endif
