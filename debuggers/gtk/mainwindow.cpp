// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "mainwindow.h"

#include "debugger.h"
#include "debug/debugcon.h"
#include "debug/debugcpu.h"
#include "debug/dvdisasm.h"
#include "debug/points.h"

#include "util/xmlfile.h"


namespace osd::debugger::gtk {

namespace {

struct mount_request
{
	std::weak_ptr<main_window *> alive;
	int index;
};

} // anonymous namespace


main_window::main_window(debugger_gtk &debugger, window_gtk *parent) :
	window_gtk(debugger, nullptr),
	m_input_edit(),
	m_state_split(gtk_paned_new(GTK_ORIENTATION_HORIZONTAL)),
	m_dasm_split(gtk_paned_new(GTK_ORIENTATION_VERTICAL)),
	m_images_menu(nullptr),
	m_dialog_cancel(g_cancellable_new()),
	m_alive(std::make_shared<main_window *>(this)),
	m_input_history()
{
	gtk_window_set_default_size(window(), 1000, 600);

	//
	// The main frame and its input and log widgets
	//

	// The input line
	m_input_edit.on_return = [this] () { execute_command_slot(); };
	m_input_edit.on_edited = [this] () { command_edited_slot(); };
	m_input_edit.on_key = [this] (guint keyval) { return input_key(keyval); };

	// The log view
	m_console_view = std::make_unique<debugger_view>(DVT_CONSOLE, m_machine);
	m_console_view->set_focusable(false);
	m_console_view->set_prefer_bottom(true);
	gtk_widget_set_vexpand(m_console_view->widget(), TRUE);

	GtkWidget *const console_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_widget_set_margin_start(console_frame, 4);
	gtk_widget_set_margin_end(console_frame, 4);
	gtk_widget_set_margin_bottom(console_frame, 2);
	gtk_box_append(GTK_BOX(console_frame), m_console_view->widget());
	gtk_box_append(GTK_BOX(console_frame), m_input_edit.widget());

	//
	// Options Menu
	//
	// Create three commands
	m_breakpoint_toggle_act = add_action("toggle-breakpoint", [this] () { toggle_breakpoint_at_cursor(); });
	m_breakpoint_enable_act = add_action("enable-breakpoint", [this] () { enable_breakpoint_at_cursor(); });
	m_run_to_cursor_act = add_action("run-to-cursor", [this] () { run_to_cursor(); });

	// Right bar options
	m_right_bar_group = add_radio_action(
			"rightbar",
			DASM_RIGHTCOL_RAW,
			[this] (int column) { right_bar_changed(column); });

	// Assemble the options menu
	GMenu *const options_menu = add_menu("_Options");
	m_breakpoint_toggle_text = "Toggle Breakpoint at Cursor";
	m_breakpoint_enable_text = "Disable Breakpoint at Cursor";
	m_breakpoint_section = g_menu_new();
	append_item(m_breakpoint_section, m_breakpoint_toggle_text.c_str(), "toggle-breakpoint", "F9");
	append_item(m_breakpoint_section, m_breakpoint_enable_text.c_str(), "enable-breakpoint", "<Shift>F9");
	append_item(m_breakpoint_section, "Run to Cursor", "run-to-cursor", "F4");
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(m_breakpoint_section));
	g_object_unref(m_breakpoint_section);

