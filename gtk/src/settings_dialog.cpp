#include "settings_dialog.hpp"

#include "pinesy_core.hpp"
#include "settings.hpp"
#include "signals.hpp"

#include <adwaita.h>
#include <glibmm/markup.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/window.h>
#include <sigc++/connection.h>

#include <algorithm>
#include <string>
#include <vector>

namespace pinesy::gtk {

namespace {

class SettingsDialog
{
public:
	explicit SettingsDialog(Glib::RefPtr<Gio::Settings> settings);
	~SettingsDialog() { m_changed.disconnect(); }

	AdwDialog *dialog() const { return ADW_DIALOG(m_dialog); }

private:
	Glib::RefPtr<Gio::Settings> m_settings;
	AdwPreferencesDialog *m_dialog = nullptr;
	AdwPreferencesPage *m_general = nullptr;
	AdwPreferencesGroup *m_rom_group = nullptr;
	AdwPreferencesGroup *m_data_group = nullptr;
	GtkWidget *m_windows = nullptr;
	GtkWidget *m_gdb_stub = nullptr;
	GtkWidget *m_port = nullptr;
	sigc::connection m_changed;
	Connections m_connections;

	std::vector<std::string> rom_folders() const;
	std::string data_folder() const;
	void build_general();
	void update_debugger();
	void choose_folder(const char *title, std::function<void(const std::string &)> chosen);
	void reject_rom_folder(const std::string &path);
};

SettingsDialog::SettingsDialog(Glib::RefPtr<Gio::Settings> settings) : m_settings(std::move(settings))
{
	m_dialog = ADW_PREFERENCES_DIALOG(adw_preferences_dialog_new());

	// General: ROM folders and the data folder.
	m_general = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
	adw_preferences_page_set_title(m_general, "General");
	adw_preferences_page_set_icon_name(m_general, "preferences-system-symbolic");
	build_general();
	adw_preferences_dialog_add(m_dialog, m_general);

	// Debugger: what Run with Debugger uses.
	auto debugger = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
	adw_preferences_page_set_title(debugger, "Debugger");
	adw_preferences_page_set_icon_name(debugger, "system-run-symbolic");
	auto group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, "Run with Debugger uses");
	adw_preferences_group_set_description(group,
			"GDB clients connect to these ports: the Next’s, and the Pi’s when the configuration has a Pi SD card. "
			"With Start paused, the Next waits at its first instruction until you continue it.");

	auto choice_row = [group](const char *title, const char *subtitle, GtkWidget *check) {
		GtkWidget *row = adw_action_row_new();
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
		adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
		gtk_widget_set_valign(check, GTK_ALIGN_CENTER);
		adw_action_row_add_prefix(ADW_ACTION_ROW(row), check);
		adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), check);
		adw_preferences_group_add(group, row);
	};
	m_windows = gtk_check_button_new();
	m_gdb_stub = gtk_check_button_new();
	gtk_check_button_set_group(GTK_CHECK_BUTTON(m_gdb_stub), GTK_CHECK_BUTTON(m_windows));
	choice_row("MAME’s GTK debugger windows", "MAME’s own debugger for the machine", m_windows);
	choice_row("GDB remote stub", "For a GDB client on this computer or another", m_gdb_stub);
	m_connections.add(m_windows, connect_notify(m_windows, "active", [this] {
		if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_windows)) && m_settings->get_string(key::debugger) != "windows")
			m_settings->set_string(key::debugger, "windows");
	}));
	m_connections.add(m_gdb_stub, connect_notify(m_gdb_stub, "active", [this] {
		if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_gdb_stub)) && m_settings->get_string(key::debugger) != "gdbStub")
			m_settings->set_string(key::debugger, "gdbStub");
	}));

	m_port = adw_spin_row_new_with_range(core::Settings::min_gdb_port, core::Settings::max_gdb_port, 1);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(m_port), "GDB port");
	adw_spin_row_set_digits(ADW_SPIN_ROW(m_port), 0);
	adw_spin_row_set_numeric(ADW_SPIN_ROW(m_port), TRUE);
	g_settings_bind(m_settings->gobj(), key::gdb_port, m_port, "value", G_SETTINGS_BIND_DEFAULT);
	adw_preferences_group_add(group, m_port);

	GtkWidget *pi_port = adw_spin_row_new_with_range(core::Settings::min_gdb_port, core::Settings::max_gdb_port, 1);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pi_port), "Pi GDB port");
	adw_spin_row_set_digits(ADW_SPIN_ROW(pi_port), 0);
	adw_spin_row_set_numeric(ADW_SPIN_ROW(pi_port), TRUE);
	g_settings_bind(m_settings->gobj(), key::pi_gdb_port, pi_port, "value", G_SETTINGS_BIND_DEFAULT);
	adw_preferences_group_add(group, pi_port);

	GtkWidget *start_paused = adw_switch_row_new();
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(start_paused), "Start paused");
	g_settings_bind(m_settings->gobj(), key::start_paused, start_paused, "active", G_SETTINGS_BIND_DEFAULT);
	adw_preferences_group_add(group, start_paused);
	adw_preferences_page_add(debugger, group);
	adw_preferences_dialog_add(m_dialog, debugger);
	update_debugger();

	m_changed = m_settings->signal_changed().connect([this](const Glib::ustring &name) {
		if (name == key::debugger)
			update_debugger();
		else if (name == key::rom_folders || name == key::data_folder)
			build_general();
	});
}

std::vector<std::string> SettingsDialog::rom_folders() const
{
	std::vector<std::string> folders;
	for (const auto &folder : m_settings->get_string_array(key::rom_folders))
		folders.push_back(folder);
	return folders;
}

