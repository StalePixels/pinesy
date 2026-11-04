// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "dasmwindow.h"

#include "debugger.h"
#include "debug/debugcon.h"
#include "debug/debugcpu.h"
#include "debug/dvdisasm.h"
#include "debug/points.h"

#include "util/xmlfile.h"


namespace osd::debugger::gtk {

dasm_window::dasm_window(debugger_gtk &debugger, window_gtk *parent) :
	window_gtk(debugger, parent),
	m_input_edit(),
	m_cpu_combo_box(gtk_drop_down_new(nullptr, nullptr)),
	m_input_history()
{
	set_title("Debug: Disassembly View");

	if (parent)
		gtk_window_set_default_size(window(), 800, 400);

	// The input edit
	m_input_edit.on_return = [this] () { expression_submitted(); };
	m_input_edit.on_edited = [this] () { expression_edited(); };
	m_input_edit.on_key = [this] (guint keyval) { return input_key(keyval); };
	gtk_widget_set_hexpand(m_input_edit.widget(), TRUE);

	// The cpu combo box
	g_object_ref_sink(m_cpu_combo_box);
	gtk_widget_set_size_request(m_cpu_combo_box, 300, -1);
	g_signal_connect(m_cpu_combo_box, "notify::selected", G_CALLBACK(on_cpu_selected), this);

	// The main disasm window
	m_dasm_view = std::make_unique<debugger_view>(DVT_DISASSEMBLY, m_machine);
	m_dasm_view->set_updated_callback([this] () { dasm_view_updated(); });

	// Force a recompute of the disassembly region
	m_dasm_view->view<debug_view_disasm>()->set_expression("curpc");

	// Populate the combo box & set the proper CPU
	populate_combo_box();
	set_to_current_cpu();

	// Layout
	GtkWidget *const sub_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_set_margin_start(sub_layout, 2);
	gtk_widget_set_margin_top(sub_layout, 2);
	gtk_widget_set_margin_end(sub_layout, 2);
	gtk_widget_set_margin_bottom(sub_layout, 2);
	gtk_box_append(GTK_BOX(sub_layout), m_input_edit.widget());
	gtk_box_append(GTK_BOX(sub_layout), m_cpu_combo_box);

	GtkWidget *const v_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_widget_set_margin_start(v_layout, 2);
	gtk_widget_set_margin_top(v_layout, 2);
	gtk_widget_set_margin_end(v_layout, 2);
	gtk_widget_set_margin_bottom(v_layout, 2);
	gtk_box_append(GTK_BOX(v_layout), sub_layout);
	gtk_widget_set_vexpand(m_dasm_view->widget(), TRUE);
	gtk_box_append(GTK_BOX(v_layout), m_dasm_view->widget());

	set_content(v_layout);
	gtk_window_set_focus(window(), m_input_edit.widget());

	// Menu bars
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
}


dasm_window::~dasm_window()
{
	g_signal_handlers_disconnect_by_data(m_cpu_combo_box, this);
	g_object_unref(m_cpu_combo_box);
	m_dasm_view.reset();
}


void dasm_window::debug_act_open_dasm()
{
	dasm_window *const foo = new dasm_window(m_debugger, this);
	foo->set_cpu_index(m_dasm_view->source_index());
	foo->m_input_edit.set_text(m_dasm_view->view<debug_view_disasm>()->expression());
	foo->expression_submitted();
}


void dasm_window::restore_configuration(util::xml::data_node const &node)
{
	window_gtk::restore_configuration(node);

	debug_view_disasm &dasmview = *m_dasm_view->view<debug_view_disasm>();

	auto const cpu = node.get_attribute_int(ATTR_WINDOW_DISASSEMBLY_CPU, m_dasm_view->source_index());
	if ((0 <= cpu) && (g_list_model_get_n_items(gtk_drop_down_get_model(GTK_DROP_DOWN(m_cpu_combo_box))) > cpu))
		set_cpu_index(cpu);

	auto const rightbar = node.get_attribute_int(ATTR_WINDOW_DISASSEMBLY_RIGHT_COLUMN, dasmview.right_column());
	if ((DASM_RIGHTCOL_RAW == rightbar) || (DASM_RIGHTCOL_ENCRYPTED == rightbar) || (DASM_RIGHTCOL_COMMENTS == rightbar))
		trigger_radio(m_right_bar_group, rightbar);

	util::xml::data_node const *const expression = node.get_child(NODE_WINDOW_EXPRESSION);
	if (expression && expression->get_value())
	{
		m_input_edit.set_text(expression->get_value());
		expression_submitted();
	}

	m_dasm_view->restore_configuration_from_node(node);
	m_input_history.restore_configuration_from_node(node);
}


void dasm_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_DISASSEMBLY_VIEWER);

	debug_view_disasm &dasmview = *m_dasm_view->view<debug_view_disasm>();
	node.set_attribute_int(ATTR_WINDOW_DISASSEMBLY_CPU, m_dasm_view->source_index());
	node.set_attribute_int(ATTR_WINDOW_DISASSEMBLY_RIGHT_COLUMN, dasmview.right_column());
	node.add_child(NODE_WINDOW_EXPRESSION, dasmview.expression());

	m_dasm_view->save_configuration_to_node(node);
	m_input_history.save_configuration_to_node(node);
}


