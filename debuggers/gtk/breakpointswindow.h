// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_BREAKPOINTSWINDOW_H
#define MAME_DEBUGGER_GTK_BREAKPOINTSWINDOW_H

#pragma once

#include "debuggerview.h"
#include "windowgtk.h"

#include <memory>


namespace osd::debugger::gtk {

//============================================================
//  The Breakpoints Window.
//============================================================
class breakpoints_window : public window_gtk
{
public:
	breakpoints_window(debugger_gtk &debugger, window_gtk *parent = nullptr);
	virtual ~breakpoints_window();

	virtual void restore_configuration(util::xml::data_node const &node) override;

protected:
	virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	void type_changed(int type);

	// Widgets
	GtkWidget *m_layout;
	std::unique_ptr<debugger_view> m_breakpoints_view;

	// Menu items
	GSimpleAction *m_type_group;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_BREAKPOINTSWINDOW_H
