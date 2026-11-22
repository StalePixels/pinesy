// The window a launch with no file to open shows when there are recent
// configurations: the recent .pinesy files, Open, and Create a New
// Machine…. With no recent configuration, the launch opens a new
// configuration instead (Application::launch).

#ifndef PINESY_GTK_RECENT_WINDOW_HPP
#define PINESY_GTK_RECENT_WINDOW_HPP

#include "pinesy_core.hpp"

#include <adwaita.h>

#include <functional>
#include <vector>

namespace pinesy::gtk {

class Application;

class RecentWindow
{
public:
	// A window with the entries, the first one selected. The window
	// owns the object; closed calls back when the window goes.
	static RecentWindow *create(Application &app, std::vector<core::recent::Entry> entries,
			std::function<void()> closed);

	void present();
	void destroy();

	RecentWindow(const RecentWindow &) = delete;
	RecentWindow &operator=(const RecentWindow &) = delete;
	~RecentWindow();

private:
	RecentWindow(Application &app, std::vector<core::recent::Entry> entries, std::function<void()> closed);

	Application &m_app;
	std::vector<core::recent::Entry> m_entries;
	std::function<void()> m_closed;
	AdwApplicationWindow *m_window = nullptr;
	GtkListBox *m_list = nullptr;
	GtkWidget *m_open = nullptr;

	void open_selected();
};

} // namespace pinesy::gtk

#endif
