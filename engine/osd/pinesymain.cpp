// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  pinesymain.cpp - entry points of the pinesy C interface
//
//  The OSD object is the Cocoa OSD's mac_osd_interface. Only
//  the window layer differs: hostview.mm on macOS, hostlinux.cpp
//  on Linux. The pointer and mouse input are the same on both, and
//  are here.
//
//============================================================

#include "pinesy.h"

// MAME headers
#include "emu.h"
#include "main.h"
#include "render.h"
#include "rendlay.h"
#include "screen.h"

// OSD headers
#include "osdmac.h"
#include "window.h"
#include "modules/osdwindow.h"
#include "modules/diagnostics/diagnostics_module.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// in hostview.mm or hostlinux.cpp
void pinesy_attach_host(const pinesy_host *host);
int pinesy_mac_keycode(int keycode);


namespace {

// Passes each line of MAME's output to the host, then on to the OSD,
// which prints it as usual.
class host_output : public osd_output
{
public:
	host_output(pinesy_output_fn fn, void *context) : m_fn(fn), m_context(context) { }

	virtual void output_callback(osd_output_channel channel, util::format_argument_pack<char> const &args) override
	{
		chain_output(channel, args);
		const std::string text = util::string_format(args);
		m_fn(m_context, int(channel), text.c_str());
	}

private:
	pinesy_output_fn m_fn;
	void *m_context;
};

// The Cocoa OSD with Pinesy's default for MAME's UI Release Pointer
// key, which grabs and releases the pointer in turn: the front ends'
// Grab/Release Pointer shortcut, Command+Shift+M on macOS and
// Ctrl+Shift+M on Linux. MAME's own default, Right Control + Right
// Option, needs a key that Mac laptops do not have. MAME's Input menu
// can still change the key.
class pinesy_osd_interface : public mac_osd_interface
{
public:
	using mac_osd_interface::mac_osd_interface;

	virtual void customize_input_type_list(std::vector<input_type_entry> &typelist) override
	{
		mac_osd_interface::customize_input_type_list(typelist);
#if defined(__APPLE__)
		const input_code left = KEYCODE_LWIN, right = KEYCODE_RWIN;
#else
		const input_code left = KEYCODE_LCONTROL, right = KEYCODE_RCONTROL;
#endif
		for (input_type_entry &entry : typelist)
			if (entry.type() == IPT_UI_RELEASE_POINTER)
				entry.defseq(SEQ_TYPE_STANDARD).set(
						left, KEYCODE_LSHIFT, KEYCODE_M, input_seq::or_code,
						left, KEYCODE_RSHIFT, KEYCODE_M, input_seq::or_code,
						right, KEYCODE_LSHIFT, KEYCODE_M, input_seq::or_code,
						right, KEYCODE_RSHIFT, KEYCODE_M);
	}
};

// pinesy_run() is running
bool s_running = false;

// an exit was asked for before the machine had a window to take it
bool s_exit_pending = false;

// the host's pointer callback, for the run
pinesy_pointer_fn s_pointer_fn = nullptr;
void *s_pointer_context = nullptr;

// the pointer is over the host's view (pinesy_mouse_inside)
bool s_pointer_inside = false;

// what MAME last asked for: the pointer hidden, the pointer captured
bool s_pointer_hidden = false;
bool s_pointer_captured = false;

// what the host was last told
bool s_host_hidden = false;
bool s_host_captured = false;

// MAME's UI Release Pointer key freed the pointer; a click in the view
// takes it back
bool s_pointer_released = false;

// the UI Release Pointer key was down at the last poll
bool s_release_key_down = false;

// the button that took the pointer back, held until its release
int s_recapture_button = -1;

} // anonymous namespace

extern "C" int MacRequestMachineExit();        // in mac/window.cpp
extern "C" void MacKeyboardEvent(int vk, int down); // in modules/input/input_mac.cpp
extern "C" void MacCharEvent(unsigned int ch);
extern "C" void MacMouseButtonEvent(int button, int down);
extern "C" void MacMouseMotionEvent(float dx, float dy);
extern "C" void MacMouseWheelEvent(float dv, float dh);

static void pinesy_set_pointer(bool hidden, bool captured);


//============================================================
//  pinesy_run
//============================================================

extern "C" int pinesy_run(int argc, const char *const argv[], const pinesy_host *host)
{
	std::vector<std::string> args;
	args.emplace_back("pinesy");
	for (int i = 0; i < argc; i++)
		args.emplace_back(argv[i]);

	// disable I/O buffering
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);

	diagnostics_module::get_instance()->init_crash_diagnostics();

	pinesy_attach_host(host);
	s_pointer_fn = host ? host->pointer : nullptr;
	s_pointer_context = host ? host->context : nullptr;
	s_pointer_inside = false;
	s_pointer_hidden = false;
	s_pointer_captured = false;
	s_host_hidden = false;
	s_host_captured = false;
	s_pointer_released = false;
	s_release_key_down = false;
	s_recapture_button = -1;

	mac_options options;
	pinesy_osd_interface osd(options);
	osd.register_options();

	// the host's view takes a software-rendered frame; -video still overrides
	options.set_default_value(OSDOPTION_VIDEO, "soft");

	// the OSD object put itself on the output stack, so this one goes above it
	std::unique_ptr<host_output> output;
	if (host && host->output)
	{
		output = std::make_unique<host_output>(host->output, host->context);
		osd_output::push(output.get());
	}

	s_running = true;
	s_exit_pending = false;
	const int res = emulator_info::start_frontend(options, osd, args);
	s_running = false;
	s_exit_pending = false;

	if (output)
		osd_output::pop(output.get());

	// the pointer is free again when the run ends
	pinesy_set_pointer(false, false);
	s_pointer_fn = nullptr;
	s_pointer_context = nullptr;

	pinesy_attach_host(nullptr);
	return res;
}


