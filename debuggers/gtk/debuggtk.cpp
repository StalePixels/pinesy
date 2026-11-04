// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
//============================================================
//
//  debuggtk.cpp - GTK debug window handling
//
//============================================================

#include "modules/debugger/debug_module.h"
#include "modules/osdmodule.h"

#ifdef PINESY_DEBUG_GTK

#include "emu.h"
#include "config.h"
#include "debugger.h"
#include "modules/lib/osdobj_common.h"

#include "logwindow.h"
#include "mainwindow.h"
#include "dasmwindow.h"
#include "memorywindow.h"
#include "breakpointswindow.h"
#include "deviceswindow.h"
#include "deviceinformationwindow.h"

#include "util/xmlfile.h"

#include <gtk/gtk.h>


#if defined(SDLMAME_UNIX) || defined(SDLMAME_WIN32)
extern int sdl_entered_debugger;
#endif


namespace osd {

namespace {

class debug_gtk : public osd_module, public debug_module, protected debugger::gtk::debugger_gtk
{
public:
	debug_gtk() :
		osd_module(OSD_DEBUG_PROVIDER, "gtk"),
		debug_module(),
		m_machine(nullptr),
		m_mainwindow(nullptr)
	{
	}

	virtual ~debug_gtk() { }

	virtual int init(osd_interface &osd, const osd_options &options) override;
	virtual void exit() override;

	virtual void init_debugger(running_machine &machine) override;
	virtual void wait_for_debugger(device_t &device, bool firststop) override;
	virtual void debugger_update() override;

	virtual running_machine &machine() const override { return *m_machine; }

private:
	void configuration_load(config_type which_type, config_level level, util::xml::data_node const *parentnode);
	void configuration_save(config_type which_type, util::xml::data_node *parentnode);
	void load_window_configurations(util::xml::data_node const &parentnode);

	running_machine *m_machine;
	debugger::gtk::main_window *m_mainwindow;
	util::xml::file::ptr m_config;
};


//============================================================
//  Core functionality
//============================================================

int debug_gtk::init(osd_interface &osd, const osd_options &options)
{
	// a GTK host has already opened the display; otherwise open one, and
	// fail without a display so that another debugger is chosen
	if (!gdk_display_get_default() && !gtk_init_check())
		return -1;
	return gdk_display_get_default() ? 0 : -1;
}


void debug_gtk::exit()
{
	// If you've done a hard reset, clear out existing windows
	exit_debugger();
	m_mainwindow = nullptr;
}


void debug_gtk::init_debugger(running_machine &machine)
{
	m_machine = &machine;

	// Setup the configuration XML saving and loading
	machine.configuration().config_register("debugger",
			configuration_manager::load_delegate(&debug_gtk::configuration_load, this),
			configuration_manager::save_delegate(&debug_gtk::configuration_save, this));
}


//============================================================
//  Core functionality
//============================================================

void debug_gtk::wait_for_debugger(device_t &device, bool firststop)
{
#if defined(SDLMAME_UNIX) || defined(SDLMAME_WIN32)
	sdl_entered_debugger = 1;
#endif

	// Dialog initialization
	if (!m_mainwindow)
	{
		m_mainwindow = new debugger::gtk::main_window(*this);
		if (m_config)
		{
			load_window_configurations(*m_config->get_first_child());
			m_config.reset();
		}
		m_mainwindow->show();
	}

	// Ensure all top level windows are visible & bring main window to front
	show_all_windows();

	if (firststop)
		m_mainwindow->present();

	// Set the main window to display the proper CPU
	m_mainwindow->set_processor(&device);

	// Run the GTK events that are pending, for up to a millisecond
	osd_sleep(osd_ticks_per_second() / 1000 * 50);
	osd_ticks_t const end = osd_ticks() + (osd_ticks_per_second() / 1000);
	while (g_main_context_iteration(nullptr, FALSE) && (osd_ticks() < end))
	{
	}
}


//============================================================
//  Available for video.*
//============================================================

void debug_gtk::debugger_update()
{
	g_main_context_iteration(nullptr, FALSE);
}


void debug_gtk::configuration_load(config_type which_type, config_level level, util::xml::data_node const *parentnode)
{
	// We only care about system configuration files for now
	if ((config_type::SYSTEM == which_type) && parentnode)
	{
		if (m_mainwindow)
		{
			load_window_configurations(*parentnode);
		}
		else
		{
			m_config = util::xml::file::create();
			parentnode->copy_into(*m_config);
		}
	}
}


void debug_gtk::configuration_save(config_type which_type, util::xml::data_node *parentnode)
{
	// We only save system configuration for now
	if ((config_type::SYSTEM == which_type) && parentnode)
		save_configuration(*parentnode);
}


void debug_gtk::load_window_configurations(util::xml::data_node const &parentnode)
{
	for (util::xml::data_node const *wnode = parentnode.get_child(debugger::NODE_WINDOW); wnode; wnode = wnode->get_next_sibling(debugger::NODE_WINDOW))
	{
		debugger::gtk::window_gtk *win = nullptr;
		switch (wnode->get_attribute_int(debugger::ATTR_WINDOW_TYPE, -1))
		{
		case debugger::WINDOW_TYPE_CONSOLE:
			win = m_mainwindow;
			break;
		case debugger::WINDOW_TYPE_MEMORY_VIEWER:
			win = new debugger::gtk::memory_window(*this);
			break;
		case debugger::WINDOW_TYPE_DISASSEMBLY_VIEWER:
			win = new debugger::gtk::dasm_window(*this);
			break;
		case debugger::WINDOW_TYPE_ERROR_LOG_VIEWER:
			win = new debugger::gtk::log_window(*this);
			break;
		case debugger::WINDOW_TYPE_POINTS_VIEWER:
			win = new debugger::gtk::breakpoints_window(*this);
			break;
		case debugger::WINDOW_TYPE_DEVICES_VIEWER:
			win = new debugger::gtk::devices_window(*this);
			break;
		case debugger::WINDOW_TYPE_DEVICE_INFO_VIEWER:
			win = new debugger::gtk::device_information_window(*this);
			break;
		}
		if (win)
			win->restore_configuration(*wnode);
	}
}

} // anonymous namespace

} // namespace osd

MODULE_DEFINITION(DEBUG_GTK, osd::debug_gtk)

#endif // PINESY_DEBUG_GTK
