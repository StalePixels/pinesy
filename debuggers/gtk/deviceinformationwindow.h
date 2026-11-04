// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_DEVICEINFORMATIONWINDOW_H
#define MAME_DEBUGGER_GTK_DEVICEINFORMATIONWINDOW_H

#pragma once

#include "windowgtk.h"


namespace osd::debugger::gtk {

//============================================================
//  The Device Information Window.
//============================================================
class device_information_window : public window_gtk
{
public:
	device_information_window(debugger_gtk &debugger, device_t *device = nullptr, window_gtk *parent = nullptr);
	virtual ~device_information_window();

	void set_device(const char *tag);

	virtual void restore_configuration(util::xml::data_node const &node) override;

protected:
	virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	static void on_activate(GtkColumnView *view, guint position, gpointer user_data);

	void open_address_space(int index);
	void add_space_column(char const *title, int column);

	device_t *m_device;
	device_memory_interface *m_memory;
	GtkWidget *m_addrspaces_view;
	GtkSingleSelection *m_addrspaces_model;

	void fill_device_information();
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_DEVICEINFORMATIONWINDOW_H
