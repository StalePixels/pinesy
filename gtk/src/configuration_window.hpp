// The configuration window: the machine, its hardware, its media and
// its command line. Everything shown comes from the core's summary of
// the document's command line, asked again after every change.

#ifndef PINESY_GTK_CONFIGURATION_WINDOW_HPP
#define PINESY_GTK_CONFIGURATION_WINDOW_HPP

#include "pinesy_core.hpp"

#include <adwaita.h>
#include <giomm/file.h>
#include <giomm/simpleaction.h>
#include <gtkmm/window.h>
#include <sigc++/connection.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pinesy::gtk {

class Application;

class ConfigurationWindow
{
public:
	// A window with no machine. The window owns the object.
	static ConfigurationWindow *create(Application &app);

	// The configuration window a GTK window is, or nullptr.
	static ConfigurationWindow *from(Gtk::Window &window);

	void present();
	void destroy();

	// Asks for the machine, with the machine picker.
	void choose_machine();

	// Reads a .pinesy file into this window. Returns false, with the
	// reason in error, when the file cannot be read.
	bool load(const Glib::RefPtr<Gio::File> &file, std::string &error);

	// True for a window with no machine and no file.
	bool is_empty() const { return m_document.arguments.empty() && !m_file; }

	ConfigurationWindow(const ConfigurationWindow &) = delete;
	ConfigurationWindow &operator=(const ConfigurationWindow &) = delete;
	~ConfigurationWindow();

private:
	explicit ConfigurationWindow(Application &app);

	Application &m_app;
	AdwApplicationWindow *m_window = nullptr;
	AdwWindowTitle *m_title = nullptr;
	AdwToastOverlay *m_toasts = nullptr;
	GtkStack *m_stack = nullptr;
	AdwPreferencesPage *m_page = nullptr;
	GtkWidget *m_stop = nullptr;
	std::vector<AdwPreferencesGroup *> m_groups;

	Glib::RefPtr<Gio::SimpleAction> m_run;
	Glib::RefPtr<Gio::SimpleAction> m_run_debug;
	Glib::RefPtr<Gio::SimpleAction> m_copy;
	Glib::RefPtr<Gio::SimpleAction> m_clear_nvram;

	core::Document m_document;
	Glib::RefPtr<Gio::File> m_file;
	bool m_modified = false;
	bool m_close_confirmed = false;
	// Custom was chosen for the window size, so the magnification row
	// stays while the user enters a listed magnification in it.
	bool m_custom_window_size = false;
	std::optional<core::Summary> m_summary;
	// the command line's label in the form
	GtkWidget *m_command_line = nullptr;
	// The configuration changed while a machine ran; the engine is asked
	// about it when the run ends.
	bool m_refresh_waiting = false;
	std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
	sigc::connection m_runner_changed;

	Gtk::Window &window();
	std::string title() const;
	void update_title();
	void update_actions();
	void on_runner_changed();
	bool on_close_request();

	void set_arguments(std::vector<std::string> arguments);
	void edit(std::function<std::vector<std::string>()> change);
	void refresh();
	void build_form(const core::Summary &summary);
	void add_group(AdwPreferencesGroup *group);
	AdwPreferencesGroup *message_group(const char *title, const std::string &text, const char *icon,
			const char *style);
	AdwPreferencesGroup *machine_group(const core::Summary &summary);
	AdwPreferencesGroup *hardware_group(const core::Summary &summary);
	AdwPreferencesGroup *media_group(const core::Summary &summary);
	AdwPreferencesGroup *command_line_group();
	// The machine takes a mouse, so the mouse setting applies.
	bool takes_mouse() const;
	std::string command_line();
	void choose_media(const core::Media &media);

	void run(bool debug);
	void copy_command_line();
	// Asks, then deletes the NVRAM MAME saved for the configuration's driver.
	void clear_nvram();
	void save(std::function<void()> then = {});
	void save_as(std::function<void()> then = {});
	bool write(const Glib::RefPtr<Gio::File> &file);
	void toast(const std::string &text);
};

} // namespace pinesy::gtk

#endif
