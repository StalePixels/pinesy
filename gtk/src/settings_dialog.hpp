// Preferences: the ROM folders, the data folder and the debugger.

#ifndef PINESY_GTK_SETTINGS_DIALOG_HPP
#define PINESY_GTK_SETTINGS_DIALOG_HPP

#include <giomm/settings.h>
#include <gtk/gtk.h>

namespace pinesy::gtk {

// Presents the preferences dialog on the window of parent, which may be
// null. Every change is stored at once.
void present_settings_dialog(GtkWidget *parent, const Glib::RefPtr<Gio::Settings> &settings);

} // namespace pinesy::gtk

#endif