//============================================================
//  pinesy_request_exit
//============================================================

extern "C" int pinesy_request_exit(void)
{
	if (!s_running)
		return 0;

	// before the machine has a window, the request waits for one
	if (!MacRequestMachineExit())
		s_exit_pending = true;
	return 1;
}

// Called from the event pump: passes a waiting exit request on once
// the machine can take it.
void pinesy_retry_exit()
{
	if (s_exit_pending && MacRequestMachineExit())
		s_exit_pending = false;
}


//============================================================
//  render size
//============================================================

// The size MAME's software renderer draws each frame at. It is the
// visible area of the machine's first screen times a whole number on
// each axis, so every machine pixel is the same number of frame pixels
// wide and high. Of the multipliers that fit in an 800x900 box, it takes
// the one with the shape closest to the shape of MAME's view, and of
// equal ones the smaller (the rule of Holub's emame, validate.py). The
// frame does not follow the window: the host scales it to its view on
// the GPU, so the CPU work is the same for any window. Leaves width and
// height as they are when the machine has no screen to measure.
void pinesy_render_size(void *wincontroller, int &width, int &height)
{
	for (const auto &w : osd_common_t::window_list())
	{
		auto &win = static_cast<mac_window_info &>(*w);
		if ((win.platform_window() != wincontroller) || (win.target() == nullptr))
			continue;

		screen_device *const screen = screen_device_enumerator(win.machine().root_device()).first();
		if (screen == nullptr)
			return;
		int vw = screen->visible_area().width();
		int vh = screen->visible_area().height();
		if ((vw <= 0) || (vh <= 0))
			return;
		if ((win.target()->orientation() ^ screen->orientation()) & ORIENTATION_SWAP_XY)
			std::swap(vw, vh);

		const double aspect = win.target()->current_view().effective_aspect();
		const double target = (aspect > 0.0) ? aspect : 4.0 / 3.0;
		constexpr int box_width = 800, box_height = 900;

		bool found = false;
		double best_error = 0.0;
		long best_area = 0;
		for (int mx = 1; mx <= std::max(1, box_width / vw); mx++)
		{
			for (int my = 1; my <= std::max(1, box_height / vh); my++)
			{
				const int fw = vw * mx, fh = vh * my;
				// errors equal to four places count as equal, as in validate.py
				const double error = std::round(std::fabs(double(fw) / fh - target) * 10000.0) / 10000.0;
				const long area = long(fw) * fh;
				if (!found || (error < best_error) || ((error == best_error) && (area < best_area)))
				{
					found = true;
					best_error = error;
					best_area = area;
					width = fw;
					height = fh;
				}
			}
		}
		return;
	}
}


// The picture fills the frame, as with MAME's -nokeepaspect: the
// frame is already the shape of whole machine pixels, so each machine
// pixel becomes exactly the multiplier's number of frame pixels on each
// axis. The hosts call this once for the view's window, when it gets
// its first frame size, so a change made later in MAME's video options
// menu still takes effect.
void pinesy_fill_frame(void *wincontroller)
{
	for (const auto &w : osd_common_t::window_list())
	{
		auto &win = static_cast<mac_window_info &>(*w);
		if ((win.platform_window() == wincontroller) && (win.target() != nullptr))
		{
			win.target()->set_keepaspect(false);
			return;
		}
	}
}


//============================================================
//  pinesy_frame_rect
//============================================================

extern "C" int pinesy_frame_rect(int frame_width, int frame_height, int view_width, int view_height,
		int *x, int *y, int *width, int *height)
{
	if ((x == nullptr) || (y == nullptr) || (width == nullptr) || (height == nullptr))
		return 0;
	*x = *y = *width = *height = 0;
	if ((frame_width <= 0) || (frame_height <= 0) || (view_width <= 0) || (view_height <= 0))
		return 0;

	// the axis with less room sets the scale; the other gets bars
	if (long(view_width) * frame_height <= long(view_height) * frame_width)
	{
		*width = view_width;
		*height = int(std::lround(double(frame_height) * view_width / frame_width));
	}
	else
	{
		*width = int(std::lround(double(frame_width) * view_height / frame_height));
		*height = view_height;
	}
	*width = std::max(1, *width);
	*height = std::max(1, *height);
	*x = (view_width - *width) / 2;
	*y = (view_height - *height) / 2;

	return ((*width % frame_width) == 0) && ((*height % frame_height) == 0)
			&& ((*width / frame_width) == (*height / frame_height)) ? 1 : 0;
}


