// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "logwindow.h"

#include "util/xmlfile.h"


namespace osd::debugger::gtk {

log_window::log_window(debugger_gtk &debugger, window_gtk *parent) :
	window_gtk(debugger, parent)
{
	set_title("Debug: Machine Log");

	if (parent)
		gtk_window_set_default_size(window(), 800, 400);

	// The main log view
	m_log_view = std::make_unique<debugger_view>(DVT_LOG, m_machine);
	m_log_view->set_margins(2, 2, 2, 2);

	set_content(m_log_view->widget());
}


log_window::~log_window()
{
}


void log_window::restore_configuration(util::xml::data_node const &node)
{
	window_gtk::restore_configuration(node);

	m_log_view->restore_configuration_from_node(node);
}


void log_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_ERROR_LOG_VIEWER);

	m_log_view->save_configuration_to_node(node);
}

} // namespace osd::debugger::gtk
