// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "windowgtk.h"

#include "breakpointswindow.h"
#include "dasmwindow.h"
#include "deviceswindow.h"
#include "logwindow.h"
#include "memorywindow.h"

#include "debugger.h"
#include "debug/debugcon.h"
#include "debug/debugcpu.h"

#include "util/xmlfile.h"

#include <algorithm>


namespace osd::debugger::gtk {

void debugger_gtk::hide_all()
{
	for (window_gtk *const window : m_windows)
		window->hide();
}


void debugger_gtk::add_window(window_gtk &window)
{
	m_windows.emplace_back(&window);
}


void debugger_gtk::remove_window(window_gtk &window)
{
	auto const it = std::find(m_windows.begin(), m_windows.end(), &window);
	if (m_windows.end() != it)
		m_windows.erase(it);
}


void debugger_gtk::exit_debugger()
{
	std::vector<window_gtk *> const windows(m_windows);
	for (window_gtk *const window : windows)
		window->debugger_exit();
}


void debugger_gtk::show_all_windows()
{
	for (window_gtk *const window : m_windows)
		window->show();
}


void debugger_gtk::save_configuration(util::xml::data_node &parentnode)
{
	for (window_gtk *const window : m_windows)
		window->save_configuration(parentnode);
}



// All debug windows are top-level.  The parent window is only used by the
// derived classes to choose a default size, as the Qt debugger uses it to
// place each new window.

window_gtk::window_gtk(debugger_gtk &debugger, window_gtk *parent) :
	m_debugger(debugger),
	m_machine(debugger.machine()),
	m_window(GTK_WINDOW(gtk_window_new())),
	m_box(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)),
	m_content(nullptr),
	m_menubar(g_menu_new()),
	m_actions(g_simple_action_group_new()),
	m_shortcuts(GTK_SHORTCUT_CONTROLLER(gtk_shortcut_controller_new()))
{
	m_debugger.add_window(*this);

	g_signal_connect(m_window, "close-request", G_CALLBACK(on_close_request), this);
	g_signal_connect(m_window, "destroy", G_CALLBACK(on_destroy), this);

	gtk_widget_insert_action_group(GTK_WIDGET(m_window), "dbg", G_ACTION_GROUP(m_actions));

	// menu shortcuts take precedence over the focused widget, as Qt shortcuts do
	gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(m_shortcuts), GTK_PHASE_CAPTURE);
	gtk_widget_add_controller(GTK_WIDGET(m_window), GTK_EVENT_CONTROLLER(m_shortcuts));

	gtk_box_append(GTK_BOX(m_box), gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(m_menubar)));
	gtk_window_set_child(m_window, m_box);

	// The Debug menu bar
	add_action("open-memory", [this] () { debug_act_open_memory(); });
	add_action("open-dasm", [this] () { debug_act_open_dasm(); });
	add_action("open-log", [this] () { debug_act_open_log(); });
	add_action("open-points", [this] () { debug_act_open_points(); });
	add_action("open-devices", [this] () { debug_act_open_devices(); });
	add_action("run", [this] () { debug_act_run(); });
	add_action("run-and-hide", [this] () { debug_act_run_and_hide(); });
	add_action("run-to-next-cpu", [this] () { debug_act_run_to_next_cpu(); });
	add_action("run-next-int", [this] () { debug_act_run_next_int(); });
	add_action("run-next-vblank", [this] () { debug_act_run_next_vblank(); });
	add_action("step-into", [this] () { debug_act_step_into(); });
	add_action("step-over", [this] () { debug_act_step_over(); });
	add_action("step-out", [this] () { debug_act_step_out(); });
	add_action("soft-reset", [this] () { debug_act_soft_reset(); });
	add_action("hard-reset", [this] () { debug_act_hard_reset(); });
	add_action("close", [this] () { debug_act_close(); });
	add_action("quit", [this] () { debug_act_quit(); });

	// Construct the menu
	GMenu *const debug_menu = add_menu("_Debug");

	GMenu *const section_open = g_menu_new();
	append_item(section_open, "New _Memory Window", "open-memory", "<Control>m");
	append_item(section_open, "New _Disassembly Window", "open-dasm", "<Control>d");
	append_item(section_open, "New Error _Log Window", "open-log", "<Control>l");
	append_item(section_open, "New (_Break|Watch)points Window", "open-points", "<Control>b");
	append_item(section_open, "New D_evices Window", "open-devices");
	g_menu_append_section(debug_menu, nullptr, G_MENU_MODEL(section_open));
	g_object_unref(section_open);

	GMenu *const section_run = g_menu_new();
	append_item(section_run, "Run", "run", "F5");
	append_item(section_run, "Run And Hide Debugger", "run-and-hide", "F12");
	append_item(section_run, "Run to Next CPU", "run-to-next-cpu", "F6");
	append_item(section_run, "Run to Next Interrupt on This CPU", "run-next-int", "F7");
	append_item(section_run, "Run to Next VBlank", "run-next-vblank", "F8");
	g_menu_append_section(debug_menu, nullptr, G_MENU_MODEL(section_run));
	g_object_unref(section_run);

	GMenu *const section_step = g_menu_new();
	append_item(section_step, "Step Into", "step-into", "F11");
	append_item(section_step, "Step Over", "step-over", "F10");
	append_item(section_step, "Step Out", "step-out", "<Shift>F11");
	g_menu_append_section(debug_menu, nullptr, G_MENU_MODEL(section_step));
	g_object_unref(section_step);

	GMenu *const section_reset = g_menu_new();
	append_item(section_reset, "Soft Reset", "soft-reset", "F3");
	append_item(section_reset, "Hard Reset", "hard-reset", "<Shift>F3");
	g_menu_append_section(debug_menu, nullptr, G_MENU_MODEL(section_reset));
	g_object_unref(section_reset);

	GMenu *const section_close = g_menu_new();
	append_item(section_close, "Close _Window", "close", "<Control>w");
	append_item(section_close, "_Quit", "quit", "<Control>q");
	g_menu_append_section(debug_menu, nullptr, G_MENU_MODEL(section_close));
	g_object_unref(section_close);
}