	GMenu *const right_bar_section = g_menu_new();
	append_item(right_bar_section, "Raw Opcodes", "rightbar", "<Control>r", g_variant_new_int32(DASM_RIGHTCOL_RAW));
	append_item(right_bar_section, "Encrypted Opcodes", "rightbar", "<Control>e", g_variant_new_int32(DASM_RIGHTCOL_ENCRYPTED));
	append_item(right_bar_section, "Comments", "rightbar", "<Control>n", g_variant_new_int32(DASM_RIGHTCOL_COMMENTS));
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(right_bar_section));
	g_object_unref(right_bar_section);

	//
	// Images menu
	//
	image_interface_enumerator image_iter_test(m_machine.root_device());
	if (image_iter_test.first())
		create_images_menu();

	//
	// Pane menu: the processor pane is on the left at full height, the
	// disassembly pane is above the console
	//
	GMenu *const dock_menu = add_menu("Doc_ks");

	// The processor pane
	m_proc_view = std::make_unique<debugger_view>(DVT_STATE, m_machine);
	m_proc_view->set_focusable(false);
	m_proc_view->set_margins(4, 0, 4, 2);
	gtk_widget_set_size_request(m_proc_view->widget(), 150, 300);
	add_toggle_action(
			"dock-processor",
			true,
			[this] (bool visible) { gtk_widget_set_visible(m_proc_view->widget(), visible ? TRUE : FALSE); });
	append_item(dock_menu, "processor", "dock-processor");

	// The disassembly pane
	m_dasm_view = std::make_unique<debugger_view>(DVT_DISASSEMBLY, m_machine);
	m_dasm_view->set_margins(4, 0, 4, 0);
	gtk_widget_set_size_request(m_dasm_view->widget(), 150, 150);
	m_dasm_view->set_updated_callback([this] () { dasm_view_updated(); });

	// Force a recompute of the disassembly region
	m_dasm_view->view<debug_view_disasm>()->set_expression("curpc");

	add_toggle_action(
			"dock-dasm",
			true,
			[this] (bool visible) { gtk_widget_set_visible(m_dasm_view->widget(), visible ? TRUE : FALSE); });
	append_item(dock_menu, "dasm", "dock-dasm");

	gtk_paned_set_start_child(GTK_PANED(m_dasm_split), m_dasm_view->widget());
	gtk_paned_set_end_child(GTK_PANED(m_dasm_split), console_frame);
	gtk_paned_set_resize_start_child(GTK_PANED(m_dasm_split), FALSE);
	gtk_paned_set_shrink_start_child(GTK_PANED(m_dasm_split), FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(m_dasm_split), FALSE);
	gtk_paned_set_position(GTK_PANED(m_dasm_split), 200);

	gtk_paned_set_start_child(GTK_PANED(m_state_split), m_proc_view->widget());
	gtk_paned_set_end_child(GTK_PANED(m_state_split), m_dasm_split);
	gtk_paned_set_resize_start_child(GTK_PANED(m_state_split), FALSE);
	gtk_paned_set_shrink_start_child(GTK_PANED(m_state_split), FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(m_state_split), FALSE);
	gtk_paned_set_position(GTK_PANED(m_state_split), 200);

	set_content(m_state_split);
	gtk_window_set_focus(window(), m_input_edit.widget());
}


main_window::~main_window()
{
	g_cancellable_cancel(m_dialog_cancel);
	g_object_unref(m_dialog_cancel);
	m_alive.reset();

	for (GMenu *const menu : m_image_menus)
		g_object_unref(menu);

	m_dasm_view.reset();
	m_proc_view.reset();
	m_console_view.reset();
}


void main_window::set_processor(device_t *processor)
{
	// Cpu swap
	m_proc_view->view()->set_source(*m_proc_view->view()->source_for_device(processor));
	m_dasm_view->view()->set_source(*m_dasm_view->view()->source_for_device(processor));

	// Scrollbar refresh
	m_dasm_view->update();

	// Window title
	set_title(string_format("Debug: %s - %s '%s'", m_machine.system().name, processor->name(), processor->tag()).c_str());
}


void main_window::restore_configuration(util::xml::data_node const &node)
{
	window_gtk::restore_configuration(node);

	debug_view_disasm &dasmview = *m_dasm_view->view<debug_view_disasm>();

	auto const rightbar = node.get_attribute_int(ATTR_WINDOW_DISASSEMBLY_RIGHT_COLUMN, dasmview.right_column());
	if ((DASM_RIGHTCOL_RAW == rightbar) || (DASM_RIGHTCOL_ENCRYPTED == rightbar) || (DASM_RIGHTCOL_COMMENTS == rightbar))
		trigger_radio(m_right_bar_group, rightbar);

	m_dasm_view->restore_configuration_from_node(node);
	m_input_history.restore_configuration_from_node(node);
}


void main_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_CONSOLE);

	debug_view_disasm &dasmview = *m_dasm_view->view<debug_view_disasm>();
	node.set_attribute_int(ATTR_WINDOW_DISASSEMBLY_RIGHT_COLUMN, dasmview.right_column());

	m_dasm_view->save_configuration_to_node(node);
	m_input_history.save_configuration_to_node(node);
}


// Used to intercept the user clicking 'X' in the upper corner
bool main_window::close_request()
{
	// Don't actually close the window - it will be brought back on user break
	debug_act_run_and_hide();
	return true;
}


// Used to intercept the user hitting the up arrow in the input widget
bool main_window::input_key(guint keyval)
{
	if (GDK_KEY_Escape == keyval)
	{
		m_input_edit.clear();
		m_input_history.reset();
		return true;
	}
	else if ((GDK_KEY_Up == keyval) || (GDK_KEY_KP_Up == keyval))
	{
		std::string const *const hist = m_input_history.previous(m_input_edit.text());
		if (hist)
		{
			m_input_edit.set_text(*hist);
			m_input_edit.move_to_end();
		}
		return true;
	}
	else if ((GDK_KEY_Down == keyval) || (GDK_KEY_KP_Down == keyval))
	{
		std::string const *const hist = m_input_history.next(m_input_edit.text());
		if (hist)
		{
			m_input_edit.set_text(*hist);
			m_input_edit.move_to_end();
		}
		return true;
	}
	else if (GDK_KEY_KP_Enter == keyval)
	{
		execute_command(false);
		return true;
	}
	else
	{
		return false;
	}
}


