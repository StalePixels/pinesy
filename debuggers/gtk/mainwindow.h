// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_MAINWINDOW_H
#define MAME_DEBUGGER_GTK_MAINWINDOW_H

#pragma once

#include "debuggerview.h"
#include "windowgtk.h"

#include "debug/dvdisasm.h"

#include <memory>
#include <string>
#include <vector>


namespace osd::debugger::gtk {

//============================================================
//  The Main Window.  Contains processor and dasm panes.
//============================================================
class main_window : public window_gtk
{
public:
	main_window(debugger_gtk &debugger, window_gtk *parent = nullptr);
	virtual ~main_window();

	void set_processor(device_t *processor);

	virtual void restore_configuration(util::xml::data_node const &node) override;

protected:
	virtual void save_configuration_to_node(util::xml::data_node &node) override;

	// Used to intercept the user clicking 'X' in the upper corner
	virtual bool close_request() override;

	// Closing the main window hides the debugger and runs the emulated system
	virtual void debug_act_close() override;

private:
	static void on_mount_ready(GObject *source, GAsyncResult *result, gpointer user_data);

	// the keys the Qt debugger handles in its event filter
	bool input_key(guint keyval);

	void toggle_breakpoint_at_cursor();
	void enable_breakpoint_at_cursor();
	void run_to_cursor();
	void right_bar_changed(int column);

	void execute_command_slot();
	void command_edited_slot();

	void mount_image(int index);
	void mount_image_file(int index, std::string const &filename);
	void unmount_image(int index);
	void set_image_title(int index, char const *name);

	void dasm_view_updated();

	void create_images_menu();

	void execute_command(bool with_clear);

	// Widgets and panes
	history_entry m_input_edit;
	std::unique_ptr<debugger_view> m_console_view;
	std::unique_ptr<debugger_view> m_proc_view;
	std::unique_ptr<debugger_view> m_dasm_view;
	GtkWidget *m_state_split;
	GtkWidget *m_dasm_split;

	// Menu items
	GMenu *m_breakpoint_section;
	GSimpleAction *m_breakpoint_toggle_act;
	GSimpleAction *m_breakpoint_enable_act;
	GSimpleAction *m_run_to_cursor_act;
	GSimpleAction *m_right_bar_group;
	std::string m_breakpoint_toggle_text;
	std::string m_breakpoint_enable_text;

	// Images menu
	GMenu *m_images_menu;
	std::vector<GMenu *> m_image_menus;
	std::vector<std::string> m_image_titles;
	std::vector<GSimpleAction *> m_unmount_acts;
	GCancellable *m_dialog_cancel;
	std::shared_ptr<main_window *> m_alive;

	// Terminal history
	command_history m_input_history;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_MAINWINDOW_H