window_gtk::~window_gtk()
{
	m_debugger.remove_window(*this);

	for (auto const &binding : m_bindings)
		g_signal_handlers_disconnect_by_data(binding->action, binding.get());
	g_signal_handlers_disconnect_by_data(m_window, this);

	g_object_unref(m_actions);
	g_object_unref(m_menubar);
}


void window_gtk::show()
{
	gtk_widget_set_visible(GTK_WIDGET(m_window), TRUE);
}


void window_gtk::hide()
{
	gtk_widget_set_visible(GTK_WIDGET(m_window), FALSE);
}


void window_gtk::present()
{
	gtk_window_present(m_window);
}


void window_gtk::set_title(char const *title)
{
	gtk_window_set_title(m_window, title);
}


void window_gtk::set_content(GtkWidget *content)
{
	if (m_content)
		gtk_box_remove(GTK_BOX(m_box), m_content);
	m_content = content;
	gtk_widget_set_vexpand(m_content, TRUE);
	gtk_box_append(GTK_BOX(m_box), m_content);
}


GMenu *window_gtk::add_menu(char const *label)
{
	GMenu *const menu = g_menu_new();
	g_menu_append_submenu(m_menubar, label, G_MENU_MODEL(menu));
	g_object_unref(menu);
	return menu;
}


GSimpleAction *window_gtk::bind_action(GSimpleAction *action, std::function<void (GVariant *)> &&handler)
{
	auto binding = std::make_unique<action_binding>();
	binding->action = action;
	binding->handler = std::move(handler);
	g_signal_connect(action, "activate", G_CALLBACK(on_action), binding.get());
	m_bindings.emplace_back(std::move(binding));
	g_action_map_add_action(G_ACTION_MAP(m_actions), G_ACTION(action));
	g_object_unref(action);
	return action;
}


