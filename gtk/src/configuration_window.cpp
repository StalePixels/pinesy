#include "configuration_window.hpp"

#include "application.hpp"
#include "machine_picker.hpp"
#include "settings.hpp"
#include "signals.hpp"

#include <gdkmm/clipboard.h>
#include <gdkmm/display.h>
#include <giomm/liststore.h>
#include <giomm/menu.h>
#include <glibmm/main.h>
#include <glibmm/markup.h>
#include <glibmm/miscutils.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>

#include <algorithm>

namespace pinesy::gtk {

namespace {

constexpr const char *data_key = "pinesy-configuration";
constexpr const char *extension = ".pinesy";

std::string escaped(const std::string &text)
{
	return Glib::Markup::escape_text(text).raw();
}

bool ends_with(const std::string &text, const std::string &end)
{
	return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

// A row title or subtitle is plain text, never markup.
void plain_row(GtkWidget *row)
{
	adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
}

// Indents a row by its depth in the hardware tree.
void indent_row(GtkWidget *row, int depth)
{
	if (depth <= 0)
		return;
	GtkWidget *space = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_size_request(space, 16 * depth, -1);
	adw_action_row_add_prefix(ADW_ACTION_ROW(row), space);
}

GtkWidget *combo_row(const std::string &title, const std::vector<std::string> &labels, guint selected,
		std::function<void(guint)> on_selected)
{
	GtkWidget *row = adw_combo_row_new();
	plain_row(row);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
	GtkStringList *model = gtk_string_list_new(nullptr);
	for (const auto &label : labels)
		gtk_string_list_append(model, label.c_str());
	adw_combo_row_set_model(ADW_COMBO_ROW(row), G_LIST_MODEL(model));
	g_object_unref(model);
	adw_combo_row_set_selected(ADW_COMBO_ROW(row), selected);
	connect_notify(row, "selected", [row, on_selected] { on_selected(adw_combo_row_get_selected(ADW_COMBO_ROW(row))); });
	return row;
}

GtkWidget *warning_icon(const std::string &tooltip)
{
	GtkWidget *icon = gtk_image_new_from_icon_name("dialog-warning-symbolic");
	gtk_widget_add_css_class(icon, "warning");
	gtk_widget_set_tooltip_text(icon, tooltip.c_str());
	return icon;
}

} // namespace

ConfigurationWindow *ConfigurationWindow::create(Application &app)
{
	auto self = new ConfigurationWindow(app);
	g_object_set_data_full(G_OBJECT(self->m_window), data_key, self,
			[](gpointer data) { delete static_cast<ConfigurationWindow *>(data); });
	return self;
}

ConfigurationWindow *ConfigurationWindow::from(Gtk::Window &window)
{
	return static_cast<ConfigurationWindow *>(g_object_get_data(G_OBJECT(window.gobj()), data_key));
}

ConfigurationWindow::ConfigurationWindow(Application &app) : m_app(app)
{
	m_window = ADW_APPLICATION_WINDOW(adw_application_window_new(app.gobj()));
	gtk_window_set_default_size(GTK_WINDOW(m_window), 620, 720);
	gtk_widget_set_size_request(GTK_WIDGET(m_window), 360, 300);

	// The header bar: Run (Run with Debugger in its menu), Stop while a
	// machine runs, and the main menu.
	GtkWidget *header = adw_header_bar_new();
	m_title = ADW_WINDOW_TITLE(adw_window_title_new("Untitled", ""));
	adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), GTK_WIDGET(m_title));

