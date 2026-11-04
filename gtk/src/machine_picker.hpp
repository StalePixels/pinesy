// The machine picker: a dialog with the makers in a sidebar, their
// machines beside them, and a search bar.

#ifndef PINESY_GTK_MACHINE_PICKER_HPP
#define PINESY_GTK_MACHINE_PICKER_HPP

#include "pinesy_core.hpp"

#include <gtk/gtk.h>

#include <functional>

namespace pinesy::gtk {

class Application;

// Presents the picker on the window of parent. on_choose gets the
// machine the user chose; nothing is called when the picker is
// dismissed. A machine cannot be chosen while a machine runs.
void present_machine_picker(GtkWidget *parent, Application &app,
		std::function<void(const core::Catalogue::Machine &)> on_choose);

} // namespace pinesy::gtk

#endif