GSimpleAction *window_gtk::add_action(char const *name, std::function<void ()> &&handler)
{
	return bind_action(
			g_simple_action_new(name, nullptr),
			[handler = std::move(handler)] (GVariant *parameter) { handler(); });
}


GSimpleAction *window_gtk::add_radio_action(char const *name, int initial, std::function<void (int)> &&handler)
{
	GSimpleAction *const action = g_simple_action_new_stateful(name, G_VARIANT_TYPE_INT32, g_variant_new_int32(initial));
	return bind_action(
			action,
			[action, handler = std::move(handler)] (GVariant *parameter)
			{
				int const value = g_variant_get_int32(parameter);
				g_simple_action_set_state(action, g_variant_new_int32(value));
				handler(value);
			});
}


GSimpleAction *window_gtk::add_toggle_action(char const *name, bool initial, std::function<void (bool)> &&handler)
{
	GSimpleAction *const action = g_simple_action_new_stateful(name, nullptr, g_variant_new_boolean(initial));
	return bind_action(
			action,
			[action, handler = std::move(handler)] (GVariant *parameter)
			{
				GVariant *const state = g_action_get_state(G_ACTION(action));
				bool const value = !g_variant_get_boolean(state);
				g_variant_unref(state);
				g_simple_action_set_state(action, g_variant_new_boolean(value));
				handler(value);
			});
}


GMenuItem *window_gtk::make_item(char const *label, char const *action, char const *accel, GVariant *target)
{
	std::string const detailed = std::string("dbg.") + action;
	GMenuItem *const item = g_menu_item_new(label, nullptr);
	g_menu_item_set_action_and_target_value(item, detailed.c_str(), target);
	if (accel)
		g_menu_item_set_attribute(item, "accel", "s", accel);
	return item;
}


void window_gtk::append_item(GMenu *menu, char const *label, char const *action, char const *accel, GVariant *target)
{
	if (target)
		g_variant_ref_sink(target);
	GMenuItem *const item = make_item(label, action, accel, target);
	g_menu_append_item(menu, item);
	g_object_unref(item);
	if (accel)
		add_shortcut(accel, action, target);
	if (target)
		g_variant_unref(target);
}


void window_gtk::add_shortcut(char const *accel, char const *action, GVariant *target)
{
	std::string const detailed = std::string("dbg.") + action;
	GtkShortcut *const shortcut = gtk_shortcut_new(
			gtk_shortcut_trigger_parse_string(accel),
			gtk_named_action_new(detailed.c_str()));
	if (target)
		gtk_shortcut_set_arguments(shortcut, target);
	gtk_shortcut_controller_add_shortcut(m_shortcuts, shortcut);
}


void window_gtk::replace_item(GMenu *menu, int position, char const *label, char const *action, char const *accel)
{
	GMenuItem *const item = make_item(label, action, accel);
	g_menu_remove(menu, position);
	g_menu_insert_item(menu, position, item);
	g_object_unref(item);
}


void window_gtk::trigger_radio(GSimpleAction *action, int value)
{
	g_action_activate(G_ACTION(action), g_variant_new_int32(value));
}


void window_gtk::set_radio_checked(GSimpleAction *action, int value)
{
	g_simple_action_set_state(action, g_variant_new_int32(value));
}


int window_gtk::radio_state(GSimpleAction *action)
{
	GVariant *const state = g_action_get_state(G_ACTION(action));
	int const value = g_variant_get_int32(state);
	g_variant_unref(state);
	return value;
}


// QAction::setChecked semantics: a change runs the toggle handler
void window_gtk::set_toggle_checked(GSimpleAction *action, bool value)
{
	GVariant *const state = g_action_get_state(G_ACTION(action));
	bool const current = g_variant_get_boolean(state);
	g_variant_unref(state);
	if (current != value)
		g_action_activate(G_ACTION(action), nullptr);
}


// GTK menus treat '_' as a mnemonic marker
std::string window_gtk::escape_mnemonic(std::string_view label)
{
	std::string result;
	for (char const ch : label)
	{
		if ('_' == ch)
			result += '_';
		result += ch;
	}
	return result;
}


