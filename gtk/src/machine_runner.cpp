#include "machine_runner.hpp"

#include "signals.hpp"

#include <glibmm/main.h>
#include <gtkmm/init.h>
#include <gtkmm/snapshot.h>

#include <algorithm>
#include <cmath>

namespace pinesy::gtk {

//------------------------------------------------------------
//  MachineView
//------------------------------------------------------------

MachineView::MachineView() : Glib::ObjectBase("PinesyMachineView")
{
	set_focusable(true);
	set_hexpand(true);
	set_vexpand(true);
}

void MachineView::show_frame(const void *pixels, int width, int height, int pitch)
{
	if (!pixels || width <= 0 || height <= 0)
		return;
	GBytes *bytes = g_bytes_new(pixels, gsize(pitch) * gsize(height));
	GdkTexture *texture = gdk_memory_texture_new(width, height, GDK_MEMORY_B8G8R8X8, bytes, gsize(pitch));
	g_bytes_unref(bytes);
	if (m_texture)
		g_object_unref(m_texture);
	m_texture = texture;
	queue_draw();
}

void MachineView::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot> &snapshot)
{
	GtkSnapshot *snap = snapshot->gobj();
	graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, float(get_width()), float(get_height()));
	if (!m_texture)
	{
		GdkRGBA black = {0, 0, 0, 1};
		gtk_snapshot_append_color(snap, &black, &bounds);
		return;
	}
	// The frame is a whole multiple of the machine's pixels. It keeps its
	// shape, centred, with black bars where the view's shape differs,
	// and the GPU scales it: sharp where the drawn size is a whole
	// multiple of the frame, smoothed at any other size, which would
	// make uneven pixels.
	double scale = get_scale_factor();
	if (GtkNative *native = gtk_widget_get_native(GTK_WIDGET(gobj())))
		if (GdkSurface *surface = gtk_native_get_surface(native))
			scale = gdk_surface_get_scale(surface);
	const int view_width = int(std::lround(get_width() * scale));
	const int view_height = int(std::lround(get_height() * scale));
	int x, y, width, height;
	const bool whole = pinesy_frame_rect(gdk_texture_get_width(m_texture), gdk_texture_get_height(m_texture),
			view_width, view_height, &x, &y, &width, &height) != 0;
	GdkRGBA black = {0, 0, 0, 1};
	gtk_snapshot_append_color(snap, &black, &bounds);
	graphene_rect_t drawn = GRAPHENE_RECT_INIT(float(x / scale), float(y / scale), float(width / scale), float(height / scale));
	gtk_snapshot_append_scaled_texture(snap, m_texture, whole ? GSK_SCALING_FILTER_NEAREST : GSK_SCALING_FILTER_LINEAR, &drawn);
}

void MachineView::size_allocate_vfunc(int width, int height, int)
{
	double scale = get_scale_factor();
	if (auto native = get_native())
		if (GdkSurface *surface = gtk_native_get_surface(native->gobj()))
			scale = gdk_surface_get_scale(surface);
	pinesy_view_size(width, height, int(std::lround(width * scale)), int(std::lround(height * scale)));
	m_allocated = true;
}

void MachineView::measure_vfunc(Gtk::Orientation orientation, int, int &minimum, int &natural,
		int &minimum_baseline, int &natural_baseline) const
{
	minimum = 1;
	natural = orientation == Gtk::Orientation::HORIZONTAL ? 640 : 480;
	minimum_baseline = natural_baseline = -1;
}

//------------------------------------------------------------
//  MachineWindow
//------------------------------------------------------------

namespace {

// The XKB keycode of the event being handled.
unsigned event_keycode(GtkEventControllerKey *controller, guint fallback)
{
	GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
	return event ? gdk_key_event_get_keycode(event) : fallback;
}

gboolean key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType, gpointer data)
{
	return static_cast<MachineWindow *>(data)->on_key_pressed(keyval, event_keycode(controller, keycode));
}

void key_released(GtkEventControllerKey *controller, guint, guint keycode, GdkModifierType, gpointer data)
{
	static_cast<MachineWindow *>(data)->on_key_released(event_keycode(controller, keycode));
}

void pointer_enter(GtkEventControllerMotion *, double x, double y, gpointer data)
{
	static_cast<MachineWindow *>(data)->on_pointer_enter(x, y);
}

