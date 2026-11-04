// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_DEVICESWINDOW_H
#define MAME_DEBUGGER_GTK_DEVICESWINDOW_H

#pragma once

#include "windowgtk.h"


namespace osd::debugger::gtk {

//============================================================
//  The Devices Window.
//============================================================
class devices_window : public window_gtk
{
public:
	devices_window(debugger_gtk &debugger, window_gtk *parent = nullptr);
	virtual ~devices_window();

protected:
	virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	static GListModel *create_children(gpointer item, gpointer user_data);
	static void on_setup_tag(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data);
	static void on_bind_tag(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data);
	static void on_setup_name(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data);
	static void on_bind_name(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data);
	static void on_activate(GtkColumnView *view, guint position, gpointer user_data);

	void activated(device_t *dev);

	GtkWidget *m_devices_view;
	GtkSingleSelection *m_selection;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_DEVICESWINDOW_H
