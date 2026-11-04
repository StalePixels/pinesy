// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_MEMORYWINDOW_H
#define MAME_DEBUGGER_GTK_MEMORYWINDOW_H

#pragma once

#include "debuggerview.h"
#include "windowgtk.h"

#include <memory>
#include <string>


namespace osd::debugger::gtk {

//=========================================================================
//  The mem window gets its own debugger view to handle right click pop-ups
//=========================================================================
class debugger_mem_view : public debugger_view
{
public:
	debugger_mem_view(debug_view_type type, running_machine &machine);
	virtual ~debugger_mem_view();

protected:
	virtual void add_items_to_context_menu(GMenu *menu) override;

private:
	static void on_copy_last_pc(GSimpleAction *action, GVariant *parameter, gpointer user_data);

	GSimpleAction *m_copy_last_pc_action;
	std::string m_last_pc;
};


//============================================================
//  The Memory Window.
//============================================================
class memory_window : public window_gtk
{
public:
	memory_window(debugger_gtk &debugger, window_gtk *parent = nullptr);
	virtual ~memory_window();

	virtual void restore_configuration(util::xml::data_node const &node) override;

	bool select_space(address_space &space);

protected:
	virtual void debug_act_open_memory() override;

	virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	static void on_region_selected(GObject *object, GParamSpec *pspec, gpointer user_data);

	// the keys the Qt debugger handles in its event filter
	bool input_key(guint keyval);

	void memory_region_changed(int index);
	void set_region_index(int index);
	void expression_submitted();
	void expression_edited();

	void format_changed(int format);
	void address_changed(bool physical);
	void radix_changed(int radix);
	void reverse_changed(bool reverse);
	void increase_bytes_per_line();
	void decrease_bytes_per_line();

	void populate_combo_box();
	void set_to_current_cpu();

	// Widgets
	history_entry m_input_edit;
	GtkWidget *m_memory_combo_box;
	std::unique_ptr<debugger_mem_view> m_mem_table;

	// Menu items
	GSimpleAction *m_data_format;
	GSimpleAction *m_address_group;
	GSimpleAction *m_radix_group;
	GSimpleAction *m_reverse_act;

	// Expression history
	command_history m_input_history;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_MEMORYWINDOW_H