void pointer_motion(GtkEventControllerMotion *, double x, double y, gpointer data)
{
	static_cast<MachineWindow *>(data)->on_pointer_motion(x, y);
}

void pointer_leave(GtkEventControllerMotion *, gpointer data)
{
	static_cast<MachineWindow *>(data)->on_pointer_leave();
}

// MAME's mouse buttons: left, right, middle, then back and forward
int mame_button(guint button)
{
	switch (button)
	{
	case GDK_BUTTON_PRIMARY: return 0;
	case GDK_BUTTON_SECONDARY: return 1;
	case GDK_BUTTON_MIDDLE: return 2;
	case 8: return 3;
	case 9: return 4;
	default: return -1;
	}
}

// Every button press and release over the view, also while another
// button is held, goes to MAME.
gboolean button_event(GtkEventControllerLegacy *, GdkEvent *event, gpointer)
{
	const GdkEventType type = gdk_event_get_event_type(event);
	if (type != GDK_BUTTON_PRESS && type != GDK_BUTTON_RELEASE)
		return FALSE;
	const int button = mame_button(gdk_button_event_get_button(event));
	if (button >= 0)
		pinesy_mouse_button(button, type == GDK_BUTTON_PRESS ? 1 : 0);
	return FALSE;
}

// A mouse wheel turns in notches; GTK gives them as wheel units, with
// dy positive towards the user.
gboolean wheel_scrolled(GtkEventControllerScroll *controller, double, double dy, gpointer)
{
	if (gtk_event_controller_scroll_get_unit(controller) != GDK_SCROLL_UNIT_WHEEL)
		return FALSE;
	pinesy_mouse_wheel(-dy);
	return TRUE;
}

} // namespace

MachineWindow::MachineWindow(const std::string &title, int window_size) : m_window_size(window_size)
{
	m_window = ADW_WINDOW(adw_window_new());
	g_object_ref_sink(m_window);
	gtk_window_set_title(GTK_WINDOW(m_window), title.c_str());
	gtk_window_set_default_size(GTK_WINDOW(m_window), 640, 480);

	m_view = Gtk::make_managed<MachineView>();
	m_header = adw_header_bar_new();
	// in full screen the header bar has a button to leave it, as keys
	// go to the machine
	GtkWidget *leave = gtk_button_new_from_icon_name("view-restore-symbolic");
	gtk_widget_set_tooltip_text(leave, "Leave Full Screen");
	gtk_widget_set_focusable(leave, FALSE);
	gtk_widget_set_visible(leave, FALSE);
	g_signal_connect_swapped(leave, "clicked", G_CALLBACK(gtk_window_unfullscreen), m_window);
	adw_header_bar_pack_end(ADW_HEADER_BAR(m_header), leave);
	GtkWidget *toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), m_header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(m_view->gobj()));
	adw_window_set_content(m_window, toolbar);

	// Keys go to the machine before anything in the window sees them,
	// so the machine gets every key, shortcuts and Tab included.
	GtkEventController *keys = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
	g_signal_connect(keys, "key-pressed", G_CALLBACK(key_pressed), this);
	g_signal_connect(keys, "key-released", G_CALLBACK(key_released), this);
	gtk_widget_add_controller(GTK_WIDGET(m_window), keys);

	// the mouse over the view
	GtkWidget *view = GTK_WIDGET(m_view->gobj());
	GtkEventController *motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "enter", G_CALLBACK(pointer_enter), this);
	g_signal_connect(motion, "motion", G_CALLBACK(pointer_motion), this);
	g_signal_connect(motion, "leave", G_CALLBACK(pointer_leave), this);
	gtk_widget_add_controller(view, motion);
	GtkEventController *buttons = gtk_event_controller_legacy_new();
	g_signal_connect(buttons, "event", G_CALLBACK(button_event), nullptr);
	gtk_widget_add_controller(view, buttons);
	GtkEventController *wheel = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect(wheel, "scroll", G_CALLBACK(wheel_scrolled), nullptr);
	gtk_widget_add_controller(view, wheel);

	m_active_handler = connect_notify(m_window, "is-active", [this] { on_active_changed(); });
	m_fullscreened_handler = connect_notify(m_window, "fullscreened", [this, leave] {
		gtk_widget_set_visible(leave, gtk_window_is_fullscreen(GTK_WINDOW(m_window)));
	});
	g_signal_connect(m_window, "close-request", G_CALLBACK(+[](GtkWindow *, gpointer data) -> gboolean {
		auto self = static_cast<MachineWindow *>(data);
		self->m_closed = true;
		if (self->m_running)
			pinesy_request_exit();
		return TRUE;   // the window goes when the run has ended
	}), this);
}

