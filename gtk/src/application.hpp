// The app: configuration windows, the app actions and Settings.

#ifndef PINESY_GTK_APPLICATION_HPP
#define PINESY_GTK_APPLICATION_HPP

#include "machine_runner.hpp"
#include "pinesy_core.hpp"

#include <giomm/settings.h>
#include <gtkmm/application.h>

#include <memory>

namespace pinesy::gtk {

class ConfigurationWindow;

class Application : public Gtk::Application
{
public:
	static Glib::RefPtr<Application> create(Glib::RefPtr<Gio::Settings> settings);

	core::Core &core() { return *m_core; }
	MachineRunner &runner() { return m_runner; }
	const Glib::RefPtr<Gio::Settings> &settings() const { return m_settings; }

	// A new configuration window, which asks for the machine.
	ConfigurationWindow *new_configuration();

	// Opens a .pinesy file: in the active window when that has no
	// machine and no file yet, else in a new window.
	void open_file(const Glib::RefPtr<Gio::File> &file);

	// Shows an alert on a window, by default the active window, or on
	// its own when there is none.
	void alert(const std::string &heading, const std::string &body, Gtk::Window *window = nullptr);

protected:
	explicit Application(Glib::RefPtr<Gio::Settings> settings);

	void on_startup() override;
	void on_activate() override;
	void on_open(const type_vec_files &files, const Glib::ustring &hint) override;

private:
	Glib::RefPtr<Gio::Settings> m_settings;
	std::unique_ptr<core::Core> m_core;
	MachineRunner m_runner;

	ConfigurationWindow *active_configuration();
	void open();
	void quit_app();
};

} // namespace pinesy::gtk

#endif
