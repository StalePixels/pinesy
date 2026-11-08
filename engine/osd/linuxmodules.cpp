// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  linuxmodules.cpp - OSD modules the Cocoa OSD needs, on Linux
//
//  osd_common_t registers the Cocoa OSD's modules by name. On
//  Linux the joystick module from src/osd/modules uses IOKit, so
//  it is not built and this file defines the same module name
//  instead: JOYSTICKINPUT_MACJOY, not supported. input_evdev.cpp
//  defines JOYSTICKINPUT_MACGAME, and hostmonitor.cpp defines
//  MONITOR_MAC on both platforms.
//
//============================================================

#include "modules/osdmodule.h"
#include "modules/input/input_module.h"
#include "modules/lib/osdobj_common.h"

#include "osdcore.h"

#include <memory>


namespace osd {

namespace {

MODULE_NOT_SUPPORTED(joystick_input_macjoy, OSD_JOYSTICKINPUT_PROVIDER, "macjoy")

} // anonymous namespace

} // namespace osd

MODULE_DEFINITION(JOYSTICKINPUT_MACJOY, osd::joystick_input_macjoy)