std::string SettingsDialog::data_folder() const
{
	return m_settings->get_string(key::data_folder);
}

void SettingsDialog::build_general()
{
	if (m_rom_group)
		adw_preferences_page_remove(m_general, m_rom_group);
	if (m_data_group)
		adw_preferences_page_remove(m_general, m_data_group);

	std::string data = data_folder();
	std::string shown_data = data.empty() ? default_data_folder() : data;

	m_rom_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(m_rom_group, "ROM folders");
	adw_preferences_group_set_description(m_rom_group, "Pinesy also looks for ROMs in these folders.");
	GtkWidget *add = gtk_button_new_from_icon_name("list-add-symbolic");
	gtk_widget_add_css_class(add, "flat");
	gtk_widget_set_valign(add, GTK_ALIGN_CENTER);
	gtk_widget_set_tooltip_text(add, "Add Folder");
	connect(add, "clicked", [this] {
		choose_folder("Add ROM Folder", [this](const std::string &path) {
			auto folders = rom_folders();
			if (std::find(folders.begin(), folders.end(), path) != folders.end())
				return;
			if (!core::paths::is_usable_rom_folder(path))
			{
				reject_rom_folder(path);
				return;
			}
			folders.push_back(path);
			m_settings->set_string_array(key::rom_folders, std::vector<Glib::ustring>(folders.begin(), folders.end()));
		});
	});
	adw_preferences_group_set_header_suffix(m_rom_group, add);

	for (const auto &folder : rom_folders())
	{
		GtkWidget *row = adw_action_row_new();
		adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), folder.c_str());
		adw_action_row_set_title_lines(ADW_ACTION_ROW(row), 1);
		GtkWidget *remove = gtk_button_new_from_icon_name("user-trash-symbolic");
		gtk_widget_add_css_class(remove, "flat");
		gtk_widget_set_valign(remove, GTK_ALIGN_CENTER);
		gtk_widget_set_tooltip_text(remove, "Remove Folder");
		connect(remove, "clicked", [this, folder] {
			auto folders = rom_folders();
			folders.erase(std::remove(folders.begin(), folders.end(), folder), folders.end());
			m_settings->set_string_array(key::rom_folders, std::vector<Glib::ustring>(folders.begin(), folders.end()));
		});
		adw_action_row_add_suffix(ADW_ACTION_ROW(row), remove);
		adw_preferences_group_add(m_rom_group, row);
	}
	adw_preferences_page_add(m_general, m_rom_group);

	m_data_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(m_data_group, "Machine settings and snapshots");
	GtkWidget *row = adw_action_row_new();
	adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), "Folder");
	adw_action_row_set_subtitle(ADW_ACTION_ROW(row), shown_data.c_str());
	if (!data.empty())
	{
		GtkWidget *reset = gtk_button_new_with_mnemonic("Use _Default");
		gtk_widget_set_valign(reset, GTK_ALIGN_CENTER);
		connect(reset, "clicked", [this] { m_settings->set_string(key::data_folder, ""); });
		adw_action_row_add_suffix(ADW_ACTION_ROW(row), reset);
	}
	GtkWidget *choose = gtk_button_new_with_mnemonic("_Choose…");
	gtk_widget_set_valign(choose, GTK_ALIGN_CENTER);
	connect(choose, "clicked", [this] {
		choose_folder("Choose Data Folder", [this](const std::string &path) {
			m_settings->set_string(key::data_folder, path);
		});
	});
	adw_action_row_add_suffix(ADW_ACTION_ROW(row), choose);
	adw_preferences_group_add(m_data_group, row);
	adw_preferences_page_add(m_general, m_data_group);
}

void SettingsDialog::update_debugger()
{
	bool gdb_stub = m_settings->get_string(key::debugger) == "gdbStub";
	gtk_check_button_set_active(GTK_CHECK_BUTTON(gdb_stub ? m_gdb_stub : m_windows), TRUE);
	gtk_widget_set_sensitive(m_port, gdb_stub);
}

void SettingsDialog::choose_folder(const char *title, std::function<void(const std::string &)> chosen)
{
	auto dialog = Gtk::FileDialog::create();
	dialog->set_title(title);
	auto done = [dialog, chosen](const Glib::RefPtr<Gio::AsyncResult> &result) {
		try
		{
			if (auto folder = dialog->select_folder_finish(result); folder && !folder->get_path().empty())
				chosen(folder->get_path());
		}
		catch (const Glib::Error &)
		{
			// dismissed
		}
	};
	GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(m_dialog));
	if (root && GTK_IS_WINDOW(root))
		dialog->select_folder(*Glib::wrap(GTK_WINDOW(root)), done);
	else
		dialog->select_folder(done);
}

void SettingsDialog::reject_rom_folder(const std::string &path)
{
	AdwDialog *alert = adw_alert_dialog_new("This folder cannot be a ROM folder", nullptr);
	adw_alert_dialog_set_body(ADW_ALERT_DIALOG(alert),
			("The emulator cannot use a folder with “;” in its path: " + path).c_str());
	adw_alert_dialog_add_response(ADW_ALERT_DIALOG(alert), "close", "_OK");
	adw_dialog_present(alert, GTK_WIDGET(m_dialog));
}

} // namespace

void present_settings_dialog(GtkWidget *parent, const Glib::RefPtr<Gio::Settings> &settings)
{
	auto dialog = new SettingsDialog(settings);
	g_object_set_data_full(G_OBJECT(dialog->dialog()), "pinesy-settings", dialog,
			[](gpointer data) { delete static_cast<SettingsDialog *>(data); });
	adw_dialog_present(dialog->dialog(), parent);
}

} // namespace pinesy::gtk
