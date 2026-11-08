// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  hostmonitor.h - the display hostmonitor.cpp reports
//
//============================================================
#ifndef PINESY_OSD_HOSTMONITOR_H
#define PINESY_OSD_HOSTMONITOR_H

#pragma once

#include "modules/osdhelper.h"

// The main display's whole area and its work area (no menu bar
// and Dock), in top-left-origin global coordinates. Returns false
// when there is no display to measure (hostview.mm, hostlinux.cpp).
bool host_display_area(osd_rect &area, osd_rect &work);

#endif // PINESY_OSD_HOSTMONITOR_H
