// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "memorywindow.h"

#include "memoryviewaccess.h"

#include "debugger.h"
#include "debug/debugcon.h"

#include "util/xmlfile.h"


namespace osd::debugger::gtk {

namespace mva = memory_view_access;

memory_window::memory_window(debugger_gtk &debugger, window_gtk *parent) :
	window_gtk(debugger, parent),
	m_input_edit(),
	m_memory_combo_box(gtk_drop_down_new(nullptr, nullptr)),
	m_input_history()
{
	set_title("Debug: Memory View");

	if (parent)
		gtk_window_set_default_size(window(), 800, 400);

	// The input edit
	m_input_edit.on_return = [this] () { expression_submitted(); };
	m_input_edit.on_edited = [this] () { expression_edited(); };
	m_input_edit.on_key = [this] (guint keyval) { return input_key(keyval); };
	gtk_widget_set_hexpand(m_input_edit.widget(), TRUE);

	// The memory space combo box
	g_object_ref_sink(m_memory_combo_box);
	gtk_widget_set_size_request(m_memory_combo_box, 300, -1);
	g_signal_connect(m_memory_combo_box, "notify::selected", G_CALLBACK(on_region_selected), this);

	// The main memory window
	m_mem_table = std::make_unique<debugger_mem_view>(DVT_MEMORY, m_machine);

	// Layout
	GtkWidget *const sub_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_set_margin_start(sub_layout, 2);
	gtk_widget_set_margin_top(sub_layout, 2);
	gtk_widget_set_margin_end(sub_layout, 2);
	gtk_widget_set_margin_bottom(sub_layout, 2);
	gtk_box_append(GTK_BOX(sub_layout), m_input_edit.widget());
	gtk_box_append(GTK_BOX(sub_layout), m_memory_combo_box);

	GtkWidget *const v_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_widget_set_margin_start(v_layout, 2);
	gtk_widget_set_margin_top(v_layout, 2);
	gtk_widget_set_margin_end(v_layout, 2);
	gtk_widget_set_margin_bottom(v_layout, 2);
	gtk_box_append(GTK_BOX(v_layout), sub_layout);
	gtk_widget_set_vexpand(m_mem_table->widget(), TRUE);
	gtk_box_append(GTK_BOX(v_layout), m_mem_table->widget());

	set_content(v_layout);
	gtk_window_set_focus(window(), m_input_edit.widget());

	// Menu bars

	// Create a data format group
	m_data_format = add_radio_action(
			"dataformat",
			int(mva::data_format::HEX_8BIT),
			[this] (int format) { format_changed(format); });

	// Create an address display group
	m_address_group = add_radio_action(
			"addressmode",
			0,
			[this] (int physical) { address_changed(physical != 0); });

	// Create an address radix group
	m_radix_group = add_radio_action(
			"radix",
			16,
			[this] (int radix) { radix_changed(radix); });

	// Create a reverse view radio
	m_reverse_act = add_toggle_action(
			"reverse",
			false,
			[this] (bool reverse) { reverse_changed(reverse); });

	// Create increase and decrease bytes-per-line actions
	add_action("increase-bpl", [this] () { increase_bytes_per_line(); });
	add_action("decrease-bpl", [this] () { decrease_bytes_per_line(); });

	// Assemble the options menu
	GMenu *const options_menu = add_menu("_Options");

	GMenu *const format_section = g_menu_new();
	append_item(format_section, "1-byte Chunks (Hex)", "dataformat", "<Control>1", g_variant_new_int32(int(mva::data_format::HEX_8BIT)));
	append_item(format_section, "2-byte Chunks (Hex)", "dataformat", "<Control>2", g_variant_new_int32(int(mva::data_format::HEX_16BIT)));
	append_item(format_section, "4-byte Chunks (Hex)", "dataformat", "<Control>4", g_variant_new_int32(int(mva::data_format::HEX_32BIT)));
	append_item(format_section, "8-byte Chunks (Hex)", "dataformat", "<Control>8", g_variant_new_int32(int(mva::data_format::HEX_64BIT)));
	append_item(format_section, "1-byte Chunks (Octal)", "dataformat", "<Control>3", g_variant_new_int32(int(mva::data_format::OCTAL_8BIT)));
	append_item(format_section, "2-byte Chunks (Octal)", "dataformat", "<Control>5", g_variant_new_int32(int(mva::data_format::OCTAL_16BIT)));
	append_item(format_section, "4-byte Chunks (Octal)", "dataformat", "<Control>7", g_variant_new_int32(int(mva::data_format::OCTAL_32BIT)));
	append_item(format_section, "8-byte Chunks (Octal)", "dataformat", "<Control>9", g_variant_new_int32(int(mva::data_format::OCTAL_64BIT)));
	append_item(format_section, "32-bit Floating Point", "dataformat", "<Control><Shift>f", g_variant_new_int32(int(mva::data_format::FLOAT_32BIT)));
	append_item(format_section, "64-bit Floating Point", "dataformat", "<Control><Shift>d", g_variant_new_int32(int(mva::data_format::FLOAT_64BIT)));
	append_item(format_section, "80-bit Floating Point", "dataformat", "<Control><Shift>e", g_variant_new_int32(int(mva::data_format::FLOAT_80BIT)));
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(format_section));
	g_object_unref(format_section);