	auto run_menu = Gio::Menu::create();
	run_menu->append("Run with _Debugger", "win.run-debug");
	GtkWidget *run_button = adw_split_button_new();
	adw_split_button_set_label(ADW_SPLIT_BUTTON(run_button), "Run");
	adw_split_button_set_menu_model(ADW_SPLIT_BUTTON(run_button), G_MENU_MODEL(run_menu->gobj()));
	adw_split_button_set_dropdown_tooltip(ADW_SPLIT_BUTTON(run_button), "More Ways to Run");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(run_button), "win.run");
	gtk_widget_set_tooltip_text(run_button, "Run This Configuration");
	gtk_widget_add_css_class(run_button, "suggested-action");
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), run_button);

	m_stop = gtk_button_new_from_icon_name("media-playback-stop-symbolic");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(m_stop), "app.stop");
	gtk_widget_set_tooltip_text(m_stop, "Stop the Running Machine");
	gtk_widget_set_visible(m_stop, FALSE);
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), m_stop);

	auto menu = Gio::Menu::create();
	auto files = Gio::Menu::create();
	files->append("_New Configuration", "app.new");
	files->append("_Open…", "app.open");
	files->append_submenu("Open _Recent", app.recent_menu());
	menu->append_section(files);
	auto saving = Gio::Menu::create();
	saving->append("_Save", "win.save");
	saving->append("Save _As…", "win.save-as");
	menu->append_section(saving);
	auto running = Gio::Menu::create();
	running->append("_Reset", "app.reset");
	running->append("_Hard Reset", "app.hard-reset");
	running->append("_Grab Pointer", "app.toggle-pointer");
	running->append("Clear _NVRAM…", "win.clear-nvram");
	menu->append_section(running);
	m_running_menu = running;
	auto machine = Gio::Menu::create();
	machine->append("_Copy Command Line", "win.copy-command-line");
	menu->append_section(machine);
	auto app_items = Gio::Menu::create();
	app_items->append("_Preferences", "app.settings");
	menu->append_section(app_items);
	GtkWidget *menu_button = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu->gobj()));
	gtk_menu_button_set_primary(GTK_MENU_BUTTON(menu_button), TRUE);
	gtk_widget_set_tooltip_text(menu_button, "Main Menu");
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), menu_button);

	// The content: no machine yet, waiting for a run to end, or the form.
	m_stack = GTK_STACK(gtk_stack_new());

	GtkWidget *empty = adw_status_page_new();
	adw_status_page_set_icon_name(ADW_STATUS_PAGE(empty), "computer-symbolic");
	adw_status_page_set_title(ADW_STATUS_PAGE(empty), "No Machine");
	adw_status_page_set_description(ADW_STATUS_PAGE(empty), "Choose a machine for this configuration.");
	gtk_stack_add_named(m_stack, empty, "empty");

	GtkWidget *waiting = adw_status_page_new();
	adw_status_page_set_icon_name(ADW_STATUS_PAGE(waiting), "media-playback-start-symbolic");
	adw_status_page_set_title(ADW_STATUS_PAGE(waiting), "A Machine Is Running");
	adw_status_page_set_description(ADW_STATUS_PAGE(waiting),
			"This configuration opens when the running machine stops.");
	gtk_stack_add_named(m_stack, waiting, "waiting");

	m_page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
	gtk_stack_add_named(m_stack, GTK_WIDGET(m_page), "form");
	gtk_stack_set_visible_child_name(m_stack, "empty");

	m_toasts = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
	adw_toast_overlay_set_child(m_toasts, GTK_WIDGET(m_stack));

	GtkWidget *toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(m_toasts));
	adw_application_window_set_content(m_window, toolbar);

	// The window's actions.
	auto map = Glib::wrap(GTK_APPLICATION_WINDOW(m_window));
	m_run = map->add_action("run", [this] { run(false); });
	m_run_debug = map->add_action("run-debug", [this] { run(true); });
	m_copy = map->add_action("copy-command-line", [this] { copy_command_line(); });
	m_clear_nvram = map->add_action("clear-nvram", [this] { clear_nvram(); });
	map->add_action("save", [this] { save(); });
	map->add_action("save-as", [this] { save_as(); });

	g_signal_connect(m_window, "close-request", G_CALLBACK(+[](GtkWindow *, gpointer data) -> gboolean {
		return static_cast<ConfigurationWindow *>(data)->on_close_request();
	}), this);

	m_runner_changed = m_app.runner().signal_changed().connect([this] { on_runner_changed(); });

	update_title();
	update_actions();
}

ConfigurationWindow::~ConfigurationWindow()
{
	m_runner_changed.disconnect();
}

Gtk::Window &ConfigurationWindow::window()
{
	return *Glib::wrap(GTK_WINDOW(m_window));
}

void ConfigurationWindow::present()
{
	gtk_window_present(GTK_WINDOW(m_window));
}

void ConfigurationWindow::destroy()
{
	gtk_window_destroy(GTK_WINDOW(m_window));
}

//------------------------------------------------------------
//  The document
//------------------------------------------------------------

std::string ConfigurationWindow::title() const
{
	if (m_file)
	{
		std::string name = m_file->get_basename();
		if (ends_with(name, extension))
			name.resize(name.size() - std::string(extension).size());
		return name;
	}
	if (m_summary && m_summary->machine)
		return m_summary->machine->name;
	return "Untitled";
}

