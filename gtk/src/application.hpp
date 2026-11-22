// The app: configuration windows, the app actions and Settings.

#ifndef PINESY_GTK_APPLICATION_HPP
#define PINESY_GTK_APPLICATION_HPP

#include "machine_runner.hpp"
#include "pinesy_core.hpp"
#include "updates.hpp"

#include <giomm/menu.h>
#include <giomm/settings.h>
#include <giomm/simpleaction.h>
#include <gtkmm/application.h>
#include <gtkmm/recentmanager.h>

#include <memory>
#include <vector>

namespace pinesy::gtk {

class ConfigurationWindow;
class RecentWindow;

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

	// The recent configurations to show, newest first, from GTK's
	// recent files (the core's rules: files that exist, each once, at
	// most its limit). Only the files Pinesy put there are read.
	std::vector<core::recent::Entry> recent_configurations() const;

	// Puts a configuration that was opened or saved first in the
	// recent configurations.
	void note_recent(const Glib::RefPtr<Gio::File> &file);

	// The Open Recent menu, which every configuration window's main
	// menu shows.
	const Glib::RefPtr<Gio::Menu> &recent_menu() const { return m_recent_menu; }

	// Shows an alert on a window, by default the active window, or on
	// its own when there is none.
	void alert(const std::string &heading, const std::string &body, Gtk::Window *window = nullptr);

	// Quits after an update started the new build, as Quit does.
	void quit_after_update() { quit_app(); }

protected:
	explicit Application(Glib::RefPtr<Gio::Settings> settings);

	void on_startup() override;
	void on_activate() override;
	void on_open(const type_vec_files &files, const Glib::ustring &hint) override;

private:
	Glib::RefPtr<Gio::Settings> m_settings;
	std::unique_ptr<core::Core> m_core;
	MachineRunner m_runner;
	Updates m_updates{ *this };
	Glib::RefPtr<Gtk::RecentManager> m_recent;
	Glib::RefPtr<Gio::Menu> m_recent_menu;
	Glib::RefPtr<Gio::SimpleAction> m_clear_recent;
	// the recent configurations window, while it is open
	RecentWindow *m_recent_window = nullptr;

	ConfigurationWindow *active_configuration();
	void open();
	// What a launch with no file to open shows: the recent
	// configurations, or a new configuration when there is none.
	void launch();
	void update_recent_menu();
	void clear_recent();
	void close_recent_window();
	void quit_app();
};

} // namespace pinesy::gtk

#endif