bool dasm_window::input_key(guint keyval)
{
	if (GDK_KEY_Escape == keyval)
	{
		m_input_edit.set_text(m_dasm_view->view<debug_view_disasm>()->expression());
		m_input_edit.select_all();
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
	else
	{
		return false;
	}
}


void dasm_window::on_cpu_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
	guint const selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
	static_cast<dasm_window *>(user_data)->cpu_changed((GTK_INVALID_LIST_POSITION == selected) ? -1 : int(selected));
}


void dasm_window::set_cpu_index(int index)
{
	gtk_drop_down_set_selected(GTK_DROP_DOWN(m_cpu_combo_box), (0 <= index) ? guint(index) : GTK_INVALID_LIST_POSITION);
}


void dasm_window::cpu_changed(int index)
{
	if ((0 <= index) && (index < m_dasm_view->view()->source_count()))
	{
		m_dasm_view->view()->set_source(*m_dasm_view->view()->source(index));
		m_dasm_view->update();
	}
}


void dasm_window::expression_submitted()
{
	std::string const expression = m_input_edit.text();
	m_dasm_view->view<debug_view_disasm>()->set_expression(expression);
	m_input_edit.select_all();

	// Add history
	if (!expression.empty())
		m_input_history.add(expression);
}


void dasm_window::expression_edited()
{
	m_input_history.edit();
}


void dasm_window::toggle_breakpoint_at_cursor()
{
	if (m_dasm_view->view()->cursor_visible())
	{
		offs_t const address = m_dasm_view->view<debug_view_disasm>()->selected_address();
		device_t *const device = m_dasm_view->view()->source()->device();
		device_debug *const cpuinfo = device->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address);

		// If none exists, add a new one
		if (!bp)
		{
			int32_t bpindex = cpuinfo->breakpoint_set(address);
			m_machine.debugger().console().printf("Breakpoint %X set\n", bpindex);
		}
		else
		{
			int32_t bpindex = bp->index();
			cpuinfo->breakpoint_clear(bpindex);
			m_machine.debugger().console().printf("Breakpoint %X cleared\n", bpindex);
		}
		m_machine.debug_view().update_all();
		m_machine.debugger().refresh_display();
	}
}


void dasm_window::enable_breakpoint_at_cursor()
{
	if (m_dasm_view->view()->cursor_visible())
	{
		offs_t const address = m_dasm_view->view<debug_view_disasm>()->selected_address();
		device_t *const device = m_dasm_view->view()->source()->device();
		device_debug *const cpuinfo = device->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address);

		if (bp)
		{
			cpuinfo->breakpoint_enable(bp->index(), !bp->enabled());
			m_machine.debugger().console().printf("Breakpoint %X %s\n", (uint32_t)bp->index(), bp->enabled() ? "enabled" : "disabled");
			m_machine.debug_view().update_all();
			m_machine.debugger().refresh_display();
		}
	}
}


void dasm_window::run_to_cursor()
{
	if (m_dasm_view->view()->cursor_visible())
	{
		offs_t const address = m_dasm_view->view<debug_view_disasm>()->selected_address();
		m_dasm_view->view()->source()->device()->debug()->go(address);
	}
}


void dasm_window::right_bar_changed(int column)
{
	debug_view_disasm *const dasm_view = m_dasm_view->view<debug_view_disasm>();
	dasm_view->set_right_column(disasm_right_column(column));
	m_dasm_view->update();
}


void dasm_window::dasm_view_updated()
{
	bool const have_cursor = m_dasm_view->view()->cursor_visible();
	bool have_breakpoint = false;
	bool breakpoint_enabled = false;
	if (have_cursor)
	{
		offs_t const address = m_dasm_view->view<debug_view_disasm>()->selected_address();
		device_t *const device = m_dasm_view->view()->source()->device();
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


void dasm_window::populate_combo_box()
{
	if (!m_dasm_view)
		return;

	GtkStringList *const list = gtk_string_list_new(nullptr);
	for (auto &source : m_dasm_view->view()->source_list())
		gtk_string_list_append(list, source->name());
	gtk_drop_down_set_model(GTK_DROP_DOWN(m_cpu_combo_box), G_LIST_MODEL(list));
	g_object_unref(list);
}


void dasm_window::set_to_current_cpu()
{
	device_t *cur_cpu = m_machine.debugger().console().get_visible_cpu();
	if (cur_cpu)
	{
		const debug_view_source *source = m_dasm_view->view()->source_for_device(cur_cpu);
		if (source)
		{
			const int list_index = m_dasm_view->view()->source_index(*source);
			set_cpu_index(list_index);
		}
	}
}

} // namespace osd::debugger::gtk