void ConfigurationWindow::update_title()
{
	std::string shown = (m_modified ? "• " : "") + title();
	adw_window_title_set_title(m_title, shown.c_str());
	std::string machine = m_summary && m_summary->machine ? m_summary->machine->name : std::string();
	adw_window_title_set_subtitle(m_title, machine == title() ? "" : machine.c_str());
	gtk_window_set_title(GTK_WINDOW(m_window), shown.c_str());
}

void ConfigurationWindow::update_actions()
{
	bool running = m_app.runner().running();
	bool can_run = !running && m_summary && m_summary->can_run() && !m_refresh_waiting;
	m_run->set_enabled(can_run);
	m_run_debug->set_enabled(can_run);
	m_copy->set_enabled(!m_document.arguments.empty());
	// MAME writes NVRAM when a machine exits, so it is not cleared while one runs
	m_clear_nvram->set_enabled(!running && !m_document.arguments.empty());
	gtk_widget_set_visible(m_stop, running);
	gtk_widget_set_sensitive(GTK_WIDGET(m_page), !running);

	// a menu model item's label is fixed, so the item is made again
	const char *const pointer_label = m_app.runner().pointer_held() ? "Release _Pointer" : "_Grab Pointer";
	m_running_menu->remove(pointer_item);
	m_running_menu->insert(pointer_item, pointer_label, "app.toggle-pointer");
}

void ConfigurationWindow::on_runner_changed()
{
	if (!m_app.runner().running() && m_refresh_waiting)
		refresh();
	update_actions();
}

void ConfigurationWindow::choose_machine()
{
	auto alive = std::weak_ptr<bool>(m_alive);
	present_machine_picker(GTK_WIDGET(m_window), m_app, [this, alive](const core::Catalogue::Machine &machine) {
		if (alive.expired())
			return;
		m_document.machine = machine.id;
		set_arguments(core::editor::new_configuration(machine, m_app.core().catalogue()));
	}, [this, alive] {
		// a new configuration with no machine closes with its picker
		if (!alive.expired() && is_empty())
			gtk_window_close(GTK_WINDOW(m_window));
	});
}

bool ConfigurationWindow::load(const Glib::RefPtr<Gio::File> &file, std::string &error)
{
	char *contents = nullptr;
	gsize length = 0;
	std::string etag;
	try
	{
		file->load_contents(contents, length, etag);
	}
	catch (const Glib::Error &failure)
	{
		error = failure.what();
		return false;
	}
	std::string text(contents, length);
	g_free(contents);

	auto document = core::Document::read(text);
	if (!document)
	{
		error = "The file is not a configuration this version of Pinesy can read.";
		return false;
	}
	m_document = *document;
	m_file = file;
	m_modified = false;
	m_summary.reset();
	refresh();
	m_app.note_recent(file);
	return true;
}

void ConfigurationWindow::set_arguments(std::vector<std::string> arguments)
{
	if (arguments != m_document.arguments)
	{
		m_document.arguments = std::move(arguments);
		m_modified = true;
	}
	// also when nothing changed: a choice the engine refused shows the
	// configuration's real value again
	refresh();
}

void ConfigurationWindow::edit(std::function<std::vector<std::string>()> change)
{
	// Rows are rebuilt after an edit, so the edit runs after the signal
	// of the row that asked for it has returned.
	auto alive = std::weak_ptr<bool>(m_alive);
	Glib::signal_idle().connect_once([this, alive, change] {
		if (alive.expired() || m_app.runner().running())
			return;
		set_arguments(change());
	});
}

void ConfigurationWindow::refresh()
{
	if (m_document.arguments.empty())
	{
		m_summary.reset();
		m_refresh_waiting = false;
		gtk_stack_set_visible_child_name(m_stack, "empty");
	}
	else if (m_app.runner().running())
	{
		// the engine must not be asked while a machine runs
		m_refresh_waiting = true;
		if (!m_summary)
			gtk_stack_set_visible_child_name(m_stack, "waiting");
	}
	else
	{
		m_refresh_waiting = false;
		auto &core = m_app.core();
		m_summary.emplace(m_document.arguments, core.describe(m_document.arguments), core.catalogue(),
				m_document.machine);
		if (m_summary->machine && m_document.machine != m_summary->machine->id)
		{
			m_document.machine = m_summary->machine->id;
			m_modified = true;
		}
		build_form(*m_summary);
		gtk_stack_set_visible_child_name(m_stack, "form");
	}
	update_title();
	update_actions();
}

