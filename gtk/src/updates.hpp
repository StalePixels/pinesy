// Updates of a CI build. A Flatpak build reads its ref in its track's
// Flatpak repository at startup, and the Flatpak portal tells it of a
// newer build while it runs and installs it; a Debian package reads its
// track's apt index. The core decides when the out-of-date notice
// shows, and what it says.

#ifndef PINESY_GTK_UPDATES_HPP
#define PINESY_GTK_UPDATES_HPP

#include <giomm/dbusconnection.h>

#include <functional>
#include <string>
#include <string_view>

namespace pinesy::gtk {

class Application;

// The variable in the environment of a new build that the Flatpak
// portal starts after an update. That build replaces the running
// instance on the session bus (G_APPLICATION_REPLACE).
inline constexpr const char *replace_variable = "PINESY_REPLACE";

class Updates
{
public:
	explicit Updates(Application &app);

	// Looks for a newer build, once, when the core says this build looks
	// for updates (a CI build with update checks on).
	void start();

private:
	Application &m_app;
	Glib::RefPtr<Gio::DBus::Connection> m_bus;
	std::string m_monitor;   // the portal's UpdateMonitor object
	bool m_updating = false;
	std::string m_noticed;   // the build the notice last showed

	void start_flatpak();
	void check_flatpak_ref();
	void start_deb();
	void get(const std::string &url, std::function<void(std::string_view)> done);
	void notice(const std::string &build, bool newer_known);
	void chosen(const std::string &response, const std::string &build);
	void update_flatpak();
	void progress(const Glib::VariantContainerBase &parameters);
	void failed(const std::string &error, const std::string &message);
	void spawn_latest();
};

} // namespace pinesy::gtk

#endif