void main_window::toggle_breakpoint_at_cursor()
{
	debug_view_disasm *const dasm_view = m_dasm_view->view<debug_view_disasm>();
	if (dasm_view->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasm_view->source()->device()))
	{
		offs_t const address = dasm_view->selected_address();
		device_debug *const cpuinfo = dasm_view->source()->device()->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address);

		// If none exists, add a new one
		std::string command;
		if (!bp)
			command = string_format("bpset 0x%X", address);
		else
			command = string_format("bpclear 0x%X", bp->index());
		m_machine.debugger().console().execute_command(command, true);
		m_machine.debug_view().update_all();
		m_machine.debugger().refresh_display();
	}
}


void main_window::enable_breakpoint_at_cursor()
{
	debug_view_disasm *const dasm_view = m_dasm_view->view<debug_view_disasm>();
	if (dasm_view->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasm_view->source()->device()))
	{
		offs_t const address = dasm_view->selected_address();
		device_debug *const cpuinfo = dasm_view->source()->device()->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address);

		if (bp)
		{
			int32_t const bpindex = bp->index();
			std::string command = string_format(bp->enabled() ? "bpdisable 0x%X" : "bpenable 0x%X", bpindex);
			m_machine.debugger().console().execute_command(command, true);
			m_machine.debug_view().update_all();
			m_machine.debugger().refresh_display();
		}
	}
}


void main_window::run_to_cursor()
{
	debug_view_disasm *const dasm_view = m_dasm_view->view<debug_view_disasm>();
	if (dasm_view->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasm_view->source()->device()))
	{
		offs_t address = dasm_view->selected_address();
		std::string command = string_format("go 0x%X", address);
		m_machine.debugger().console().execute_command(command, true);
	}
}


void main_window::right_bar_changed(int column)
{
	debug_view_disasm *const dasm_view = m_dasm_view->view<debug_view_disasm>();
	dasm_view->set_right_column(disasm_right_column(column));
	m_dasm_view->update();
}


void main_window::execute_command_slot()
{
	execute_command(true);
}


void main_window::command_edited_slot()
{
	m_input_history.edit();
}


void main_window::execute_command(bool with_clear)
{
	std::string const command = m_input_edit.text();
	if (command.empty())
	{
		// A blank command is a "silent step"
		m_machine.debugger().console().get_visible_cpu()->debug()->single_step();
		m_input_history.reset();
	}
	else
	{
		// Send along the command
		m_machine.debugger().console().execute_command(command, true);

		// Add history
		m_input_history.add(command);

		// Clear out the text and reset the history pointer only if asked
		if (with_clear)
		{
			m_input_edit.clear();
			m_input_history.edit();
		}
	}
}