	GMenu *const address_section = g_menu_new();
	append_item(address_section, "Logical Addresses", "addressmode", "<Control>g", g_variant_new_int32(0));
	append_item(address_section, "Physical Addresses", "addressmode", "<Control>y", g_variant_new_int32(1));
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(address_section));
	g_object_unref(address_section);

	GMenu *const radix_section = g_menu_new();
	append_item(radix_section, "Hexadecimal Addresses", "radix", "<Control><Shift>h", g_variant_new_int32(16));
	append_item(radix_section, "Decimal Addresses", "radix", nullptr, g_variant_new_int32(10));
	append_item(radix_section, "Octal Addresses", "radix", "<Control><Shift>o", g_variant_new_int32(8));
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(radix_section));
	g_object_unref(radix_section);

	GMenu *const reverse_section = g_menu_new();
	append_item(reverse_section, "Reverse View", "reverse", "<Control>r");
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(reverse_section));
	g_object_unref(reverse_section);

	GMenu *const bpl_section = g_menu_new();
	append_item(bpl_section, "Increase Bytes Per Line", "increase-bpl", "<Control>p");
	append_item(bpl_section, "Decrease Bytes Per Line", "decrease-bpl", "<Control>o");
	g_menu_append_section(options_menu, nullptr, G_MENU_MODEL(bpl_section));
	g_object_unref(bpl_section);

	// Initialize
	populate_combo_box();

	// Set to the current CPU's memory view
	set_to_current_cpu();
}


memory_window::~memory_window()
{
	g_signal_handlers_disconnect_by_data(m_memory_combo_box, this);
	g_object_unref(m_memory_combo_box);
	m_mem_table.reset();
}


bool memory_window::select_space(address_space &space)
{
	int const index = mva::source_index_for_space(*m_mem_table->view(), space);
	if (0 > index)
		return false;
	if (m_mem_table->source_index() != index)
		set_region_index(index);
	return true;
}


void memory_window::debug_act_open_memory()
{
	memory_window *const foo = new memory_window(m_debugger, this);
	foo->set_region_index(m_mem_table->source_index());
	foo->m_input_edit.set_text(mva::expression(*m_mem_table->view()));
	foo->expression_submitted();
}


