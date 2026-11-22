#include "application.hpp"

#include "configuration_window.hpp"
#include "recent_window.hpp"
#include "settings.hpp"
#include "settings_dialog.hpp"
#include "signals.hpp"

#include <adwaita.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>
#include <giomm/liststore.h>
#include <giomm/menuitem.h>

#include <algorithm>

namespace pinesy::gtk {

namespace {

// The application name of Pinesy's entries in GTK's recent files.
constexpr const char *recent_app_name = PINESY_APP_ID;

// A menu item's label shows "_" as a mnemonic; a file name keeps its own.
std::string menu_label(const std::string &text)
{
	std::string label;
	for (char c : text)
	{
		if (c == '_')
			label += '_';
		label += c;
	}
	return label;
}

} // namespace

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
	add_action_with_parameter("open-recent", Glib::VARIANT_TYPE_STRING, [this](const Glib::VariantBase &path) {
		open_file(Gio::File::create_for_path(
				Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(path).get().raw()));
	});
	m_clear_recent = add_action("clear-recent", [this] { clear_recent(); });
	m_recent = Gtk::RecentManager::get_default();
	m_recent_menu = Gio::Menu::create();
	m_recent->signal_changed().connect([this] { update_recent_menu(); });
	update_recent_menu();
	add_action("settings", [this] {
		auto window = get_active_window();
		present_settings_dialog(window ? GTK_WIDGET(window->gobj()) : nullptr, m_settings);
	});
	auto stop = add_action("stop", [this] { m_runner.stop(); });
	auto reset = add_action("reset", [this] { m_runner.reset(false); });
	auto hard_reset = add_action("hard-reset", [this] { m_runner.reset(true); });
	auto toggle_pointer = add_action("toggle-pointer", [this] { m_runner.toggle_pointer(); });
	stop->set_enabled(false);
	reset->set_enabled(false);
	hard_reset->set_enabled(false);
	toggle_pointer->set_enabled(false);
	m_runner.signal_changed().connect([this, stop, reset, hard_reset, toggle_pointer] {
		stop->set_enabled(m_runner.running());
		reset->set_enabled(m_runner.running());
		hard_reset->set_enabled(m_runner.running());
		toggle_pointer->set_enabled(m_runner.running());
	});
	add_action("quit", [this] { quit_app(); });

	set_accel_for_action("app.new", "<Control>n");
	set_accel_for_action("app.open", "<Control>o");
	set_accel_for_action("app.settings", "<Control>comma");
	set_accel_for_action("app.stop", "<Control>period");
	set_accel_for_action("app.toggle-pointer", "<Control><Shift>m");
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
	if (m_recent_window)
		m_recent_window->present();
	else if (get_windows().empty())
		launch();
	else
		new_configuration();
}

void Application::launch()
{
	auto entries = recent_configurations();
	if (core::recent::launch(false, entries.size()) == core::recent::Launch::recent)
	{
		m_recent_window = RecentWindow::create(*this, entries, [this] { m_recent_window = nullptr; });
		m_recent_window->present();
	}
	else
	{
		new_configuration();
	}
}

void Application::close_recent_window()
{
	if (m_recent_window)
		m_recent_window->destroy();
}

std::vector<core::recent::Entry> Application::recent_configurations() const
{
	std::vector<Glib::RefPtr<Gtk::RecentInfo>> items;
	for (const auto &item : m_recent->get_items())
		if (item->has_application(recent_app_name))
			items.push_back(item);
	std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
		return a->get_modified().to_unix() > b->get_modified().to_unix();
	});
	std::vector<std::string> paths;
	for (const auto &item : items)
		paths.push_back(Gio::File::create_for_uri(item->get_uri())->get_path());
	return core::recent::entries(paths);
}

void Application::note_recent(const Glib::RefPtr<Gio::File> &file)
{
	Gtk::RecentManager::Data data;
	data.app_name = recent_app_name;
	// never a file argument: any argument starts headless mode
	data.app_exec = "pinesy";
	data.mime_type = "application/x-pinesy";
	m_recent->add_item(file->get_uri(), data);
}

void Application::update_recent_menu()
{
	auto entries = recent_configurations();
	m_recent_menu->remove_all();
	auto files = Gio::Menu::create();
	for (const auto &entry : entries)
	{
		auto item = Gio::MenuItem::create(menu_label(entry.name), "");
		item->set_action_and_target("app.open-recent", Glib::Variant<Glib::ustring>::create(entry.path));
		files->append_item(item);
	}
	m_recent_menu->append_section(files);
	auto clearing = Gio::Menu::create();
	clearing->append("_Clear Menu", "app.clear-recent");
	m_recent_menu->append_section(clearing);
	m_clear_recent->set_enabled(!entries.empty());
}

void Application::clear_recent()
{
	for (const auto &item : m_recent->get_items())
	{
		if (!item->has_application(recent_app_name))
			continue;
		try
		{
			m_recent->remove_item(item->get_uri());
		}
		catch (const Glib::Error &)
		{
			// already gone
		}
	}
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
	close_recent_window();
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
		close_recent_window();
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
