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
//  size MAME asks for: the host gets the frame's size, and
//  the core decides the window's. Window order, fullscreen and the
//  pointer belong to the host, so those calls do nothing here.
//
//============================================================

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "pinesy.h"

#include "hostmonitor.h"

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
	int frame_width = 0;    // the frame size last sent to the host
	int frame_height = 0;
};

pinesy_host s_host = {};
bool s_view_in_use = false;

// the host's view had the keyboard at the last event pump
bool s_had_keyboard = false;

} // anonymous namespace

// in pinesymain.cpp
void pinesy_retry_exit();
void pinesy_render_size(void *wincontroller, int &width, int &height);
void pinesy_fill_frame(void *wincontroller);
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

	// the first frame size, and each new one (a new screen mode), goes to the host
	if ((*width != surface->frame_width) || (*height != surface->frame_height))
	{
		if (surface->frame_width == 0)
			pinesy_fill_frame(wincontroller);
		surface->frame_width = *width;
		surface->frame_height = *height;
		if (s_host.aspect != nullptr)
			s_host.aspect(s_host.context, *width, *height);
		if (s_host.size != nullptr)
			s_host.size(s_host.context, *width, *height);
	}
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
		// The frame keeps its shape, centred, with black bars where the
		// view's shape differs; the GPU scales it. It is sharp where the
		// drawn size is a whole multiple of the frame, and smoothed at
		// any other size, which would make uneven pixels.
		[layer setContentsGravity:kCAGravityResizeAspect];
		[layer setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
		const NSRect bounds = [view convertRectToBacking:[view bounds]];
		int x, y, drawn_width, drawn_height;
		const bool whole = pinesy_frame_rect(width, height, int(std::lround(NSWidth(bounds))), int(std::lround(NSHeight(bounds))),
				&x, &y, &drawn_width, &drawn_height) != 0;
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

// The window's shape and size come from the frame (MacWindowGetSizePixels), so
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
