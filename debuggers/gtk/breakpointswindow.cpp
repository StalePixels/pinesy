// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "breakpointswindow.h"

#include "util/xmlfile.h"


namespace osd::debugger::gtk {

breakpoints_window::breakpoints_window(debugger_gtk &debugger, window_gtk *parent) :
	window_gtk(debugger, parent),
	m_layout(gtk_box_new(GTK_ORIENTATION_VERTICAL, 3))
{
	set_title("Debug: All Breakpoints");

	if (parent)
		gtk_window_set_default_size(window(), 800, 400);

	// The main breakpoints view
	m_breakpoints_view = std::make_unique<debugger_view>(DVT_BREAK_POINTS, m_machine);
	gtk_widget_set_vexpand(m_breakpoints_view->widget(), TRUE);

	// Layout
	gtk_widget_set_margin_start(m_layout, 2);
	gtk_widget_set_margin_top(m_layout, 2);
	gtk_widget_set_margin_end(m_layout, 2);
	gtk_widget_set_margin_bottom(m_layout, 2);
	gtk_box_append(GTK_BOX(m_layout), m_breakpoints_view->widget());

	set_content(m_layout);

	// Menu bars
	m_type_group = add_radio_action("type", 0, [this] (int type) { type_changed(type); });

	// Assemble the options menu
	GMenu *const options_menu = add_menu("_Options");
	append_item(options_menu, "Breakpoints", "type", "<Control>1", g_variant_new_int32(0));
	append_item(options_menu, "Watchpoints", "type", "<Control>2", g_variant_new_int32(1));
	append_item(options_menu, "Registerpoints", "type", "<Control>3", g_variant_new_int32(2));
	append_item(options_menu, "Exceptionpoints", "type", "<Control>4", g_variant_new_int32(3));
}


breakpoints_window::~breakpoints_window()
{
}


void breakpoints_window::restore_configuration(util::xml::data_node const &node)
{
	window_gtk::restore_configuration(node);

	auto const type = node.get_attribute_int(ATTR_WINDOW_POINTS_TYPE, -1);
	if ((0 <= type) && (4 > type))
		trigger_radio(m_type_group, type);

	m_breakpoints_view->restore_configuration_from_node(node);
}


void breakpoints_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_POINTS_VIEWER);
	if (m_breakpoints_view)
	{
		switch (m_breakpoints_view->view()->type())
		{
		case DVT_BREAK_POINTS:
			node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 0);
			break;
		case DVT_WATCH_POINTS:
			node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 1);
			break;
		case DVT_REGISTER_POINTS:
			node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 2);
			break;
		case DVT_EXCEPTION_POINTS:
			node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 3);
			break;
		default:
			break;
		}
	}

	m_breakpoints_view->save_configuration_to_node(node);
}


void breakpoints_window::type_changed(int type)
{
	// Clean
	if (m_breakpoints_view)
		gtk_box_remove(GTK_BOX(m_layout), m_breakpoints_view->widget());
	m_breakpoints_view.reset();

	// Create
	switch (type)
	{
	case 0:
		m_breakpoints_view = std::make_unique<debugger_view>(DVT_BREAK_POINTS, m_machine);
		set_title("Debug: All Breakpoints");
		break;
	case 1:
		m_breakpoints_view = std::make_unique<debugger_view>(DVT_WATCH_POINTS, m_machine);
		set_title("Debug: All Watchpoints");
		break;
	case 2:
		m_breakpoints_view = std::make_unique<debugger_view>(DVT_REGISTER_POINTS, m_machine);
		set_title("Debug: All Registerpoints");
		break;
	case 3:
		m_breakpoints_view = std::make_unique<debugger_view>(DVT_EXCEPTION_POINTS, m_machine);
		set_title("Debug: All Exceptionpoints");
		break;
	}

	// Re-register
	gtk_widget_set_vexpand(m_breakpoints_view->widget(), TRUE);
	gtk_box_append(GTK_BOX(m_layout), m_breakpoints_view->widget());
}

} // namespace osd::debugger::gtk
