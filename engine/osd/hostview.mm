// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  hostview.mm - the Cocoa OSD's window layer, on a host view
//
//  The Cocoa OSD (src/osd/mac) calls a set of C functions for
//  its native windows; src/osd/mac/windowcontroller.mm makes
//  them on NSWindows of its own. This file makes the same
//  functions for a host application that owns every window:
//  the first MAME window draws on the NSView the host passes
//  to pinesy_run(), and MAME creates no window of its own.
//  The host's window takes the shape and size of the frame
//  (pinesy_render_size in pinesymain.cpp), not the window
//  size MAME asks for. Window order, fullscreen and the
//  pointer belong to the host, so those calls do nothing here.
//
//============================================================

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "pinesy.h"

#include "hostmonitor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>


//============================================================
//  host and surfaces
//============================================================

namespace {

// one per MAME window
struct host_surface
{
	NSView *view;   // the host's view, or nil
	int width;      // size used when there is no view
	int height;
	int frame_width = 0;    // the frame size the host's window was last fitted to
	int frame_height = 0;
};

pinesy_host s_host = {};
bool s_view_in_use = false;

// the host's view had the keyboard at the last event pump
bool s_had_keyboard = false;

} // anonymous namespace

static void FitWindowToFrame(host_surface *surface);

// in pinesymain.cpp
void pinesy_retry_exit();
void pinesy_render_size(void *wincontroller, int &width, int &height);
void pinesy_release_keys();

void pinesy_attach_host(const pinesy_host *host)
{
	if (host != nullptr)
		s_host = *host;
	else
		s_host = pinesy_host{};
	s_view_in_use = false;
	s_had_keyboard = false;
}

void *CreateMAMEWindow(const char *title, int x, int y, int w, int h, bool isFullscreen, uint32_t displayID, bool headless)
{
	host_surface *surface = new host_surface{ nil, w, h };

	// the host's view shows the first window that has a picture; the
	// window gets its size from the first frame (MacWindowGetSizePixels)
	if (!headless && !s_view_in_use && (s_host.view != nullptr))
	{
		surface->view = [(NSView *)s_host.view retain];
		s_view_in_use = true;
	}

	return surface;
}

void MacDestroyWindow(void *wincontroller)
{
	host_surface *surface = (host_surface *)wincontroller;

	if (surface->view != nil)
	{
		[surface->view release];
		s_view_in_use = false;
	}
	delete surface;
}

// the native handle bgfx and the input code ask for: the host's view
void *GetOSWindow(void *wincontroller)
{
	return ((host_surface *)wincontroller)->view;
}

void *GetOSStandardWindow(void *wincontroller)
{
	return GetOSWindow(wincontroller);
}

void MacWindowGetSize(void *wincontroller, int *width, int *height)
{
	host_surface *surface = (host_surface *)wincontroller;

	if (surface->view == nil)
	{
		*width = surface->width;
		*height = surface->height;
		return;
	}

	NSRect bounds = [surface->view bounds];
	*width = (int)bounds.size.width;
	*height = (int)bounds.size.height;
}

void MacWindowGetSizePixels(void *wincontroller, int *width, int *height)
{
	host_surface *surface = (host_surface *)wincontroller;

	if (surface->view == nil)
	{
		*width = surface->width;
		*height = surface->height;
		return;
	}

	NSRect backing = [surface->view convertRectToBacking:[surface->view bounds]];
	*width = (int)backing.size.width;
	*height = (int)backing.size.height;
	pinesy_render_size(wincontroller, *width, *height);

	// a new frame size (the first frame, a new screen mode) fits the window to it
	if ((*width != surface->frame_width) || (*height != surface->frame_height))
	{
		surface->frame_width = *width;
		surface->frame_height = *height;
		FitWindowToFrame(surface);
	}
}


//============================================================
//  window size
//============================================================

// Gives the host's window the frame's shape, and a size that is the
// largest whole multiple of the frame that fits on the screen, so each
// frame pixel covers the same number of screen pixels.
static void FitWindowToFrame(host_surface *surface)
{
	const int fw = surface->frame_width, fh = surface->frame_height;
	if ((fw <= 0) || (fh <= 0))
		return;

	if (s_host.aspect != nullptr)
		s_host.aspect(s_host.context, fw, fh);
	if (s_host.size == nullptr)
		return;

	NSWindow *window = [surface->view window];
	NSScreen *screen = ((window != nil) && ([window screen] != nil)) ? [window screen] : [NSScreen mainScreen];
	const CGFloat scale = (window != nil) ? [window backingScaleFactor] : ((screen != nil) ? [screen backingScaleFactor] : 1.0);
	int multiple = 1;
	if (screen != nil)
	{
		const NSRect visible = [screen visibleFrame];
		const CGFloat chrome = (window != nil) ? (NSHeight([window frame]) - NSHeight([window contentLayoutRect])) : 28.0;
		const int across = int(std::floor(NSWidth(visible) * scale / fw));
		const int down = int(std::floor((NSHeight(visible) - chrome) * scale / fh));
		multiple = std::max(1, std::min(across, down));
	}
	s_host.size(s_host.context, int(std::lround(fw * multiple / scale)), int(std::lround(fh * multiple / scale)));
}


//============================================================
//  frames
//============================================================

