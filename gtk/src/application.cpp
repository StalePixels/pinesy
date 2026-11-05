#include "application.hpp"

#include "configuration_window.hpp"
#include "settings.hpp"
#include "settings_dialog.hpp"
#include "signals.hpp"

#include <adwaita.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>
#include <giomm/liststore.h>

namespace pinesy::gtk {

Glib::RefPtr<Application> Application::create(Glib::RefPtr<Gio::Settings> settings)
{
	return Glib::make_refptr_for_instance<Application>(new Application(std::move(settings)));
}

Application::Application(Glib::RefPtr<Gio::Settings> settings)
	: Gtk::Application(PINESY_APP_ID, Gio::Application::Flags::HANDLES_OPEN)
	, m_settings(std::move(settings))
	, m_core(make_core(m_settings))
	, m_runner(*this)
{
	m_settings->signal_changed().connect([this](const Glib::ustring &) {
		m_core->set_settings(core_settings(m_settings));
	});
	m_runner.signal_failed().connect([this](const std::string &title, int code, const std::string &errors) {
		std::string text = errors;
		auto first = text.find_first_not_of(" \t\r\n");
		auto last = text.find_last_not_of(" \t\r\n");
		text = first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
		if (text.empty())
			text = "The emulator exited with code " + std::to_string(code) + ".";
		alert(title + " stopped with an error", text);
	});
}

void Application::on_startup()
{
	Gtk::Application::on_startup();
	adw_init();
	gtk_window_set_default_icon_name(PINESY_APP_ID);

	add_action("new", [this] { new_configuration(); });
	add_action("open", [this] { open(); });
	add_action("settings", [this] {
		auto window = get_active_window();
		present_settings_dialog(window ? GTK_WIDGET(window->gobj()) : nullptr, m_settings);
	});
	auto stop = add_action("stop", [this] { m_runner.stop(); });
	stop->set_enabled(false);
	m_runner.signal_changed().connect([this, stop] { stop->set_enabled(m_runner.running()); });
	add_action("quit", [this] { quit_app(); });

	set_accel_for_action("app.new", "<Control>n");
	set_accel_for_action("app.open", "<Control>o");
	set_accel_for_action("app.settings", "<Control>comma");
	set_accel_for_action("app.stop", "<Control>period");
	set_accel_for_action("app.quit", "<Control>q");
	set_accel_for_action("win.save", "<Control>s");
	set_accel_for_action("win.save-as", "<Control><Shift>s");
	set_accel_for_action("win.run", "<Control>r");
	set_accel_for_action("win.run-debug", "<Control><Shift>r");
	set_accel_for_action("win.copy-command-line", "<Control><Shift>c");
	set_accel_for_action("window.close", "<Control>w");

	// The machine list comes from the engine, which must not be asked
	// while a machine runs, so it is built before anything can run.
	m_core->catalogue();
}

void Application::on_activate()
{
	new_configuration();
}

void Application::on_open(const type_vec_files &files, const Glib::ustring &)
{
	for (const auto &file : files)
		open_file(file);
}

ConfigurationWindow *Application::new_configuration()
{
	auto window = ConfigurationWindow::create(*this);
	window->present();
	window->choose_machine();
	return window;
}

ConfigurationWindow *Application::active_configuration()
{
	auto window = get_active_window();
	return window ? ConfigurationWindow::from(*window) : nullptr;
}

void Application::open_file(const Glib::RefPtr<Gio::File> &file)
{
	// a new window becomes the active one, so take the active window first
	Gtk::Window *active = get_active_window();
	auto window = active_configuration();
	bool fresh = !window || !window->is_empty();
	if (fresh)
		window = ConfigurationWindow::create(*this);
	std::string error;
	if (window->load(file, error))
	{
		window->present();
		return;
	}
	if (fresh)
		window->destroy();
	alert("Could not open “" + file->get_basename() + "”", error, active);
}

void Application::open()
{
	auto dialog = Gtk::FileDialog::create();
	dialog->set_title("Open Configuration");
	auto filter = Gtk::FileFilter::create();
	filter->set_name("Pinesy configurations");
	filter->add_suffix("pinesy");
	auto filters = Gio::ListStore<Gtk::FileFilter>::create();
	filters->append(filter);
	dialog->set_filters(filters);
	dialog->set_default_filter(filter);

	auto done = [this, dialog](const Glib::RefPtr<Gio::AsyncResult> &result) {
		try
		{
			if (auto file = dialog->open_finish(result))
				open_file(file);
		}
		catch (const Glib::Error &)
		{
			// dismissed
		}
	};
	if (auto window = get_active_window())
		dialog->open(*window, done);
	else
		dialog->open(done);
}

void Application::alert(const std::string &heading, const std::string &body, Gtk::Window *window)
{
	AdwDialog *dialog = adw_alert_dialog_new(heading.c_str(), body.c_str());
	adw_alert_dialog_set_body_use_markup(ADW_ALERT_DIALOG(dialog), FALSE);
	adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "close", "_Close");
	if (!window)
		window = get_active_window();
	adw_dialog_present(dialog, window ? GTK_WIDGET(window->gobj()) : nullptr);
}

void Application::quit_app()
{
	// Each window asks about unsaved changes as it closes. The app ends
	// when the last window has closed and no machine runs.
	m_runner.stop();
	for (auto window : get_windows())
		window->close();
}

} // namespace pinesy::gtk