void main_window::mount_image(int index)
{
	image_interface_enumerator iter(m_machine.root_device());
	device_image_interface *img = iter.byindex(index);
	if (!img)
	{
		m_machine.debugger().console().printf("Something is wrong with the mount menu.\n");
		return;
	}

	// File dialog
	GtkFileDialog *const dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Select an image file");
	gtk_file_dialog_set_modal(dialog, TRUE);

	char *const current_dir = g_get_current_dir();
	GFile *const folder = g_file_new_for_path(current_dir);
	g_free(current_dir);
	gtk_file_dialog_set_initial_folder(dialog, folder);
	g_object_unref(folder);

	GtkFileFilter *const filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All files (*.*)");
	gtk_file_filter_add_pattern(filter, "*.*");
	GListStore *const filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	g_object_unref(filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	g_object_unref(filters);

	gtk_file_dialog_open(dialog, window(), m_dialog_cancel, &main_window::on_mount_ready, new mount_request{ m_alive, index });
	g_object_unref(dialog);
}


void main_window::on_mount_ready(GObject *source, GAsyncResult *result, gpointer user_data)
{
	std::unique_ptr<mount_request> const request(static_cast<mount_request *>(user_data));

	// a dismissed dialog gives an empty file name, as QFileDialog does
	GError *error = nullptr;
	GFile *const file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (error)
		g_error_free(error);

	std::string filename;
	if (file)
	{
		char *const path = g_file_get_path(file);
		if (path)
		{
			filename = path;
			g_free(path);
		}
		g_object_unref(file);
	}

	std::shared_ptr<main_window *> const self = request->alive.lock();
	if (self)
		(*self)->mount_image_file(request->index, filename);
}


void main_window::mount_image_file(int index, std::string const &filename)
{
	image_interface_enumerator iter(m_machine.root_device());
	device_image_interface *img = iter.byindex(index);
	if (!img)
	{
		m_machine.debugger().console().printf("Something is wrong with the mount menu.\n");
		return;
	}

	auto [err, message] = img->load(filename);
	if (err)
	{
		m_machine.debugger().console().printf("Image could not be mounted: %s\n", !message.empty() ? message : err.message());
		return;
	}

	// Activate the unmount menu option
	g_simple_action_set_enabled(m_unmount_acts[index], TRUE);

	// Set the mount name
	set_image_title(index, img->filename());

	m_machine.debugger().console().printf("Image %s mounted successfully.\n", filename);
}


void main_window::unmount_image(int index)
{
	image_interface_enumerator iter(m_machine.root_device());
	device_image_interface *img = iter.byindex(index);

	img->unload();

	// Deactivate the unmount menu option
	g_simple_action_set_enabled(m_unmount_acts[index], FALSE);

	// Set the mount name
	set_image_title(index, "[empty slot]");

	m_machine.debugger().console().printf("Image successfully unmounted.\n");
}


void main_window::set_image_title(int index, char const *name)
{
	std::string &title = m_image_titles[index];
	std::string::size_type const pos = title.rfind(" : ");
	title.resize((std::string::npos != pos) ? pos : 0);
	title += " : ";
	title += name;

	g_menu_remove(m_images_menu, index);
	g_menu_insert_submenu(m_images_menu, index, escape_mnemonic(title).c_str(), G_MENU_MODEL(m_image_menus[index]));
}


void main_window::dasm_view_updated()
{
	debug_view_disasm *const dasm_view = m_dasm_view->view<debug_view_disasm>();
	bool const have_cursor = dasm_view->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasm_view->source()->device());
	bool have_breakpoint = false;
	bool breakpoint_enabled = false;
	if (have_cursor)
	{
		offs_t const address = dasm_view->selected_address();
		device_t *const device = dasm_view->source()->device();
		device_debug *const cpuinfo = device->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address);

		if (bp)
		{
			have_breakpoint = true;
			breakpoint_enabled = bp->enabled();
		}
	}

	char const *const toggle_text = have_breakpoint ? "Clear Breakpoint at Cursor" : have_cursor ? "Set Breakpoint at Cursor" : "Toggle Breakpoint at Cursor";
	char const *const enable_text = (!have_breakpoint || breakpoint_enabled) ? "Disable Breakpoint at Cursor" : "Enable Breakpoint at Cursor";
	if (m_breakpoint_toggle_text != toggle_text)
	{
		m_breakpoint_toggle_text = toggle_text;
		replace_item(m_breakpoint_section, 0, toggle_text, "toggle-breakpoint", "F9");
	}
	if (m_breakpoint_enable_text != enable_text)
	{
		m_breakpoint_enable_text = enable_text;
		replace_item(m_breakpoint_section, 1, enable_text, "enable-breakpoint", "<Shift>F9");
	}
	g_simple_action_set_enabled(m_breakpoint_toggle_act, have_cursor);
	g_simple_action_set_enabled(m_breakpoint_enable_act, have_breakpoint);
	g_simple_action_set_enabled(m_run_to_cursor_act, have_cursor);
}


void main_window::debug_act_close()
{
	m_machine.schedule_exit();
}


void main_window::create_images_menu()
{
	m_images_menu = add_menu("_Images");

	int interface_index = 0;
	for (device_image_interface &img : image_interface_enumerator(m_machine.root_device()))
	{
		std::string const menu_name = string_format("%s : %s", img.device().name(), img.exists() ? img.filename() : "[empty slot]");

		std::string const mount_name = string_format("mount-%d", interface_index);
		std::string const unmount_name = string_format("unmount-%d", interface_index);
		add_action(mount_name.c_str(), [this, interface_index] () { mount_image(interface_index); });
		GSimpleAction *const unmount_act = add_action(unmount_name.c_str(), [this, interface_index] () { unmount_image(interface_index); });

		if (!img.exists())
			g_simple_action_set_enabled(unmount_act, FALSE);

		GMenu *const interface_menu = g_menu_new();
		append_item(interface_menu, "Mount...", mount_name.c_str());
		append_item(interface_menu, "Unmount", unmount_name.c_str());
		g_menu_append_submenu(m_images_menu, escape_mnemonic(menu_name).c_str(), G_MENU_MODEL(interface_menu));

		m_image_menus.emplace_back(interface_menu);
		m_image_titles.emplace_back(menu_name);
		m_unmount_acts.emplace_back(unmount_act);

		// TODO: Cassette operations

		interface_index++;
	}
}

} // namespace osd::debugger::gtk