// put a BGRX software-rendered frame on the view's layer, then tell the host
void MacBlitVideoFrame(void *wincontroller, const void *bits, int width, int height, int pitch)
{
	host_surface *surface = (host_surface *)wincontroller;
	const size_t stride = size_t(pitch) * 4;

	if (surface->view != nil)
	{
		static CGColorSpaceRef s_colorspace = nullptr;
		if (s_colorspace == nullptr)
			s_colorspace = CGColorSpaceCreateDeviceRGB();

		// the layer may hold onto the image after we return, so give it a copy
		CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)bits, stride * height);
		CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
		CGImageRef image = CGImageCreate(
				width, height,
				8, 32, stride,
				s_colorspace,
				CGBitmapInfo(kCGBitmapByteOrder32Little) | CGBitmapInfo(kCGImageAlphaNoneSkipFirst),
				provider, nullptr, false, kCGRenderingIntentDefault);

		NSView *view = surface->view;
		if (![view wantsLayer])
			[view setWantsLayer:YES];

		// suppress the implicit contents-change animation
		[CATransaction begin];
		[CATransaction setDisableActions:YES];
		CALayer *layer = [view layer];
		// the frame is at the machine's size; the GPU scales it to the view
		[layer setContentsGravity:kCAGravityResize];
		// sharp when the view is a whole multiple of the frame; any other
		// size would make uneven pixels, so it is smoothed instead
		const NSRect bounds = [view convertRectToBacking:[view bounds]];
		const int bw = int(std::lround(NSWidth(bounds))), bh = int(std::lround(NSHeight(bounds)));
		const bool whole = (bw >= width) && (bh >= height) && (bw % width == 0) && (bh % height == 0);
		[layer setMagnificationFilter:whole ? kCAFilterNearest : kCAFilterLinear];
		[layer setContentsScale:([view window] != nil) ? [[view window] backingScaleFactor] : 1.0];
		[layer setContents:(id)image];
		[CATransaction commit];

		CGImageRelease(image);
		CGDataProviderRelease(provider);
		CFRelease(data);
	}

	if (s_host.frame != nullptr)
		s_host.frame(s_host.context, bits, width, height, int(stride));
}


//============================================================
//  event pump
//============================================================

// Runs the host's AppKit event loop for one pass. The host's views get
// their key events here and send them back through pinesy_key().
void MacPollInputs()
{
	NSEvent *event;

	while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
			untilDate:[NSDate distantPast] // do not wait for event
			inMode:NSDefaultRunLoopMode
			dequeue:YES]) != nil)
	{
		[NSApp sendEvent:event];
	}

	[NSApp updateWindows];

	// the view has the keyboard while the app is active and its window is key
	NSWindow *window = (s_host.view != nullptr) ? [(NSView *)s_host.view window] : nil;
	const bool has_keyboard = [NSApp isActive] && (window != nil) && [window isKeyWindow];
	if (s_had_keyboard && !has_keyboard)
		pinesy_release_keys();
	s_had_keyboard = has_keyboard;

	pinesy_retry_exit();
}

bool MacAppHasFocus()
{
	return [NSApp isActive];
}

// the host's key codes are already macOS virtual key codes
int pinesy_mac_keycode(int keycode)
{
	return keycode;
}


//============================================================
//  displays
//============================================================

static NSScreen *ScreenForDisplayID(uint32_t displayID)
{
	for (NSScreen *screen in [NSScreen screens])
	{
		NSNumber *number = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
		if ([number unsignedIntValue] == displayID)
			return screen;
	}
	return nil;
}

uint32_t MacDisplayIDForWindow(void *wincontroller)
{
	host_surface *surface = (host_surface *)wincontroller;
	NSScreen *screen = (surface->view != nil) ? [[surface->view window] screen] : nil;

	if (screen == nil)
		return 0;

	return [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
}

// a display's work area (no menu bar and Dock) in top-left-origin global coordinates
bool MacGetDisplayWorkArea(uint32_t displayID, int *x, int *y, int *width, int *height)
{
	NSScreen *screen = ScreenForDisplayID(displayID);

	if (screen == nil)
		return false;

	NSRect visible = [screen visibleFrame];
	*x = (int)visible.origin.x;
	*y = (int)(NSMaxY([[[NSScreen screens] firstObject] frame]) - NSMaxY(visible));
	*width = (int)visible.size.width;
	*height = (int)visible.size.height;

	return true;
}

bool host_display_area(osd_rect &area, osd_rect &work)
{
	NSScreen *screen = [NSScreen mainScreen];

	if (screen == nil)
		return false;

	// AppKit puts the origin at the bottom left of the first screen
	CGFloat const top = NSMaxY([[[NSScreen screens] firstObject] frame]);
	NSRect const frame = [screen frame];
	NSRect const visible = [screen visibleFrame];
	area = osd_rect(int(frame.origin.x), int(top - NSMaxY(frame)), int(frame.size.width), int(frame.size.height));
	work = osd_rect(int(visible.origin.x), int(top - NSMaxY(visible)), int(visible.size.width), int(visible.size.height));

	return area.width() > 0 && area.height() > 0;
}


//============================================================
//  window shape: decided by the frame, not by MAME
//============================================================

// The window's shape and size come from the frame (FitWindowToFrame), so
// MAME's own requests are not passed on.
void MacSetWindowAspectRatio(void *wincontroller, int width, int height) { }

void MacSetWindowContentSize(void *wincontroller, int width, int height) { }


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