//------------------------------------------------------------
//  The form
//------------------------------------------------------------

void ConfigurationWindow::add_group(AdwPreferencesGroup *group)
{
	adw_preferences_page_add(m_page, group);
	m_groups.push_back(group);
}

void ConfigurationWindow::build_form(const core::Summary &summary)
{
	for (auto group : m_groups)
		adw_preferences_page_remove(m_page, group);
	m_groups.clear();
	m_command_line = nullptr;

	if (!summary.errors.empty())
		add_group(message_group("This configuration cannot run", summary.errors, "dialog-warning-symbolic", "error"));
	if (!summary.warnings.empty())
		add_group(message_group("The emulator warns", summary.warnings, "dialog-information-symbolic", "dim-label"));
	add_group(machine_group(summary));
	if (!summary.hardware.empty())
		add_group(hardware_group(summary));
	if (!summary.media.empty())
		add_group(media_group(summary));
	add_group(command_line_group());
}

AdwPreferencesGroup *ConfigurationWindow::message_group(const char *title, const std::string &text, const char *icon,
		const char *style)
{
	auto group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, title);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_add_css_class(box, "card");
	GtkWidget *image = gtk_image_new_from_icon_name(icon);
	gtk_widget_add_css_class(image, style);
	gtk_widget_set_valign(image, GTK_ALIGN_START);
	gtk_widget_set_margin_start(image, 12);
	gtk_widget_set_margin_top(image, 12);
	gtk_box_append(GTK_BOX(box), image);

	std::string trimmed = text;
	while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == ' '))
		trimmed.pop_back();
	GtkWidget *label = gtk_label_new(trimmed.c_str());
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	// a focused selectable label selects all its text; it takes the
	// focus only when clicked
	gtk_widget_set_focusable(label, FALSE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_margin_top(label, 12);
	gtk_widget_set_margin_bottom(label, 12);
	gtk_widget_set_margin_end(label, 12);
	gtk_box_append(GTK_BOX(box), label);

	adw_preferences_group_add(group, box);
	return group;
}

