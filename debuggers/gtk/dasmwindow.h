// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_DASMWINDOW_H
#define MAME_DEBUGGER_GTK_DASMWINDOW_H

#pragma once

#include "debuggerview.h"
#include "windowgtk.h"

#include <memory>


namespace osd::debugger::gtk {

//============================================================
//  The Disassembly Window.
//============================================================
class dasm_window : public window_gtk
{
public:
	dasm_window(debugger_gtk &debugger, window_gtk *parent = nullptr);
	virtual ~dasm_window();

	virtual void restore_configuration(util::xml::data_node const &node) override;

protected:
	virtual void debug_act_open_dasm() override;

	virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	static void on_cpu_selected(GObject *object, GParamSpec *pspec, gpointer user_data);

	// the keys the Qt debugger handles in its event filter
	bool input_key(guint keyval);

	void cpu_changed(int index);
	void set_cpu_index(int index);
	void expression_submitted();
	void expression_edited();

	void toggle_breakpoint_at_cursor();
	void enable_breakpoint_at_cursor();
	void run_to_cursor();
	void right_bar_changed(int column);

	void dasm_view_updated();

	void populate_combo_box();
	void set_to_current_cpu();

	// Widgets
	history_entry m_input_edit;
	GtkWidget *m_cpu_combo_box;
	std::unique_ptr<debugger_view> m_dasm_view;

	// Menu items
	GMenu *m_breakpoint_section;
	GSimpleAction *m_breakpoint_toggle_act;
	GSimpleAction *m_breakpoint_enable_act;
	GSimpleAction *m_run_to_cursor_act;
	GSimpleAction *m_right_bar_group;
	std::string m_breakpoint_toggle_text;
	std::string m_breakpoint_enable_text;

	// Expression history
	command_history m_input_history;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_DASMWINDOW_H
