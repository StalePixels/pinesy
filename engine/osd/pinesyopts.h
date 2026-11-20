// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  pinesyopts.h - options that Pinesy adds to MAME's
//
//  The nextpi card (mame/src/devices/bus/rs232/nextpi.cpp) reads
//  the Pi options by name when it starts QEMU, once per process.
//  A build of the card with another OSD has no such options, and
//  its Pi has no GDB stub.
//
//============================================================

#ifndef PINESY_ENGINE_OSD_PINESYOPTS_H
#define PINESY_ENGINE_OSD_PINESYOPTS_H

#pragma once

#include "options.h"

// TCP port of QEMU's GDB stub for the Pi; 0 = no stub. The stub binds
// to -debugger_host, as MAME's own stub does.
#define PINESYOPTION_PI_GDB_PORT "pi_gdb_port"

// The Pi waits at its first instruction until a GDB client or
// pinesy_continue() continues it (QEMU -S).
#define PINESYOPTION_PI_GDB_WAIT "pi_gdb_wait"

inline void pinesy_add_options(core_options &options)
{
	static const options_entry entries[] =
	{
		{ nullptr,                  nullptr, core_options::option_type::HEADER,  "PINESY PI OPTIONS" },
		{ PINESYOPTION_PI_GDB_PORT, "0",     core_options::option_type::INTEGER, "port of the Pi's GDB stub (QEMU); 0 for none" },
		{ PINESYOPTION_PI_GDB_WAIT, "0",     core_options::option_type::BOOLEAN, "Pi waits at its first instruction until it is continued" },
		{ nullptr }
	};
	options.add_entries(entries);
}

#endif // PINESY_ENGINE_OSD_PINESYOPTS_H
