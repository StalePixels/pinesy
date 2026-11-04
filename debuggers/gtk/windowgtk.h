// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_WINDOWGTK_H
#define MAME_DEBUGGER_GTK_WINDOWGTK_H

#pragma once

#include "modules/debugger/xmlconfig.h"

#include <gtk/gtk.h>

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>


namespace osd::debugger::gtk {

class window_gtk;


//============================================================
//  The GTK debugger module interface
//============================================================
class debugger_gtk
{
public:
	virtual ~debugger_gtk() { }

	virtual running_machine &machine() const = 0;

	void hide_all();

	void add_window(window_gtk &window);
	void remove_window(window_gtk &window);

protected:
	void exit_debugger();
	void show_all_windows();
	void save_configuration(util::xml::data_node &parentnode);

private:
	std::vector<window_gtk *> m_windows;
};


//============================================================
//  The GTK window that everyone derives from.
//============================================================
class window_gtk
{
public:
	virtual ~window_gtk();

	virtual void restore_configuration(util::xml::data_node const &node);
	virtual void debugger_exit();

	void show();
	void hide();
	void present();
	void save_configuration(util::xml::data_node &parentnode);

protected:
	window_gtk(debugger_gtk &debugger, window_gtk *parent = nullptr);

	virtual void debug_act_open_memory();
	virtual void debug_act_open_dasm();
	void debug_act_open_log();
	void debug_act_open_points();
	void debug_act_open_devices();
	void debug_act_run();
	void debug_act_run_and_hide();
	void debug_act_run_to_next_cpu();
	void debug_act_run_next_int();
	void debug_act_run_next_vblank();
	void debug_act_step_into();
	void debug_act_step_over();
	void debug_act_step_out();
	void debug_act_soft_reset();
	void debug_act_hard_reset();
	virtual void debug_act_close();
	void debug_act_quit();

	// return true to keep the window open
	virtual bool close_request() { return false; }

	virtual void save_configuration_to_node(util::xml::data_node &node);

	GtkWindow *window() const { return m_window; }
	void set_title(char const *title);
	void set_content(GtkWidget *content);

	// menus and actions; action names are used as "dbg.<name>"
	GMenu *add_menu(char const *label);
	GSimpleAction *add_action(char const *name, std::function<void ()> &&handler);
	GSimpleAction *add_radio_action(char const *name, int initial, std::function<void (int)> &&handler);
	GSimpleAction *add_toggle_action(char const *name, bool initial, std::function<void (bool)> &&handler);
	void append_item(GMenu *menu, char const *label, char const *action, char const *accel = nullptr, GVariant *target = nullptr);
	void add_shortcut(char const *accel, char const *action, GVariant *target = nullptr);

	static GMenuItem *make_item(char const *label, char const *action, char const *accel = nullptr, GVariant *target = nullptr);
	static void replace_item(GMenu *menu, int position, char const *label, char const *action, char const *accel = nullptr);
	static void trigger_radio(GSimpleAction *action, int value);
	static void set_radio_checked(GSimpleAction *action, int value);
	static int radio_state(GSimpleAction *action);
	static void set_toggle_checked(GSimpleAction *action, bool value);
	static std::string escape_mnemonic(std::string_view label);

	debugger_gtk &m_debugger;
	running_machine &m_machine;

private:
	struct action_binding
	{
		GSimpleAction *action;
		std::function<void (GVariant *)> handler;
	};

	static gboolean on_close_request(GtkWindow *window, gpointer user_data);
	static void on_destroy(GtkWidget *widget, gpointer user_data);
	static void on_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);

	GSimpleAction *bind_action(GSimpleAction *action, std::function<void (GVariant *)> &&handler);

	GtkWindow *m_window;
	GtkWidget *m_box;
	GtkWidget *m_content;
	GMenu *m_menubar;
	GSimpleActionGroup *m_actions;
	GtkShortcutController *m_shortcuts;
	std::vector<std::unique_ptr<action_binding> > m_bindings;
};


//============================================================
//  Command history helper
//============================================================
class command_history
{
public:
	command_history();
	~command_history();

	void add(std::string const &entry);
	std::string const *previous(std::string const &current);
	std::string const *next(std::string const &current);
	void edit();
	void reset();
	void clear();

	void restore_configuration_from_node(util::xml::data_node const &node);
	void save_configuration_to_node(util::xml::data_node &node);

private:
	static inline constexpr unsigned CAPACITY = 100U;

	std::deque<std::string> m_history;
	std::unique_ptr<std::string> m_current;
	int m_position;
};


//============================================================
//  Single line input with the history keys the Qt debugger
//  handles in its event filters
//============================================================
class history_entry
{
public:
	history_entry();
	~history_entry();

	GtkWidget *widget() const { return m_entry; }

	std::string text() const;
	void set_text(std::string const &text);
	void clear() { set_text(std::string()); }
	void select_all();
	void move_to_end();

	// Return pressed
	std::function<void ()> on_return;
	// text changed by the user
	std::function<void ()> on_edited;
	// key handler run before the entry sees the key; return true when handled
	std::function<bool (guint keyval)> on_key;

private:
	static void on_activate(GtkEntry *entry, gpointer user_data);
	static void on_changed(GtkEditable *editable, gpointer user_data);
	static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

	GtkWidget *m_entry;
	GtkEventController *m_keys;
	bool m_setting;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_WINDOWGTK_H
