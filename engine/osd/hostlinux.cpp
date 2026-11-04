// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  hostlinux.cpp - the Cocoa OSD's window layer, on a Linux host
//
//  The Cocoa OSD (src/osd/mac) reaches the platform only
//  through a set of C functions. hostview.mm makes them on a
//  host NSView; this file makes them for a GTK or Qt host on
//  Linux. MAME renders in software and passes each frame to the
//  host, which draws it. The host's events run through its pump
//  function. The host owns the windows, window order,
//  fullscreen and the pointer, so those calls do nothing here.
//
//============================================================

#include "pinesy.h"

#include <cstdint>
#include <iterator>


//============================================================
//  host and surfaces
//============================================================

namespace {

// one per MAME window
struct host_surface
{
	bool shown;     // the host shows this window
	int width;      // size used when the host does not show it
	int height;
};

pinesy_host s_host = {};
bool s_view_in_use = false;
bool s_focused = true;

// the host's view, as pinesy_view_size() last gave it; 0 until then
int s_view_width = 0;
int s_view_height = 0;
int s_view_pixel_width = 0;
int s_view_pixel_height = 0;

} // anonymous namespace

// in pinesymain.cpp
void pinesy_retry_exit();

void pinesy_attach_host(const pinesy_host *host)
{
	if (host != nullptr)
		s_host = *host;
	else
		s_host = pinesy_host{};
	s_view_in_use = false;
	s_focused = true;
}

extern "C" void pinesy_view_size(int width, int height, int pixel_width, int pixel_height)
{
	s_view_width = width;
	s_view_height = height;
	s_view_pixel_width = pixel_width;
	s_view_pixel_height = pixel_height;
}

extern "C" void pinesy_focus(int focused)
{
	s_focused = focused != 0;
}

void *CreateMAMEWindow(const char *title, int x, int y, int w, int h, bool isFullscreen, uint32_t displayID, bool headless)
{
	host_surface *surface = new host_surface{ false, w, h };

	// the host shows the first window that has a picture
	if (!headless && !s_view_in_use && (s_host.view != nullptr))
	{
		surface->shown = true;
		s_view_in_use = true;

		// the host sizes its view as MAME would size its own window
		if (s_host.size != nullptr)
			s_host.size(s_host.context, w, h);
	}

	return surface;
}

void MacDestroyWindow(void *wincontroller)
{
	host_surface *surface = (host_surface *)wincontroller;

	if (surface->shown)
		s_view_in_use = false;
	delete surface;
}

// the native handle bgfx and the input code ask for; there is none
void *GetOSWindow(void *wincontroller)
{
	return nullptr;
}

void *GetOSStandardWindow(void *wincontroller)
{
	return nullptr;
}

void MacWindowGetSize(void *wincontroller, int *width, int *height)
{
	host_surface *surface = (host_surface *)wincontroller;

	if (surface->shown && (s_view_width > 0) && (s_view_height > 0))
	{
		*width = s_view_width;
		*height = s_view_height;
		return;
	}

	*width = surface->width;
	*height = surface->height;
}

void MacWindowGetSizePixels(void *wincontroller, int *width, int *height)
{
	host_surface *surface = (host_surface *)wincontroller;

	if (surface->shown && (s_view_pixel_width > 0) && (s_view_pixel_height > 0))
	{
		*width = s_view_pixel_width;
		*height = s_view_pixel_height;
		return;
	}

	*width = surface->width;
	*height = surface->height;
}


//============================================================
//  frames
//============================================================

// hand a BGRX software-rendered frame to the host
void MacBlitVideoFrame(void *wincontroller, const void *bits, int width, int height, int pitch)
{
	if (s_host.frame != nullptr)
		s_host.frame(s_host.context, bits, width, height, pitch * 4);
}


//============================================================
//  event pump
//============================================================

// Runs the host's events once. The host's views get their key events
// here and send them back through pinesy_key().
void MacPollInputs()
{
	if (s_host.pump != nullptr)
		s_host.pump(s_host.context);

	pinesy_retry_exit();
}

bool MacAppHasFocus()
{
	return s_focused;
}


//============================================================
//  displays: one, of a fixed size, as the host places the
//  windows itself
//============================================================

uint32_t MacDisplayIDForWindow(void *wincontroller)
{
	return 0;
}

bool MacGetDisplayWorkArea(uint32_t displayID, int *x, int *y, int *width, int *height)
{
	return false;
}


//============================================================
//  window shape: passed to the host for its view
//============================================================