void memory_window::restore_configuration(util::xml::data_node const &node)
{
	window_gtk::restore_configuration(node);

	debug_view &mem_view = *m_mem_table->view();

	auto const region = node.get_attribute_int(ATTR_WINDOW_MEMORY_REGION, m_mem_table->source_index());
	if ((0 <= region) && (g_list_model_get_n_items(gtk_drop_down_get_model(GTK_DROP_DOWN(m_memory_combo_box))) > region))
		set_region_index(region);

	auto const reverse = node.get_attribute_int(ATTR_WINDOW_MEMORY_REVERSE_COLUMNS, mva::reverse(mem_view) ? 1 : 0);
	if (mva::reverse(mem_view) != bool(reverse))
	{
		mva::set_reverse(mem_view, bool(reverse));
		set_toggle_checked(m_reverse_act, bool(reverse));
	}

	auto const mode = node.get_attribute_int(ATTR_WINDOW_MEMORY_ADDRESS_MODE, mva::physical(mem_view) ? 1 : 0);
	if ((0 == mode) || (1 == mode))
		trigger_radio(m_address_group, mode);

	auto const radix = node.get_attribute_int(ATTR_WINDOW_MEMORY_ADDRESS_RADIX, mva::address_radix(mem_view));
	if ((16 == radix) || (10 == radix) || (8 == radix))
		trigger_radio(m_radix_group, radix);

	auto const format = node.get_attribute_int(ATTR_WINDOW_MEMORY_DATA_FORMAT, mva::get_data_format(mem_view));
	if (mva::is_data_format(format))
		trigger_radio(m_data_format, format);

	auto const chunks = node.get_attribute_int(ATTR_WINDOW_MEMORY_ROW_CHUNKS, mva::chunks_per_row(mem_view));
	mva::set_chunks_per_row(mem_view, chunks);

	util::xml::data_node const *const expression = node.get_child(NODE_WINDOW_EXPRESSION);
	if (expression && expression->get_value())
	{
		m_input_edit.set_text(expression->get_value());
		expression_submitted();
	}

	m_mem_table->restore_configuration_from_node(node);
	m_input_history.restore_configuration_from_node(node);
}


void memory_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_MEMORY_VIEWER);

	debug_view &mem_view = *m_mem_table->view();
	node.set_attribute_int(ATTR_WINDOW_MEMORY_REGION, m_mem_table->source_index());
	node.set_attribute_int(ATTR_WINDOW_MEMORY_REVERSE_COLUMNS, mva::reverse(mem_view) ? 1 : 0);
	node.set_attribute_int(ATTR_WINDOW_MEMORY_ADDRESS_MODE, mva::physical(mem_view) ? 1 : 0);
	node.set_attribute_int(ATTR_WINDOW_MEMORY_ADDRESS_RADIX, mva::address_radix(mem_view));
	node.set_attribute_int(ATTR_WINDOW_MEMORY_DATA_FORMAT, mva::get_data_format(mem_view));
	node.set_attribute_int(ATTR_WINDOW_MEMORY_ROW_CHUNKS, mva::chunks_per_row(mem_view));
	node.add_child(NODE_WINDOW_EXPRESSION, mva::expression(mem_view).c_str());

	m_mem_table->save_configuration_to_node(node);
	m_input_history.save_configuration_to_node(node);
}


void memory_window::on_region_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
	guint const selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
	static_cast<memory_window *>(user_data)->memory_region_changed((GTK_INVALID_LIST_POSITION == selected) ? -1 : int(selected));
}


void memory_window::set_region_index(int index)
{
	gtk_drop_down_set_selected(GTK_DROP_DOWN(m_memory_combo_box), (0 <= index) ? guint(index) : GTK_INVALID_LIST_POSITION);
}


void memory_window::memory_region_changed(int index)
{
	if ((0 <= index) && (index < m_mem_table->view()->source_count()))
	{
		m_mem_table->view()->set_source(*m_mem_table->view()->source(index));
		m_mem_table->update();

		// Update the data format radio buttons to the memory region's default
		debug_view &mem_view = *m_mem_table->view();
		set_radio_checked(m_data_format, mva::get_data_format(mem_view));
		set_radio_checked(m_radix_group, mva::address_radix(mem_view));
	}
}


