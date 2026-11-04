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

#include <set>
#include <string>
#include <vector>

namespace pinesy::gtk {

// The widget MAME's first screen shows in. It draws the last frame
// and tells the engine its size.
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
// machine. Closing the window asks the machine to stop; the window
// goes when the run has ended.
class MachineWindow
{
public:
	explicit MachineWindow(const std::string &title);
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

	// MAME's error lines from the run.
	const std::string &errors() const { return m_errors; }

	// Key events, by XKB keycode.
	bool on_key_pressed(unsigned keyval, unsigned keycode);
	void on_key_released(unsigned keycode);

private:
	AdwWindow *m_window = nullptr;
	GtkWidget *m_header = nullptr;
	MachineView *m_view = nullptr;
	gulong m_active_handler = 0;
	std::set<unsigned> m_keys_down;
	std::string m_errors;
	bool m_running = false;
	bool m_closed = false;

	static void on_frame(void *context, const void *pixels, int width, int height, int pitch);
	static void on_output(void *context, int channel, const char *text);
	static void on_size(void *context, int width, int height);
	static void on_pump(void *context);

	void on_active_changed();
	void release_keys();
};

// One run at a time, for the app.
class MachineRunner
{
public:
	explicit MachineRunner(Gtk::Application &app) : m_app(app) {}

	bool running() const { return m_running; }

	// Runs a configuration in its own window, starting from the main
	// loop once the current event has been handled. Does nothing while
	// a machine runs.
	void start(const core::Core &core, const std::string &title, const std::vector<std::string> &arguments,
			bool debug);

	// Asks the running machine to stop. Its window closes when it has.
	void stop();

	// Emitted when a run starts and when it ends.
	sigc::signal<void()> &signal_changed() { return m_changed; }

	// Emitted when a run ends with an exit code that is not 0: the
	// window title, the exit code and MAME's error lines.
	sigc::signal<void(const std::string &, int, const std::string &)> &signal_failed() { return m_failed; }

private:
	Gtk::Application &m_app;
	bool m_running = false;
	sigc::signal<void()> m_changed;
	sigc::signal<void(const std::string &, int, const std::string &)> m_failed;

	void run(const core::Core &core, const std::string &title, const std::vector<std::string> &arguments,
			bool debug);
};

// Headless mode with a window: runs the arguments in a machine window,
// with no app around it. Returns MAME's exit code.
int run_headless_in_window(const core::Core &core, const std::vector<std::string> &arguments);

} // namespace pinesy::gtk

#endif
