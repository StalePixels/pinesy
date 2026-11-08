// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  hostmonitor.cpp - the Cocoa OSD's monitor module, for a host
//
//  osd_common_t registers the Cocoa OSD's modules by name. The
//  host places every window, so this file defines MONITOR_MAC
//  as one display in place of src/osd/modules/monitor/
//  monitor_mac.cpp. MAME only uses the display's size to choose
//  window sizes. host_display_area() gives that size; with no
//  display (Linux, or a Mac with every display asleep or none
//  connected) a fixed size is used. The monitor provider has no
//  "none" module.
//
//============================================================

#include "hostmonitor.h"

#include "modules/osdmodule.h"
#include "modules/lib/osdobj_common.h"
#include "modules/monitor/monitor_common.h"

#include "osdcore.h"

#include <memory>


namespace {

// the display MAME sizes its windows for when there is none
constexpr int DISPLAY_WIDTH = 1920;
constexpr int DISPLAY_HEIGHT = 1080;

class host_monitor_info : public osd_monitor_info
{
public:
	host_monitor_info(monitor_module &module)
		: osd_monitor_info(module, 0, std::string(OSDOPTION_SCREEN "0"), float(DISPLAY_WIDTH) / float(DISPLAY_HEIGHT))
	{
		host_monitor_info::refresh();
		set_aspect(float(m_pos_size.width()) / float(m_pos_size.height()));
	}

private:
	void refresh() override
	{
		if (!host_display_area(m_pos_size, m_usuable_pos_size))
		{
			m_pos_size = osd_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
			m_usuable_pos_size = m_pos_size;
		}
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

MODULE_DEFINITION(MONITOR_MAC, host_monitor_module)