AdwPreferencesGroup *ConfigurationWindow::machine_group(const core::Summary &summary)
{
	auto group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, escaped(summary.machine ? summary.machine->name : summary.driver).c_str());

	std::vector<std::string> lines;
	std::string detail;
	if (summary.machine && !summary.machine->manufacturer.starts_with("<"))
		detail = summary.machine->manufacturer;
	if (!summary.year.empty() && summary.year.find('?') == std::string::npos)
		detail += (detail.empty() ? "" : ", ") + summary.year;
	if (!detail.empty())
		lines.push_back(detail);
	if (summary.preliminary)
		lines.push_back("The emulation of this machine is not complete.");
	std::string description;
	for (const auto &line : lines)
		description += (description.empty() ? "" : "\n") + line;
	if (!description.empty())
		adw_preferences_group_set_description(group, escaped(description).c_str());

	if (summary.models.size() > 1)
	{
		std::vector<std::string> labels;
		guint selected = GTK_INVALID_LIST_POSITION;
		for (const auto &model : summary.models)
		{
			if (model.driver == summary.driver)
				selected = guint(labels.size());
			labels.push_back(model.year.find('?') != std::string::npos ? model.label
					: model.label + " (" + model.year + ")");
		}
		auto models = summary.models;
		std::string current = summary.driver;
		adw_preferences_group_add(group, combo_row("Model", labels, selected, [this, models, current](guint index) {
			if (index >= models.size() || models[index].driver == current)
				return;
			std::string driver = models[index].driver;
			edit([this, driver] {
				return core::editor::set_driver(m_document.arguments, driver, m_app.core().catalogue(),
						m_app.core().describer());
			});
		}));
	}

	std::vector<core::Summary::ROMChoice> roms = summary.roms;
	roms.insert(roms.end(), summary.variant_roms.begin(), summary.variant_roms.end());
	if (roms.size() > 1)
	{
		std::vector<std::string> labels;
		guint selected = GTK_INVALID_LIST_POSITION;
		for (const auto &rom : roms)
		{
			if (summary.current_rom && summary.current_rom->driver == rom.driver && summary.current_rom->bios == rom.bios)
				selected = guint(labels.size());
			labels.push_back(rom.label);
		}
		adw_preferences_group_add(group, combo_row("ROM", labels, selected, [this, roms, selected](guint index) {
			if (index >= roms.size() || index == selected)
				return;
			auto choice = roms[index];
			edit([this, choice] {
				return core::editor::set_rom(m_document.arguments, choice.driver, choice.bios, m_app.core().catalogue(),
						m_app.core().describer());
			});
		}));
	}

	// the window size: the core's choices, and a magnification row for Custom
	std::vector<int> choices{ core::window::fit, core::window::full_screen };
	std::vector<std::string> sizes{ "Fit to screen", "Full screen" };
	for (int magnification = 1; magnification <= core::window::listed_magnifications; magnification++)
	{
		choices.push_back(magnification);
		sizes.push_back(std::to_string(magnification) + "×");
	}
	choices.push_back(core::window::custom);
	sizes.push_back("Custom");
	const int chosen = core::window::choice(m_document.window_size, m_custom_window_size);
	const auto found = std::find(choices.begin(), choices.end(), chosen);

	GtkWidget *magnification = adw_spin_row_new_with_range(1, G_MAXINT, 1);
	plain_row(magnification);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(magnification), "Magnification");
	adw_spin_row_set_digits(ADW_SPIN_ROW(magnification), 0);
	adw_spin_row_set_numeric(ADW_SPIN_ROW(magnification), TRUE);
	adw_spin_row_set_value(ADW_SPIN_ROW(magnification),
			core::window::size_for_choice(core::window::custom, m_document.window_size));
	gtk_widget_set_visible(magnification, chosen == core::window::custom);

	auto set_window_size = [this](int window_size) {
		if (window_size == m_document.window_size)
			return;
		m_document.window_size = window_size;
		m_modified = true;
		update_title();
	};
	adw_preferences_group_add(group, combo_row("Window size", sizes, guint(found - choices.begin()),
			[this, choices, magnification, set_window_size](guint index) {
		if (index >= choices.size())
			return;
		const int choice = choices[index];
		m_custom_window_size = choice == core::window::custom;
		set_window_size(core::window::size_for_choice(choice, m_document.window_size));
		if (m_custom_window_size)
			adw_spin_row_set_value(ADW_SPIN_ROW(magnification), m_document.window_size);
		gtk_widget_set_visible(magnification, m_custom_window_size);
	}));
	connect_notify(magnification, "value", [this, magnification, set_window_size] {
		if (core::window::choice(m_document.window_size, m_custom_window_size) != core::window::custom)
			return;
		set_window_size(core::window::size_for_choice(core::window::custom,
				int(adw_spin_row_get_value(ADW_SPIN_ROW(magnification)))));
	});
	adw_preferences_group_add(group, magnification);

	// the mouse setting, for a machine that takes a mouse
	if (summary.mouse)
	{
		GtkWidget *mouse = adw_switch_row_new();
		plain_row(mouse);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(mouse), "Mouse");
		adw_switch_row_set_active(ADW_SWITCH_ROW(mouse), m_document.mouse);
		connect_notify(mouse, "active", [this, mouse] {
			const bool on = adw_switch_row_get_active(ADW_SWITCH_ROW(mouse));
			if (on == m_document.mouse)
				return;
			m_document.mouse = on;
			m_modified = true;
			update_title();
			if (m_command_line)
				gtk_label_set_text(GTK_LABEL(m_command_line), command_line().c_str());
		});
		adw_preferences_group_add(group, mouse);
	}
	return group;
}

