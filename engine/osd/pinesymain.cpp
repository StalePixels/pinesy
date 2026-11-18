// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  pinesymain.cpp - entry points of the pinesy C interface
//
//  The OSD object is the Cocoa OSD's mac_osd_interface. Only
//  the window layer differs: hostview.mm on macOS, hostlinux.cpp
//  on Linux.
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

// pinesy_run() is running
bool s_running = false;

// an exit was asked for before the machine had a window to take it
bool s_exit_pending = false;

} // anonymous namespace

extern "C" int MacRequestMachineExit();        // in mac/window.cpp
extern "C" void MacKeyboardEvent(int vk, int down); // in modules/input/input_mac.cpp
extern "C" void MacCharEvent(unsigned int ch);


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

	mac_options options;
	mac_osd_interface osd(options);
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