gboolean window_gtk::on_close_request(GtkWindow *window, gpointer user_data)
{
	return static_cast<window_gtk *>(user_data)->close_request() ? TRUE : FALSE;
}


void window_gtk::on_destroy(GtkWidget *widget, gpointer user_data)
{
	delete static_cast<window_gtk *>(user_data);
}


void window_gtk::on_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	static_cast<action_binding *>(user_data)->handler(parameter);
}


void window_gtk::debug_act_open_memory()
{
	memory_window *const foo = new memory_window(m_debugger, this);
	foo->show();
}


void window_gtk::debug_act_open_dasm()
{
	dasm_window *const foo = new dasm_window(m_debugger, this);
	foo->show();
}


void window_gtk::debug_act_open_log()
{
	log_window *const foo = new log_window(m_debugger, this);
	foo->show();
}


void window_gtk::debug_act_open_points()
{
	breakpoints_window *const foo = new breakpoints_window(m_debugger, this);
	foo->show();
}


void window_gtk::debug_act_open_devices()
{
	devices_window *const foo = new devices_window(m_debugger, this);
	foo->show();
}


void window_gtk::debug_act_run()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->go();
}

void window_gtk::debug_act_run_and_hide()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->go();
	m_debugger.hide_all();
}

void window_gtk::debug_act_run_to_next_cpu()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->go_next_device();
}

void window_gtk::debug_act_run_next_int()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->go_interrupt();
}

void window_gtk::debug_act_run_next_vblank()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->go_vblank();
}

void window_gtk::debug_act_step_into()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->single_step();
}

void window_gtk::debug_act_step_over()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->single_step_over();
}

void window_gtk::debug_act_step_out()
{
	m_machine.debugger().console().get_visible_cpu()->debug()->single_step_out();
}

void window_gtk::debug_act_soft_reset()
{
	m_machine.schedule_soft_reset();
	m_machine.debugger().console().get_visible_cpu()->debug()->single_step();
}

void window_gtk::debug_act_hard_reset()
{
	m_machine.schedule_hard_reset();
}

void window_gtk::debug_act_close()
{
	gtk_window_close(m_window);
}

void window_gtk::debug_act_quit()
{
	m_machine.schedule_exit();
}

void window_gtk::debugger_exit()
{
	// destroying the window deletes this object
	gtk_window_destroy(m_window);
}


// GTK cannot read or set a window position, so only the size is restored
void window_gtk::restore_configuration(util::xml::data_node const &node)
{
	int width, height;
	gtk_window_get_default_size(m_window, &width, &height);
	if (0 >= width)
		width = gtk_widget_get_width(GTK_WIDGET(m_window));
	if (0 >= height)
		height = gtk_widget_get_height(GTK_WIDGET(m_window));

	width = node.get_attribute_int(ATTR_WINDOW_WIDTH, width);
	height = node.get_attribute_int(ATTR_WINDOW_HEIGHT, height);

	// TODO: sanity checks, restrict to screen area

	gtk_window_set_default_size(m_window, width, height);
}


void window_gtk::save_configuration(util::xml::data_node &parentnode)
{
	util::xml::data_node *const node = parentnode.add_child(NODE_WINDOW, nullptr);
	if (node)
		save_configuration_to_node(*node);
}


void window_gtk::save_configuration_to_node(util::xml::data_node &node)
{
	int width, height;
	gtk_window_get_default_size(m_window, &width, &height);
	if (0 >= width)
		width = gtk_widget_get_width(GTK_WIDGET(m_window));
	if (0 >= height)
		height = gtk_widget_get_height(GTK_WIDGET(m_window));

	node.set_attribute_int(ATTR_WINDOW_WIDTH, width);
	node.set_attribute_int(ATTR_WINDOW_HEIGHT, height);
}


command_history::command_history() :
	m_history(),
	m_current(),
	m_position(-1)
{
}


command_history::~command_history()
{
}


void command_history::add(std::string const &entry)
{
	if (m_history.empty() || (m_history.front() != entry))
	{
		while (m_history.size() >= CAPACITY)
			m_history.pop_back();
		m_history.push_front(entry);
	}
	m_position = 0;
}


