// Runs one machine at a time in its own window. MAME runs inside
// pinesy_run() on the main thread and pumps GTK's events while it
// runs, so the app's other windows stay live.

#ifndef PINESY_GTK_MACHINE_RUNNER_HPP
#define PINESY_GTK_MACHINE_RUNNER_HPP

#include "pinesy_core.hpp"
#include "pinesy.h"

#include <adwaita.h>
#include <gtkmm/application.h>
#include <gtkmm/widget.h>
#include <sigc++/signal.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace pinesy::gtk {

// The widget MAME's first screen shows in. It draws the last frame at
// the frame's shape, centred, with black bars where the view's shape
// differs, and tells the engine its size.
class MachineView : public Gtk::Widget
{
public:
	MachineView();

	// Shows a BGRX frame from the engine's frame callback.
	void show_frame(const void *pixels, int width, int height, int pitch);

	bool allocated() const { return m_allocated; }

protected:
	void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot> &snapshot) override;
	void size_allocate_vfunc(int width, int height, int baseline) override;
	void measure_vfunc(Gtk::Orientation orientation, int for_size, int &minimum, int &natural,
			int &minimum_baseline, int &natural_baseline) const override;

private:
	GdkTexture *m_texture = nullptr;
	bool m_allocated = false;
};

// A machine's window: a header bar and the view. Every key goes to the
// machine, and the mouse over the view goes to MAME. Closing the window
// asks the machine to stop; the window goes when the run has ended.
class MachineWindow
{
public:
	// window_size is the configuration's window size
	// (core::window::content_size).
	MachineWindow(const std::string &title, int window_size);
	~MachineWindow();

	MachineWindow(const MachineWindow &) = delete;
	MachineWindow &operator=(const MachineWindow &) = delete;

	// Shows the window and waits until the view has its size. False
	// when the window was closed before that.
	bool show_and_wait();

	// The host callbacks for pinesy_run(), valid while this window lives.
	pinesy_host host();

	// Tells the window that the machine is running, so closing it asks
	// the machine to stop.
	void set_running(bool running) { m_running = running; }

	// Called with true when MAME holds the pointer in the view, and with
	// false when it lets it go.
	void set_on_pointer_held(std::function<void(bool)> fn) { m_on_pointer_held = std::move(fn); }

	// MAME's error lines from the run.
	const std::string &errors() const { return m_errors; }

	// Key events, by XKB keycode.
	bool on_key_pressed(unsigned keyval, unsigned keycode);
	void on_key_released(unsigned keycode);

	// Pointer events over the view, in the view's logical units.
	void on_pointer_enter(double x, double y);
	void on_pointer_motion(double x, double y);
	void on_pointer_leave();

private:
	AdwWindow *m_window = nullptr;
	GtkWidget *m_header = nullptr;
	MachineView *m_view = nullptr;
	int m_window_size = core::window::fit;
	// the window goes to full screen with the first frame, when its
	// window size says so
	bool m_sized = false;
	gulong m_active_handler = 0;
	gulong m_fullscreened_handler = 0;
	std::set<unsigned> m_keys_down;
	std::string m_errors;
	bool m_running = false;
	bool m_closed = false;
	std::function<void(bool)> m_on_pointer_held;
	// the pointer's last position over the view, to send its movement
	bool m_has_pointer = false;
	double m_pointer_x = 0;
	double m_pointer_y = 0;

	static void on_frame(void *context, const void *pixels, int width, int height, int pitch);
	static void on_output(void *context, int channel, const char *text);
	static void on_size(void *context, int width, int height);
	static void on_pump(void *context);
	static void on_pointer(void *context, int hide, int capture);

	void on_active_changed();
	void release_keys();
};

// One run at a time, for the app.
class MachineRunner
{
public:
	explicit MachineRunner(Gtk::Application &app) : m_app(app) {}

	bool running() const { return m_running; }

	// The running machine holds the pointer in its view.
	bool pointer_held() const { return m_pointer_held; }

	// Runs a configuration in its own window, starting from the main
	// loop once the current event has been handled. window_size is the
	// configuration's window size. takes_mouse is the summary's mouse
	// and mouse the configuration's mouse setting. Does nothing while a
	// machine runs.
	void start(const core::Core &core, const std::string &title, const std::vector<std::string> &arguments,
			bool debug, int window_size, bool takes_mouse, bool mouse);

	// Asks the running machine to stop. Its window closes when it has.
	void stop();

	// Resets the running machine. A hard reset starts it again from
	// power-on.
	void reset(bool hard);

	// Releases a held pointer, or grabs it again.
	void toggle_pointer();

	// Emitted when a run starts and when it ends, and when the pointer
	// is held or let go.
	sigc::signal<void()> &signal_changed() { return m_changed; }

	// Emitted when a run ends with an exit code that is not 0: the
	// window title, the exit code and MAME's error lines.
	sigc::signal<void(const std::string &, int, const std::string &)> &signal_failed() { return m_failed; }

private:
	Gtk::Application &m_app;
	bool m_running = false;
	bool m_pointer_held = false;
	sigc::signal<void()> m_changed;
	sigc::signal<void(const std::string &, int, const std::string &)> m_failed;

	void run(const core::Core &core, const std::string &title, const std::vector<std::string> &arguments,
			bool debug, int window_size, bool takes_mouse, bool mouse);
};

// Headless mode with a window: runs the arguments in a machine window
// that fits the screen, with no app around it. Returns MAME's exit code.
int run_headless_in_window(const core::Core &core, const std::vector<std::string> &arguments);

} // namespace pinesy::gtk

#endif