MachineWindow::~MachineWindow()
{
	g_signal_handlers_disconnect_by_data(m_window, this);
	g_signal_handler_disconnect(m_window, m_active_handler);
	g_signal_handler_disconnect(m_window, m_fullscreened_handler);
	gtk_window_destroy(GTK_WINDOW(m_window));
	g_object_unref(m_window);
	// let GTK finish with the window before the caller goes on
	while (g_main_context_iteration(nullptr, FALSE))
		;
}

bool MachineWindow::show_and_wait()
{
	gtk_window_present(GTK_WINDOW(m_window));
	m_view->grab_focus();
	while (!m_closed && !(m_view->allocated() && gtk_widget_get_mapped(GTK_WIDGET(m_window))))
		g_main_context_iteration(nullptr, TRUE);
	if (m_closed)
		return false;
	pinesy_focus(gtk_window_is_active(GTK_WINDOW(m_window)) ? 1 : 0);
	return true;
}

pinesy_host MachineWindow::host()
{
	pinesy_host host = {};
	host.view = m_view;   // any non-null value: the window shows the first screen
	host.frame = on_frame;
	host.output = on_output;
	host.size = on_size;
	host.pump = on_pump;
	host.pointer = on_pointer;
	host.context = this;
	return host;
}

void MachineWindow::on_frame(void *context, const void *pixels, int width, int height, int pitch)
{
	static_cast<MachineWindow *>(context)->m_view->show_frame(pixels, width, height, pitch);
}

void MachineWindow::on_output(void *context, int channel, const char *text)
{
	if (channel == 0 && text)
		static_cast<MachineWindow *>(context)->m_errors += text;
}

void MachineWindow::on_size(void *context, int width, int height)
{
	auto self = static_cast<MachineWindow *>(context);
	if (width <= 0 || height <= 0)
		return;

	// The size is the frame's, in pixels. The core gives the view's size
	// from it, the window size and the monitor the window is on; the
	// header bar comes on top of the view. In full screen this is the
	// size the window takes when it leaves full screen.
	int header = 0;
	gtk_widget_measure(self->m_header, GTK_ORIENTATION_VERTICAL, -1, nullptr, &header, nullptr, nullptr);

	GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(self->m_window));
	GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(self->m_window));
	GdkMonitor *monitor = surface ? gdk_display_get_monitor_at_surface(display, surface) : nullptr;
	const double scale = surface ? gdk_surface_get_scale(surface) : 1.0;
	GdkRectangle area = {0, 0, 0, 0};
	if (monitor)
		gdk_monitor_get_geometry(monitor, &area);
	const core::window::ContentSize view = core::window::content_size(width, height, self->m_window_size, scale,
			area.width, area.height - header);
	if (view.width <= 0)
		return;
	gtk_window_set_default_size(GTK_WINDOW(self->m_window), view.width, view.height + header);
	if (!self->m_sized)
	{
		self->m_sized = true;
		if (core::window::opens_full_screen(self->m_window_size))
			gtk_window_fullscreen(GTK_WINDOW(self->m_window));
	}
}

void MachineWindow::on_pump(void *)
{
	while (g_main_context_iteration(nullptr, FALSE))
		;
}

// GTK has no call that holds the pointer in a window, so capture is
// not done: the pointer is hidden over the view, and the machine gets
// the mouse's movement while the pointer is over the view or a drag
// that started there goes on.
void MachineWindow::on_pointer(void *context, int hide, int)
{
	auto self = static_cast<MachineWindow *>(context);
	gtk_widget_set_cursor_from_name(GTK_WIDGET(self->m_view->gobj()), hide ? "none" : nullptr);
}

