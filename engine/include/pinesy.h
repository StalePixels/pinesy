// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  pinesy.h - C interface between MAME and a native host
//
//  The host is a native application: Cocoa on macOS, GTK or Qt
//  on Linux. It owns the windows and the menus. MAME runs on
//  the main thread inside pinesy_run() and keeps its own main
//  loop; while it runs it pumps the host's events, so the
//  host's UI stays live, also while the debugger has the
//  machine stopped. On macOS MAME pumps AppKit itself; on
//  Linux it calls the host's pump function.
//
//============================================================

#ifndef MAME_OSD_PINESY_PINESY_H
#define MAME_OSD_PINESY_PINESY_H

#ifdef __cplusplus
extern "C" {
#endif

// Called after each frame is drawn. pixels is BGRX, 4 bytes per
// pixel, top row first, pitch bytes per row. Rows can be padded, so
// pitch can be more than width * 4. It is valid only during the call.
// May be null.
typedef void (*pinesy_frame_fn)(void *context, const void *pixels, int width, int height, int pitch);

// Receives one complete UTF-8 JSON document. Valid only during the call.
typedef void (*pinesy_text_fn)(void *context, const char *text);

// Receives one line of MAME output. channel is MAME's osd_output_channel:
// 0 error, 1 warning, 2 info, 3 debug, 4 verbose, 5 log.
typedef void (*pinesy_output_fn)(void *context, int channel, const char *text);

// Receives a width and a height, in pixels.
typedef void (*pinesy_size_fn)(void *context, int width, int height);

// Runs the host's pending events once, without waiting.
typedef void (*pinesy_pump_fn)(void *context);

// What the host does with the system pointer over its view: hide is 1
// when the pointer is hidden there, capture is 1 when the pointer is
// held in the view, so that it cannot leave while the host goes on
// sending its movement.
typedef void (*pinesy_pointer_fn)(void *context, int hide, int capture);

// Receives a yes (1) or a no (0).
typedef void (*pinesy_flag_fn)(void *context, int value);

typedef struct pinesy_host
{
	// macOS: the NSView that shows the first screen. MAME sets the
	// frame as the contents of its layer, which draws it where
	// pinesy_frame_rect() says. The host keeps the view alive for the
	// whole run.
	// Linux: any non-null value says that the host shows the first
	// screen; the host draws each frame it gets in its view, where
	// pinesy_frame_rect() says. With a view, MAME renders the frame at
	// the visible area of the machine's first screen times a whole
	// number on each axis, so each machine pixel is the same number of
	// frame pixels wide and high: of the multiples that fit in 800 x
	// 900, the one with the shape closest to MAME's view of the
	// machine, and of equal ones the smaller. The picture fills the
	// frame, as with MAME's -nokeepaspect. May be null: then MAME
	// renders at its own size and only calls frame.
	void *view;

	pinesy_frame_fn frame;

	// Receives MAME's output while it runs. May be null. MAME still
	// prints the output as it does without a host.
	pinesy_output_fn output;

	// The frame's size, in pixels: aspect gives the shape to keep when
	// the user resizes the window, size the frame to size the window
	// for (pinesy_core_window_size() in the core gives the window's
	// size). MAME calls both with the first frame, and again when the
	// frame's size changes (screen mode, rotation). A resize by the
	// user brings no call. Either may be null.
	pinesy_size_fn aspect;
	pinesy_size_fn size;

	// Linux: runs the host's toolkit events (g_main_context_iteration,
	// QCoreApplication::processEvents). macOS ignores it. May be null.
	pinesy_pump_fn pump;

	// MAME's pointer requests. While the pointer is over the view
	// (pinesy_mouse_inside), MAME asks to hide it in a run with -mouse
	// or -lightgun, and to capture it in a run with -mouse. It asks to
	// show and release it when the app loses the focus, when MAME's
	// menu opens, and at the end of the run. MAME's UI Release Pointer
	// key (Command+Shift+M on macOS and Ctrl+Shift+M on Linux by
	// default, set in MAME's Input menu) or pinesy_toggle_pointer()
	// shows and frees a captured pointer, and the machine gets no mouse
	// input until the same key, pinesy_toggle_pointer() or a click in
	// the view grabs it again; that click does not reach the machine. A call comes only when either value changes. MAME's
	// debugger, while enabled, makes no requests. May be null.
	pinesy_pointer_fn pointer;

	// A CPU of the machine is stopped and waits (1), or none is (0):
	// the Next stopped in MAME's debugger, or the Pi of the nextpi card
	// waiting at its first instruction (-pi_gdb_wait) or stopped by its
	// GDB stub. pinesy_continue() continues them. Each run starts with
	// none stopped, and a run that ends while one is stopped gets a 0.
	// A call comes only when the value changes. May be null.
	pinesy_flag_fn stopped;

	void *context;
} pinesy_host;

// Runs MAME with a command line, as `mame` would get it without the
// program name: for example { "tbblue", "-pi", "nextpi.img" }.
// Main thread only. Returns when MAME exits, with MAME's exit code.
// host may be null.
int pinesy_run(int argc, const char *const argv[], const pinesy_host *host);

// Asks the running machine to exit. Returns 1 while pinesy_run() runs.
// A request made before the machine has started is kept, and the
// machine exits as soon as it has.
int pinesy_request_exit(void);

// Resets the running machine, as MAME's F3 (soft) and Shift+F3 (hard)
// do. A soft reset (hard 0) resets the machine's devices and keeps
// RAM. A hard reset (hard 1) starts the machine again from power-on,
// with the same command line. Returns 1 if a machine took the request,
// 0 if none is running or it has not started yet.
int pinesy_reset(int hard);

// Releases a captured pointer, or grabs it again, as MAME's UI Release
// Pointer key does (see pinesy_host.pointer). A grab moves the pointer
// into the view. Returns 1 while pinesy_run() runs.
int pinesy_toggle_pointer(void);

// Under MAME's debugger (-debug), MAME stops each new machine at its
// first instruction. With paused 0, the default, Pinesy continues it
// there, so it runs from the start; with 1 it stays stopped until the
// debugger, a GDB client or pinesy_continue() continues it. Applies to
// the runs that pinesy_run() starts after the call.
void pinesy_set_start_paused(int paused);

// Continues every stopped CPU (see pinesy_host.stopped): the Next, as
// the debugger's go command does, and the Pi, as GDB's continue command
// does. A GDB client that has one stopped is not told. Returns 1 if one
// was stopped, 0 if none was or no machine is running.
int pinesy_continue(void);

// Keyboard input. keycode is the key's position, not its character:
// on macOS a virtual key code (NSEvent keyCode, kVK_* in Carbon), on
// Linux an XKB keycode, the evdev code plus 8 (GTK
// gdk_key_event_get_keycode, Qt QKeyEvent::nativeScanCode, on X11 and
// on Wayland). Modifier keys are sent as key events too.
void pinesy_key(int keycode, int down);

// Typed characters, for text entry in MAME's own menus.
void pinesy_char(unsigned int ch);

// Mouse input, for MAME's mouse. MAME takes it only with -mouse; the
// machine reads it through its own mouse inputs, such as the Next's
// Kempston mouse. The host sends the events of its view of the first
// screen, and the movement of a drag that leaves the view.
//
// pinesy_mouse_inside: the pointer came over the view (1) or left it
// (0). Each run starts with the pointer outside. MAME hides and
// captures the pointer only while it is inside.
void pinesy_mouse_inside(int inside);

// pinesy_mouse_move: the pointer moved by dx, dy logical units, right
// and down positive. While the pointer is captured the host still
// sends the movement the mouse makes.
void pinesy_mouse_move(double dx, double dy);

// pinesy_mouse_button: button 0 is the left button, 1 the right, 2 the
// middle, and 3 and 4 the next two. Other buttons are ignored.
void pinesy_mouse_button(int button, int down);

// pinesy_mouse_wheel: the vertical wheel turned by a number of notches,
// positive away from the user. A device that scrolls smoothly sends
// parts of a notch.
void pinesy_mouse_wheel(double notches);

// Linux: the size of the host's view of the first screen, in
// logical units and in pixels. Call it before pinesy_run() and
// whenever the view changes size. macOS takes the size from the
// NSView and has no such function.
void pinesy_view_size(int width, int height, int pixel_width, int pixel_height);

// Where a host draws a frame in its view: at the frame's shape, as
// big as the view takes, centred. The rest of the view is black. All
// sizes are in pixels. Gives the rectangle in x, y, width and height,
// from the view's top left. Returns 1 when the drawn size is a whole
// multiple of the frame, so the host scales it with nearest-neighbour
// filtering and every frame pixel covers the same number of view
// pixels; 0 when it is not, and the host scales it with linear
// filtering. Returns 0, with every output 0, when a size is 0 or less
// or an output is null.
int pinesy_frame_rect(int frame_width, int frame_height, int view_width, int view_height,
		int *x, int *y, int *width, int *height);

// Linux: whether the host's window has the keyboard focus (1) or not
// (0). Without -background_input MAME ignores input while it has not.
// When the window loses the focus, MAME takes every key as released.
// Each run starts as focused. macOS asks NSApp and has no such
// function.
void pinesy_focus(int focused);

// Every driver in the library, as one JSON array. Each driver has
// name, description, manufacturer, year, cloneof (or null),
// sourcefile, unofficial, status (good, imperfect or preliminary,
// as in -listxml), biossets, devices (the sorted device type names
// of the default configuration, without the driver itself) and
// slots (as pinesy_describe gives them, for the default
// configuration). A driver MAME cannot describe is left out.
// Returns 0.
int pinesy_list_machines(pinesy_text_fn fn, void *context);

// The machine a command line describes, as one JSON object. argv is
// a command line as pinesy_run() takes it: a driver, then options.
// MAME parses it as for a run (ini files are read only when the
// command line allows it) and builds the machine configuration
// without starting it. The object has driver, bios, biossets, slots
// and media; mouse, true when the machine with this hardware has
// mouse inputs (input fields of MAME's mouse types, the controls
// -listxml shows as type "mouse"); errors, with MAME's error text;
// and warnings, with MAME's warning text, which do not stop a run.
// Both are empty when there is none. Returns 0, or MAME's error code.
int pinesy_describe(int argc, const char *const argv[], pinesy_text_fn fn, void *context);

#ifdef __cplusplus
}
#endif

#endif // MAME_OSD_PINESY_PINESY_H