void MacSetWindowAspectRatio(void *wincontroller, int width, int height)
{
	if (((host_surface *)wincontroller)->shown && (s_host.aspect != nullptr))
		s_host.aspect(s_host.context, width, height);
}

void MacSetWindowContentSize(void *wincontroller, int width, int height)
{
	if (((host_surface *)wincontroller)->shown && (s_host.size != nullptr))
		s_host.size(s_host.context, width, height);
}


//============================================================
//  owned by the host: window order, fullscreen, pointer
//============================================================

void MacOrderWindowFront(void *wincontroller) { }
void MacSetWindowMinSize(void *wincontroller, int width, int height) { }
void MacSetFullscreen(void *wincontroller, bool fullscreen, uint32_t displayID) { }
void MacShowPointer(bool show) { }
void MacCapturePointer(void *wincontroller, bool capture) { }
int MacPointerInWindow() { return 0; }
bool MacGetPointerPosition(void *wincontroller, int *outx, int *outy) { return false; }


//============================================================
//  keyboard: XKB keycodes to macOS virtual key codes
//============================================================

namespace {

// indexed by the evdev key code (the XKB keycode minus 8); -1 where
// the Cocoa OSD's keyboard has no key. Scroll Lock, Pause and Print
// Screen sit where an Apple extended keyboard has them: F14, F15, F13.
constexpr int16_t s_evdev_to_mac[] =
{
	-1,   0x35, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16,  //   0: -, Esc, 1-6
	0x1a, 0x1c, 0x19, 0x1d, 0x1b, 0x18, 0x33, 0x30,  //   8: 7-0, -, =, Backspace, Tab
	0x0c, 0x0d, 0x0e, 0x0f, 0x11, 0x10, 0x20, 0x22,  //  16: Q W E R T Y U I
	0x1f, 0x23, 0x21, 0x1e, 0x24, 0x3b, 0x00, 0x01,  //  24: O P [ ] Enter LCtrl A S
	0x02, 0x03, 0x05, 0x04, 0x26, 0x28, 0x25, 0x29,  //  32: D F G H J K L ;
	0x27, 0x32, 0x38, 0x2a, 0x06, 0x07, 0x08, 0x09,  //  40: ' ` LShift \ Z X C V
	0x0b, 0x2d, 0x2e, 0x2b, 0x2f, 0x2c, 0x3c, 0x43,  //  48: B N M , . / RShift KP*
	0x3a, 0x31, 0x39, 0x7a, 0x78, 0x63, 0x76, 0x60,  //  56: LAlt Space Caps F1-F5
	0x61, 0x62, 0x64, 0x65, 0x6d, 0x47, 0x6b, 0x59,  //  64: F6-F10 NumLock ScrollLock KP7
	0x5b, 0x5c, 0x4e, 0x56, 0x57, 0x58, 0x45, 0x53,  //  72: KP8 KP9 KP- KP4 KP5 KP6 KP+ KP1
	0x54, 0x55, 0x52, 0x41, -1,   -1,   0x0a, 0x67,  //  80: KP2 KP3 KP0 KP. - - 102nd F11
	0x6f, -1,   -1,   -1,   -1,   -1,   -1,   -1,    //  88: F12
	0x4c, 0x3e, 0x4b, 0x69, 0x3d, -1,   0x73, 0x7e,  //  96: KPEnter RCtrl KP/ SysRq RAlt - Home Up
	0x74, 0x7b, 0x7c, 0x77, 0x7d, 0x79, 0x72, 0x75,  // 104: PgUp Left Right End Down PgDn Ins Del
	-1,   -1,   -1,   -1,   -1,   0x51, -1,   0x71,  // 112: KP= - Pause
	-1,   -1,   -1,   -1,   -1,   0x37, 0x36, 0x6e,  // 120: LMeta RMeta Compose
};

// F13 to F20 (evdev 183 to 190)
constexpr int16_t s_evdev_f13_to_mac[] = { 0x69, 0x6b, 0x71, 0x6a, 0x40, 0x4f, 0x50, 0x5a };

} // anonymous namespace

int pinesy_mac_keycode(int keycode)
{
	const int evdev = keycode - 8;
	if ((evdev >= 0) && (evdev < int(std::size(s_evdev_to_mac))))
		return s_evdev_to_mac[evdev];
	if ((evdev >= 183) && (evdev < 183 + int(std::size(s_evdev_f13_to_mac))))
		return s_evdev_f13_to_mac[evdev - 183];
	return -1;
}