AdwPreferencesGroup *ConfigurationWindow::hardware_group(const core::Summary &summary)
{
	auto group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, "Hardware");

	for (const auto &row : summary.hardware)
	{
		if (row.fixed_text)
		{
			GtkWidget *fixed = adw_action_row_new();
			plain_row(fixed);
			adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fixed), row.label.c_str());
			GtkWidget *value = gtk_label_new(row.fixed_text->c_str());
			gtk_widget_add_css_class(value, "dim-label");
			adw_action_row_add_suffix(ADW_ACTION_ROW(fixed), value);
			indent_row(fixed, row.depth);
			adw_preferences_group_add(group, fixed);
		}
		else
		{
			std::vector<std::string> values;
			std::vector<std::string> labels;
			for (const auto &choice : row.choices)
			{
				values.push_back(choice.value);
				labels.push_back(choice.label);
			}
			auto found = std::find(values.begin(), values.end(), row.slot.current);
			if (found == values.end())
			{
				values.push_back(row.slot.current);
				labels.push_back(row.slot.currentdescription.value_or(row.slot.current));
				found = values.end() - 1;
			}
			auto slot = row.slot;
			GtkWidget *combo = combo_row(row.label, labels, guint(found - values.begin()),
					[this, values, slot](guint index) {
				if (index >= values.size() || values[index] == slot.current)
					return;
				std::string card = values[index];
				edit([this, slot, card] {
					return core::editor::set_slot(m_document.arguments, slot, card, m_app.core().catalogue(),
							m_app.core().describer());
				});
			});
			if (row.meant_for)
				adw_action_row_set_subtitle(ADW_ACTION_ROW(combo), ("Made for: " + *row.meant_for).c_str());
			indent_row(combo, row.depth);
			adw_preferences_group_add(group, combo);
		}

		if (!row.roms.empty())
		{
			std::vector<std::string> values;
			std::vector<std::string> labels;
			guint selected = GTK_INVALID_LIST_POSITION;
			for (const auto &rom : row.roms)
			{
				if (rom.value == row.current_rom)
					selected = guint(values.size());
				values.push_back(rom.value);
				labels.push_back(rom.label);
			}
			auto slot = row.slot;
			std::string current = row.current_rom;
			GtkWidget *combo = combo_row("ROM", labels, selected, [this, values, slot, current](guint index) {
				if (index >= values.size() || values[index] == current)
					return;
				std::string bios = values[index];
				edit([this, slot, bios] {
					return core::editor::set_card_rom(m_document.arguments, slot, bios, m_app.core().catalogue(),
							m_app.core().describer());
				});
			});
			indent_row(combo, row.depth + 1);
			adw_preferences_group_add(group, combo);
		}
	}
	return group;
}

AdwPreferencesGroup *ConfigurationWindow::media_group(const core::Summary &summary)
{
	auto group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, "Media");

	for (const auto &row : summary.media)
	{
		const core::Media &media = row.media;
		GtkWidget *item = adw_action_row_new();
		plain_row(item);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(item), row.label.c_str());
		std::string extensions;
		for (const auto &ext : media.extension_list())
			extensions += (extensions.empty() ? "." : " .") + ext;
		std::string problem;
		if (media.mandatory && media.current.empty())
			problem = "This machine needs a file here before it can run.";
		else if (summary.missing_files.count(media.name))
			problem = "File not found: " + media.current;
		else if (auto found = summary.sd_card_problems.find(media.name); found != summary.sd_card_problems.end())
			problem = found->second;
		// a problem is the subtitle, in place of the extensions
		adw_action_row_set_subtitle(ADW_ACTION_ROW(item), (problem.empty() ? extensions : problem).c_str());
		adw_action_row_set_subtitle_lines(ADW_ACTION_ROW(item), problem.empty() ? 1 : 0);
		if (!problem.empty())
		{
			gtk_widget_add_css_class(item, "error");
			adw_action_row_add_suffix(ADW_ACTION_ROW(item), warning_icon(problem));
		}

		const std::string shown = media.current.empty() ? "Empty" : Glib::path_get_basename(media.current);
		GtkWidget *file = gtk_label_new(shown.c_str());
		if (media.current.empty())
			gtk_widget_add_css_class(file, "dim-label");
		else
			gtk_widget_set_tooltip_text(file, media.current.c_str());
		// An ellipsized label's minimum width is only the ellipsis, and the
		// row shares spare width evenly between the title and the suffixes,
		// so the name would shrink while the title still had room to wrap.
		// The minimum width is the whole name, up to the cap.
		constexpr int file_chars = 24;
		gtk_label_set_ellipsize(GTK_LABEL(file), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_width_chars(GTK_LABEL(file), std::min<int>(g_utf8_strlen(shown.c_str(), -1), file_chars));
		gtk_label_set_max_width_chars(GTK_LABEL(file), file_chars);
		adw_action_row_add_suffix(ADW_ACTION_ROW(item), file);

		GtkWidget *choose = gtk_button_new_with_label("Choose…");
		gtk_widget_set_valign(choose, GTK_ALIGN_CENTER);
		connect(choose, "clicked", [this, media] { choose_media(media); });
		adw_action_row_add_suffix(ADW_ACTION_ROW(item), choose);

		if (!media.current.empty())
		{
			GtkWidget *eject = gtk_button_new_from_icon_name("media-eject-symbolic");
			gtk_widget_set_valign(eject, GTK_ALIGN_CENTER);
			gtk_widget_add_css_class(eject, "flat");
			gtk_widget_set_tooltip_text(eject, "Remove the File");
			std::string name = media.name;
			connect(eject, "clicked", [this, name] {
				edit([this, name] { return core::editor::set_media(m_document.arguments, name, std::nullopt); });
			});
			adw_action_row_add_suffix(ADW_ACTION_ROW(item), eject);
		}
		adw_preferences_group_add(group, item);

		// the Pi pause setting, next to the Pi's SD card
		if (media.name == summary.pi_sd_card)
		{
			GtkWidget *paused = adw_switch_row_new();
			plain_row(paused);
			adw_preferences_row_set_title(ADW_PREFERENCES_ROW(paused), "Pi starts paused");
			adw_action_row_set_subtitle(ADW_ACTION_ROW(paused),
					"With Run with Debugger, the Pi waits at its first instruction until you continue it.");
			adw_switch_row_set_active(ADW_SWITCH_ROW(paused), m_document.pi_paused);
			connect_notify(paused, "active", [this, paused] {
				const bool on = adw_switch_row_get_active(ADW_SWITCH_ROW(paused));
				if (on == m_document.pi_paused)
					return;
				m_document.pi_paused = on;
				m_modified = true;
				update_title();
			});
			adw_preferences_group_add(group, paused);
		}
	}
	return group;
}