//============================================================
//  pinesy_release_keys
//============================================================

// A key let go while another window or app has the keyboard sends its
// key-up there, so MAME would hold the key down from then on. On the
// Next a held key also blocks the ROM's key scan, which then ignores
// every other key. The hosts call this when the machine's view loses
// the keyboard, and MAME sees every key as up.
void pinesy_release_keys()
{
	if (osd_common_t::window_list().empty())
		return;

	downcast<mac_osd_interface &>(osd_common_t::window_list().front()->machine().osd()).release_keys();
}


//============================================================
//  pinesy_reset
//============================================================

extern "C" int pinesy_reset(int hard)
{
	// the machine has windows from its start to its exit
	if (!s_running || osd_common_t::window_list().empty())
		return 0;

	running_machine &machine = osd_common_t::window_list().front()->machine();
	if (hard)
		machine.schedule_hard_reset();
	else
		machine.schedule_soft_reset();
	return 1;
}


//============================================================
//  keyboard
//============================================================

extern "C" void pinesy_key(int keycode, int down)
{
	const int vk = pinesy_mac_keycode(keycode);
	if (vk >= 0)
		MacKeyboardEvent(vk, down);
}

extern "C" void pinesy_char(unsigned int ch)
{
	MacCharEvent(ch);
}


//============================================================
//  the pointer: MAME's requests go to the host
//============================================================

// Tells the host when the pointer it should have changes: what MAME
// asks for, but shown and free while the release key has freed it.
static void pinesy_update_host_pointer()
{
	const bool hidden = s_pointer_hidden && !s_pointer_released;
	const bool captured = s_pointer_captured && !s_pointer_released;
	if ((hidden == s_host_hidden) && (captured == s_host_captured))
		return;
	s_host_hidden = hidden;
	s_host_captured = captured;
	if (s_pointer_fn != nullptr)
		s_pointer_fn(s_pointer_context, hidden ? 1 : 0, captured ? 1 : 0);
}

static void pinesy_set_pointer(bool hidden, bool captured)
{
	s_pointer_hidden = hidden;
	s_pointer_captured = captured;
	pinesy_update_host_pointer();
}

// Releases a held pointer, so it can reach the host's menus, or grabs
// it again. A grab does not wait for the pointer to come over the
// view: MAME is told that it is there, so MAME asks to hold it, and the
// host moves it into the view.
static void pinesy_toggle_pointer_now()
{
	if (s_pointer_captured && !s_pointer_released)
	{
		s_pointer_released = true;
	}
	else
	{
		s_pointer_released = false;
		s_pointer_inside = true;
	}
	pinesy_update_host_pointer();
}

// Called from the event pump. MAME's UI Release Pointer key grabs and
// releases the pointer in turn. The Cocoa OSD defines the key but does
// not act on it.
void pinesy_poll_pointer()
{
	if (!s_running || osd_common_t::window_list().empty())
		return;

	running_machine &machine = osd_common_t::window_list().front()->machine();
	const bool down = machine.ioport().type_pressed(IPT_UI_RELEASE_POINTER);
	if (down && !s_release_key_down)
		pinesy_toggle_pointer_now();
	s_release_key_down = down;
}

extern "C" int pinesy_toggle_pointer(void)
{
	if (!s_running)
		return 0;
	pinesy_toggle_pointer_now();
	return 1;
}

// mac/window.cpp asks for these as the pointer moves over the view,
// the focus changes and MAME's menu opens and closes
void MacShowPointer(bool show)
{
	pinesy_set_pointer(!show, s_pointer_captured);
}

void MacCapturePointer(void *wincontroller, bool capture)
{
	pinesy_set_pointer(s_pointer_hidden, capture);
}

int MacPointerInWindow()
{
	return s_pointer_inside ? 1 : 0;
}


//============================================================
//  mouse
//============================================================

extern "C" void pinesy_mouse_inside(int inside)
{
	s_pointer_inside = inside != 0;
}

// While the release key has freed the pointer, the machine gets no
// mouse input. A click in the view takes the pointer back; that click
// does not reach the machine.
extern "C" void pinesy_mouse_move(double dx, double dy)
{
	if (s_pointer_released)
		return;
	MacMouseMotionEvent(float(dx), float(dy));
}

extern "C" void pinesy_mouse_button(int button, int down)
{
	if (s_pointer_released)
	{
		if (down && s_pointer_inside)
		{
			s_pointer_released = false;
			s_recapture_button = button;
			pinesy_update_host_pointer();
		}
		return;
	}
	if (button == s_recapture_button)
	{
		if (!down)
			s_recapture_button = -1;
		return;
	}
	MacMouseButtonEvent(button, down);
}

extern "C" void pinesy_mouse_wheel(double notches)
{
	if (s_pointer_released)
		return;
	MacMouseWheelEvent(float(notches), 0.0f);
}