void MachineWindow::on_pointer_enter(double x, double y)
{
	m_has_pointer = true;
	m_pointer_x = x;
	m_pointer_y = y;
	pinesy_mouse_inside(1);
}

void MachineWindow::on_pointer_motion(double x, double y)
{
	if (m_has_pointer)
		pinesy_mouse_move(x - m_pointer_x, y - m_pointer_y);
	m_has_pointer = true;
	m_pointer_x = x;
	m_pointer_y = y;
	GtkWidget *view = GTK_WIDGET(m_view->gobj());
	const bool inside = x >= 0 && y >= 0 && x < gtk_widget_get_width(view) && y < gtk_widget_get_height(view);
	pinesy_mouse_inside(inside ? 1 : 0);
}

void MachineWindow::on_pointer_leave()
{
	m_has_pointer = false;
	pinesy_mouse_inside(0);
}

bool MachineWindow::on_key_pressed(unsigned keyval, unsigned keycode)
{
	// GTK repeats a held key as more presses; the machine wants one
	if (m_keys_down.insert(keycode).second)
		pinesy_key(int(keycode), 1);
	if (gunichar ch = gdk_keyval_to_unicode(keyval))
		pinesy_char(ch);
	return true;
}

void MachineWindow::on_key_released(unsigned keycode)
{
	if (m_keys_down.erase(keycode))
		pinesy_key(int(keycode), 0);
}

void MachineWindow::on_active_changed()
{
	bool active = gtk_window_is_active(GTK_WINDOW(m_window));
	// keys held when the window loses the focus are released elsewhere
	if (!active)
		release_keys();
	pinesy_focus(active ? 1 : 0);
}

void MachineWindow::release_keys()
{
	for (unsigned keycode : m_keys_down)
		pinesy_key(int(keycode), 0);
	m_keys_down.clear();
}

//------------------------------------------------------------
//  MachineRunner
//------------------------------------------------------------

void MachineRunner::start(const core::Core &core, const std::string &title, const std::vector<std::string> &arguments,
		bool debug, int window_size, bool mouse)
{
	if (m_running)
		return;
	m_running = true;
	m_app.hold();
	m_changed.emit();

	// pinesy_run() keeps its own loop. Start it from an idle callback, so
	// the action or click that asked for the run has been handled first.
	Glib::signal_idle().connect_once([this, &core, title, arguments, debug, window_size, mouse] {
		run(core, title, arguments, debug, window_size, mouse);
	});
}

void MachineRunner::run(const core::Core &core, const std::string &title, const std::vector<std::string> &arguments,
		bool debug, int window_size, bool mouse)
{
	int code = 0;
	bool ran = false;
	std::string errors;
	{
		MachineWindow window(title, window_size);
		if (window.show_and_wait())
		{
			pinesy_host host = window.host();
			window.set_running(true);
			code = core.run(arguments, debug, mouse, &host);
			window.set_running(false);
			ran = true;
		}
		errors = window.errors();
	}

	m_running = false;
	m_changed.emit();
	m_app.release();

	if (ran && code != 0)
		m_failed.emit(title, code, errors);
}

void MachineRunner::stop()
{
	if (m_running)
		pinesy_request_exit();
}

void MachineRunner::reset(bool hard)
{
	if (m_running)
		pinesy_reset(hard ? 1 : 0);
}

//------------------------------------------------------------
//  Headless mode with a window
//------------------------------------------------------------

int run_headless_in_window(const core::Core &core, const std::vector<std::string> &arguments)
{
	gtk_init();
	Gtk::init_gtkmm_internals();
	adw_init();

	std::vector<std::string> mame = arguments;
	if (!mame.empty() && mame.front() == "--")
		mame.erase(mame.begin());
	auto driver = std::find_if(mame.begin(), mame.end(), [](const std::string &a) { return a.empty() || a[0] != '-'; });
	std::string title = driver != mame.end() ? *driver : "Pi/Ne-Sy";

	MachineWindow window(title, core::window::fit);
	// A window closed before the run still runs MAME, so the exit code
	// is MAME's. The engine keeps an exit request made before the
	// machine starts, so the machine stops at once.
	if (!window.show_and_wait())
		pinesy_request_exit();
	pinesy_host host = window.host();
	window.set_running(true);
	return core.headless(arguments, &host);
}

} // namespace pinesy::gtk