AdwPreferencesGroup *ConfigurationWindow::command_line_group()
{
	auto group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, "Command line");

	GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
	gtk_widget_add_css_class(copy, "flat");
	gtk_widget_set_valign(copy, GTK_ALIGN_CENTER);
	gtk_widget_set_tooltip_text(copy, "Copy Command Line");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(copy), "win.copy-command-line");
	adw_preferences_group_set_header_suffix(group, copy);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(box, "card");
	GtkWidget *label = gtk_label_new(command_line().c_str());
	m_command_line = label;
	gtk_widget_add_css_class(label, "monospace");
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_widget_set_margin_top(label, 12);
	gtk_widget_set_margin_bottom(label, 12);
	gtk_widget_set_margin_start(label, 12);
	gtk_widget_set_margin_end(label, 12);
	gtk_box_append(GTK_BOX(box), label);
	adw_preferences_group_add(group, box);
	return group;
}

bool ConfigurationWindow::takes_mouse() const
{
	return m_summary && m_summary->mouse;
}

std::string ConfigurationWindow::command_line()
{
	return m_app.core().copy_command_line(executable_path(), m_document.arguments, false, takes_mouse(),
			m_document.mouse);
}

void ConfigurationWindow::choose_media(const core::Media &media)
{
	auto dialog = Gtk::FileDialog::create();
	dialog->set_title("Choose a File");
	// "All files" is there because MAME opens files with extensions that
	// its list for the device leaves out, such as .img SD card images.
	auto extensions = media.extension_list();
	if (!extensions.empty())
	{
		auto filter = Gtk::FileFilter::create();
		filter->set_name("Files for this device");
		for (const auto &ext : extensions)
			filter->add_suffix(ext);
		auto all = Gtk::FileFilter::create();
		all->set_name("All files");
		all->add_pattern("*");
		auto filters = Gio::ListStore<Gtk::FileFilter>::create();
		filters->append(filter);
		filters->append(all);
		dialog->set_filters(filters);
		dialog->set_default_filter(filter);
	}
	std::string name = media.name;
	auto alive = std::weak_ptr<bool>(m_alive);
	dialog->open(window(), [this, alive, dialog, name](const Glib::RefPtr<Gio::AsyncResult> &result) {
		Glib::RefPtr<Gio::File> file;
		try
		{
			file = dialog->open_finish(result);
		}
		catch (const Glib::Error &)
		{
			return;   // dismissed
		}
		if (alive.expired() || !file || file->get_path().empty())
			return;
		std::string path = file->get_path();
		if (m_summary)
			for (const auto &row : m_summary->media)
				if (row.media.name == name && row.media.interface == "sdcard")
					if (std::string problem = core::sd_card::problem(path); !problem.empty())
					{
						m_app.alert("This SD card image cannot be used", problem, &window());
						return;
					}
		edit([this, name, path] { return core::editor::set_media(m_document.arguments, name, path); });
	});
}

//------------------------------------------------------------
//  Actions
//------------------------------------------------------------