bool memory_window::input_key(guint keyval)
{
	if (GDK_KEY_Escape == keyval)
	{
		m_input_edit.set_text(mva::expression(*m_mem_table->view()));
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


void memory_window::expression_submitted()
{
	std::string const expression = m_input_edit.text();
	mva::set_expression(*m_mem_table->view(), expression);
	m_input_edit.select_all();

	// Add history
	if (!expression.empty())
		m_input_history.add(expression);
}


void memory_window::expression_edited()
{
	m_input_history.edit();
}


void memory_window::format_changed(int format)
{
	mva::set_data_format(*m_mem_table->view(), format);
	m_mem_table->update();
}


void memory_window::address_changed(bool physical)
{
	mva::set_physical(*m_mem_table->view(), physical);
	m_mem_table->update();
}


void memory_window::radix_changed(int radix)
{
	mva::set_address_radix(*m_mem_table->view(), radix);
	m_mem_table->update();
}


void memory_window::reverse_changed(bool reverse)
{
	mva::set_reverse(*m_mem_table->view(), reverse);
	m_mem_table->update();
}


void memory_window::increase_bytes_per_line()
{
	debug_view &mem_view = *m_mem_table->view();
	mva::set_chunks_per_row(mem_view, mva::chunks_per_row(mem_view) + 1);
	m_mem_table->update();
}


void memory_window::decrease_bytes_per_line()
{
	debug_view &mem_view = *m_mem_table->view();
	mva::set_chunks_per_row(mem_view, mva::chunks_per_row(mem_view) - 1);
	m_mem_table->update();
}


void memory_window::populate_combo_box()
{
	if (!m_mem_table)
		return;

	GtkStringList *const list = gtk_string_list_new(nullptr);
	for (auto &source : m_mem_table->view()->source_list())
		gtk_string_list_append(list, source->name());
	gtk_drop_down_set_model(GTK_DROP_DOWN(m_memory_combo_box), G_LIST_MODEL(list));
	g_object_unref(list);
}


void memory_window::set_to_current_cpu()
{
	device_t *cur_cpu = m_machine.debugger().console().get_visible_cpu();
	if (cur_cpu)
	{
		const debug_view_source *source = m_mem_table->view()->source_for_device(cur_cpu);
		if (source)
		{
			const int list_index = m_mem_table->view()->source_index(*source);
			set_region_index(list_index);
		}
	}
}


//=========================================================================
//  debugger_mem_view
//=========================================================================
debugger_mem_view::debugger_mem_view(debug_view_type type, running_machine &machine) :
	debugger_view(type, machine),
	m_copy_last_pc_action(g_simple_action_new("copy-last-pc", nullptr))
{
	g_signal_connect(m_copy_last_pc_action, "activate", G_CALLBACK(on_copy_last_pc), this);
	g_action_map_add_action(G_ACTION_MAP(actions()), G_ACTION(m_copy_last_pc_action));
	g_object_unref(m_copy_last_pc_action);
}


debugger_mem_view::~debugger_mem_view()
{
	g_signal_handlers_disconnect_by_data(m_copy_last_pc_action, this);
}


void debugger_mem_view::add_items_to_context_menu(GMenu *menu)
{
	debugger_view::add_items_to_context_menu(menu);

	if (view()->cursor_visible())
	{
		mva::last_pc const last_pc = mva::last_pc_at_cursor(*view());
		if (last_pc.has_space)
		{
			m_last_pc = last_pc.text;

			GMenu *const section = g_menu_new();
			g_menu_append(section, m_last_pc.c_str(), "view.copy-last-pc");
			g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
			g_object_unref(section);
			g_simple_action_set_enabled(m_copy_last_pc_action, last_pc.known);
		}
	}
}


void debugger_mem_view::on_copy_last_pc(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	debugger_mem_view &self = *static_cast<debugger_mem_view *>(user_data);
	gdk_clipboard_set_text(gtk_widget_get_clipboard(self.area()), self.m_last_pc.c_str());
}

} // namespace osd::debugger::gtk
