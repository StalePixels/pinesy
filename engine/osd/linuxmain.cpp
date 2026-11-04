// license:BSD-3-Clause
// copyright-holders:R. Belmont, D. Rimron-Soutter
//============================================================
//
//  linuxmain.cpp - the Cocoa OSD's object, on Linux
//
//  src/osd/mac/macmain.cpp defines mac_options and the
//  general parts of mac_osd_interface with Carbon and sysctl.
//  This file defines the same parts for Linux. The entry point
//  is pinesy_run() in pinesymain.cpp.
//
//============================================================

// MAME headers
#include "corestr.h"
#include "emu.h"
#include "emuopts.h"
#include "main.h"

// OSD headers
#include "osdmac.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>


//============================================================
//  OPTIONS
//============================================================

#ifndef INI_PATH
#define INI_PATH "$HOME/.APP_NAME;.;ini"
#endif // INI_PATH

const options_entry mac_options::s_option_entries[] =
{
	{ MACOPTION_INIPATH, INI_PATH, core_options::option_type::STRING, "path to ini files" },

	// End of list
	{ nullptr }
};

mac_options::mac_options()
	: osd_options()
{
	std::string ini_path(INI_PATH);
	add_entries(mac_options::s_option_entries);
	strreplace(ini_path, "APP_NAME", emulator_info::get_appname_lower());
	set_default_value(MACOPTION_INIPATH, std::move(ini_path));
}


//============================================================
//  construction
//============================================================

mac_osd_interface::mac_osd_interface(mac_options &options)
	: osd_common_t(options), m_options(options)
{
}

mac_osd_interface::~mac_osd_interface()
{
}

void mac_osd_interface::osd_exit()
{
	osd_common_t::osd_exit();
}

void mac_osd_interface::output_oslog(const char *buffer)
{
	fputs(buffer, stderr);
}

void osd_setup_osd_specific_emu_options(emu_options &opts)
{
	opts.add_entries(osd_options::s_option_entries);
}


//============================================================
//  init
//============================================================

void mac_osd_interface::init(running_machine &machine)
{
	osd_common_t::init(machine);

	// benchmarking runs flat out, with no sound and no picture
	const int bench = options().bench();
	if (bench > 0)
	{
		options().set_value(OPTION_SLEEP, false, OPTION_PRIORITY_MAXIMUM);
		options().set_value(OPTION_THROTTLE, false, OPTION_PRIORITY_MAXIMUM);
		options().set_value(OSDOPTION_SOUND, "none", OPTION_PRIORITY_MAXIMUM);
		options().set_value(OSDOPTION_VIDEO, "none", OPTION_PRIORITY_MAXIMUM);
		options().set_value(OPTION_SECONDS_TO_RUN, bench, OPTION_PRIORITY_MAXIMUM);
	}

	const char *const numprocessors = options().numprocessors();
	osd_num_processors = 0;
	if (strcmp(numprocessors, "auto") != 0)
	{
		osd_num_processors = atoi(numprocessors);
		if (osd_num_processors < 1)
		{
			osd_printf_warning("numprocessors < 1 doesn't make much sense. Assuming auto ...\n");
			osd_num_processors = 0;
		}
	}

	osd_printf_verbose("Build version:      %s\n", emulator_info::get_build_version());

	osd_common_t::init_subsystems();

	if (options().oslog())
	{
		using namespace std::placeholders;
		machine.add_logerror_callback(std::bind(&mac_osd_interface::output_oslog, this, _1));
	}
}