std::string const *command_history::previous(std::string const &current)
{
	if ((m_position + 1) < m_history.size())
	{
		if (0 > m_position)
			m_current = std::make_unique<std::string>(current);
		return &m_history[++m_position];
	}
	else
	{
		return nullptr;
	}
}


std::string const *command_history::next(std::string const &current)
{
	if (0 < m_position)
	{
		return &m_history[--m_position];
	}
	else if (!m_position && m_current && (m_history.front() != *m_current))
	{
		--m_position;
		return m_current.get();
	}
	else
	{
		return nullptr;
	}
}


void command_history::edit()
{
	if (!m_position)
		--m_position;
}


void command_history::reset()
{
	m_position = -1;
	m_current.reset();
}


void command_history::clear()
{
	m_position = -1;
	m_current.reset();
	m_history.clear();
}


void command_history::restore_configuration_from_node(util::xml::data_node const &node)
{
	clear();
	util::xml::data_node const *const historynode = node.get_child(NODE_WINDOW_HISTORY);
	if (historynode)
	{
		util::xml::data_node const *itemnode = historynode->get_child(NODE_HISTORY_ITEM);
		while (itemnode)
		{
			if (itemnode->get_value() && *itemnode->get_value())
			{
				while (m_history.size() >= CAPACITY)
					m_history.pop_back();
				m_history.push_front(itemnode->get_value());
			}
			itemnode = itemnode->get_next_sibling(NODE_HISTORY_ITEM);
		}
	}
}


void command_history::save_configuration_to_node(util::xml::data_node &node)
{
	util::xml::data_node *const historynode = node.add_child(NODE_WINDOW_HISTORY, nullptr);
	if (historynode)
	{
		for (auto it = m_history.crbegin(); m_history.crend() != it; ++it)
			historynode->add_child(NODE_HISTORY_ITEM, it->c_str());
	}
}


history_entry::history_entry() :
	m_entry(gtk_entry_new()),
	m_keys(gtk_event_controller_key_new()),
	m_setting(false)
{
	g_object_ref_sink(m_entry);
	g_signal_connect(m_entry, "activate", G_CALLBACK(on_activate), this);
	g_signal_connect(m_entry, "changed", G_CALLBACK(on_changed), this);

	// see the keys before the entry's text widget does
	gtk_event_controller_set_propagation_phase(m_keys, GTK_PHASE_CAPTURE);
	g_signal_connect(m_keys, "key-pressed", G_CALLBACK(on_key_pressed), this);
	gtk_widget_add_controller(m_entry, m_keys);
}


history_entry::~history_entry()
{
	g_signal_handlers_disconnect_by_data(m_keys, this);
	g_signal_handlers_disconnect_by_data(m_entry, this);
	g_object_unref(m_entry);
}


std::string history_entry::text() const
{
	return gtk_editable_get_text(GTK_EDITABLE(m_entry));
}


void history_entry::set_text(std::string const &text)
{
	m_setting = true;
	gtk_editable_set_text(GTK_EDITABLE(m_entry), text.c_str());
	m_setting = false;
}


void history_entry::select_all()
{
	gtk_editable_select_region(GTK_EDITABLE(m_entry), 0, -1);
}


void history_entry::move_to_end()
{
	gtk_editable_set_position(GTK_EDITABLE(m_entry), -1);
}


void history_entry::on_activate(GtkEntry *entry, gpointer user_data)
{
	history_entry &self = *static_cast<history_entry *>(user_data);
	if (self.on_return)
		self.on_return();
}


void history_entry::on_changed(GtkEditable *editable, gpointer user_data)
{
	history_entry &self = *static_cast<history_entry *>(user_data);
	if (!self.m_setting && self.on_edited)
		self.on_edited();
}


gboolean history_entry::on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	history_entry &self = *static_cast<history_entry *>(user_data);
	return (self.on_key && self.on_key(keyval)) ? TRUE : FALSE;
}

} // namespace osd::debugger::gtk
