// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  linuxmodules.cpp - OSD modules the Cocoa OSD needs, on Linux
//
//  osd_common_t registers the Cocoa OSD's modules by name. On
//  Linux the monitor and joystick modules from src/osd/modules
//  use CoreGraphics and IOKit, so they are not built and this
//  file defines the same module names instead:
//  - MONITOR_MAC: one display. The host places its windows, so
//    MAME only uses the display's size to choose a starting
//    window size. The monitor provider has no "none" module.
//  - JOYSTICKINPUT_MACJOY: not supported. input_evdev.cpp
//    defines JOYSTICKINPUT_MACGAME.
//
//============================================================

#include "modules/osdmodule.h"
#include "modules/input/input_module.h"
#include "modules/lib/osdobj_common.h"
#include "modules/monitor/monitor_common.h"

#include "osdcore.h"

#include <memory>


namespace {

// the display MAME sizes its windows for
constexpr int DISPLAY_WIDTH = 1920;
constexpr int DISPLAY_HEIGHT = 1080;

class host_monitor_info : public osd_monitor_info
{
public:
	host_monitor_info(monitor_module &module)
		: osd_monitor_info(module, 0, std::string(OSDOPTION_SCREEN "0"), float(DISPLAY_WIDTH) / float(DISPLAY_HEIGHT))
	{
		host_monitor_info::refresh();
	}

private:
	void refresh() override
	{
		m_pos_size = osd_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
		m_usuable_pos_size = m_pos_size;
		m_is_primary = true;
	}
};

class host_monitor_module : public monitor_module_base
{
public:
	host_monitor_module()
		: monitor_module_base(OSD_MONITOR_PROVIDER, "mac")
	{
	}

	std::shared_ptr<osd_monitor_info> monitor_from_rect(const osd_rect &proposed) override
	{
		return m_initialized ? list().front() : nullptr;
	}

	std::shared_ptr<osd_monitor_info> monitor_from_window(const osd_window &window) override
	{
		return m_initialized ? list().front() : nullptr;
	}

protected:
	int init_internal(const osd_options &options) override
	{
		add_monitor(std::make_shared<host_monitor_info>(*this));
		return 0;
	}
};

} // anonymous namespace

namespace osd {

namespace {

MODULE_NOT_SUPPORTED(joystick_input_macjoy, OSD_JOYSTICKINPUT_PROVIDER, "macjoy")

} // anonymous namespace

} // namespace osd

MODULE_DEFINITION(MONITOR_MAC, host_monitor_module)
MODULE_DEFINITION(JOYSTICKINPUT_MACJOY, osd::joystick_input_macjoy)
