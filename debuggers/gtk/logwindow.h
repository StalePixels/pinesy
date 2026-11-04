// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_LOGWINDOW_H
#define MAME_DEBUGGER_GTK_LOGWINDOW_H

#pragma once

#include "debuggerview.h"
#include "windowgtk.h"

#include <memory>


namespace osd::debugger::gtk {

//============================================================
//  The Log Window.
//============================================================
class log_window : public window_gtk
{
public:
	log_window(debugger_gtk &debugger, window_gtk *parent = nullptr);
	virtual ~log_window();

	virtual void restore_configuration(util::xml::data_node const &node) override;

protected:
	virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	// Widgets
	std::unique_ptr<debugger_view> m_log_view;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_LOGWINDOW_H