void ConfigurationWindow::run(bool debug)
{
	if (m_app.runner().running() || !m_summary || !m_summary->can_run() || m_refresh_waiting)
		return;
	m_app.runner().start(m_app.core(), title(), m_document.arguments, debug, m_document.window_size,
			takes_mouse(), m_document.mouse, m_document.pi_paused);
}

void ConfigurationWindow::copy_command_line()
{
	if (m_document.arguments.empty())
		return;
	window().get_clipboard()->set_text(command_line());
	toast("Command line copied");
}

void ConfigurationWindow::toast(const std::string &text)
{
	adw_toast_overlay_add_toast(m_toasts, adw_toast_new(text.c_str()));
}

bool ConfigurationWindow::write(const Glib::RefPtr<Gio::File> &file)
{
	try
	{
		std::string new_etag;
		file->replace_contents(m_document.write(), "", new_etag);
	}
	catch (const Glib::Error &error)
	{
		m_app.alert("Could not save “" + file->get_basename() + "”", error.what());
		return false;
	}
	m_file = file;
	m_modified = false;
	update_title();
	m_app.note_recent(file);
	return true;
}

void ConfigurationWindow::save(std::function<void()> then)
{
	if (m_document.arguments.empty())
		return;
	if (!m_file)
	{
		save_as(std::move(then));
		return;
	}
	if (write(m_file) && then)
		then();
}

void ConfigurationWindow::save_as(std::function<void()> then)
{
	if (m_document.arguments.empty())
		return;
	auto dialog = Gtk::FileDialog::create();
	dialog->set_title("Save Configuration");
	dialog->set_initial_name(title() + extension);
	auto filter = Gtk::FileFilter::create();
	filter->set_name("Pinesy configurations");
	filter->add_suffix("pinesy");
	auto filters = Gio::ListStore<Gtk::FileFilter>::create();
	filters->append(filter);
	dialog->set_filters(filters);
	dialog->set_default_filter(filter);

	auto alive = std::weak_ptr<bool>(m_alive);
	dialog->save(window(), [this, alive, dialog, then](const Glib::RefPtr<Gio::AsyncResult> &result) {
		Glib::RefPtr<Gio::File> file;
		try
		{
			file = dialog->save_finish(result);
		}
		catch (const Glib::Error &)
		{
			return;   // dismissed
		}
		if (alive.expired() || !file)
			return;
		if (!ends_with(file->get_basename(), extension))
			file = file->get_parent()->get_child(file->get_basename() + extension);
		if (write(file) && then)
			then();
	});
}

void ConfigurationWindow::clear_nvram()
{
	if (m_app.runner().running() || m_document.arguments.empty())
		return;

	AdwDialog *dialog = adw_alert_dialog_new(("Clear the NVRAM of “" + title() + "”?").c_str(), nullptr);
	adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
			"The NVRAM that MAME saved for this machine is deleted. You cannot undo this.");
	adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", "_Cancel", "clear", "_Clear NVRAM", nullptr);
	adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "clear", ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
	auto alive = std::weak_ptr<bool>(m_alive);
	connect_string(dialog, "response", [this, alive](const std::string &response) {
		if (alive.expired() || response != "clear" || m_app.runner().running())
			return;
		if (!m_app.core().clear_nvram(m_document.arguments))
			m_app.alert("The NVRAM could not be cleared", "A file in the NVRAM folder could not be deleted.", &window());
	});
	adw_dialog_present(dialog, GTK_WIDGET(m_window));
}

bool ConfigurationWindow::on_close_request()
{
	if (m_close_confirmed || !m_modified || m_document.arguments.empty())
		return false;

	AdwDialog *dialog = adw_alert_dialog_new("Save Changes?", nullptr);
	adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
			("Changes to “" + title() + "” are lost if you close the window without saving them.").c_str());
	adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", "_Cancel", "discard", "_Discard",
			"save", "_Save", nullptr);
	adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "discard", ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "save", ADW_RESPONSE_SUGGESTED);
	adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "save");
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
	auto alive = std::weak_ptr<bool>(m_alive);
	connect_string(dialog, "response", [this, alive](const std::string &response) {
		if (alive.expired())
			return;
		auto close = [this] {
			m_close_confirmed = true;
			gtk_window_close(GTK_WINDOW(m_window));
		};
		if (response == "discard")
			close();
		else if (response == "save")
			save(close);
	});
	adw_dialog_present(dialog, GTK_WIDGET(m_window));
	return true;
}

} // namespace pinesy::gtk
