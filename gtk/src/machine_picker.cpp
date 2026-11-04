#include "machine_picker.hpp"

#include "application.hpp"
#include "signals.hpp"

#include <adwaita.h>
#include <glibmm/main.h>
#include <sigc++/connection.h>

#include <string>
#include <vector>

namespace pinesy::gtk {

namespace {

constexpr const char *running_tooltip = "A machine is running. Choose when it stops.";

class MachinePicker
{
public:
	MachinePicker(Application &app, std::function<void(const core::Catalogue::Machine &)> on_choose);
	~MachinePicker() { m_runner_changed.disconnect(); }

	AdwDialog *dialog() const { return m_dialog; }

private:
	Application &m_app;
	std::function<void(const core::Catalogue::Machine &)> m_on_choose;
	AdwDialog *m_dialog = nullptr;
	GtkWidget *m_choose = nullptr;
	GtkWidget *m_entry = nullptr;
	GtkListBox *m_sections = nullptr;
	GtkListBox *m_machines = nullptr;
	GtkStack *m_stack = nullptr;
	AdwStatusPage *m_empty = nullptr;
	std::vector<core::Catalogue::Machine> m_shown;
	int m_section = 0;
	bool m_searching = false;
	sigc::connection m_runner_changed;
	Connections m_connections;

	const std::vector<core::Catalogue::Section> &sections() { return m_app.core().catalogue().sections(); }
	void show_machines();
	void update_choose();
	const core::Catalogue::Machine *selected();
	void choose(const core::Catalogue::Machine &machine);

	static void on_section_selected(GtkListBox *, GtkListBoxRow *row, gpointer data);
	static void on_machine_selected(GtkListBox *, GtkListBoxRow *, gpointer data);
	static void on_machine_activated(GtkListBox *, GtkListBoxRow *row, gpointer data);
};

GtkWidget *scrolled(GtkWidget *child)
{
	GtkWidget *window = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(window), child);
	return window;
}

MachinePicker::MachinePicker(Application &app, std::function<void(const core::Catalogue::Machine &)> on_choose)
	: m_app(app), m_on_choose(std::move(on_choose))
{
	m_dialog = adw_dialog_new();
	adw_dialog_set_title(m_dialog, "Choose a Machine");
	adw_dialog_set_content_width(m_dialog, 720);
	adw_dialog_set_content_height(m_dialog, 480);

	// The header bar: Cancel, the search button and Choose.
	GtkWidget *header = adw_header_bar_new();
	adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(header), FALSE);
	adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header), FALSE);
	GtkWidget *cancel = gtk_button_new_with_mnemonic("_Cancel");
	connect(cancel, "clicked", [this] { adw_dialog_close(m_dialog); });
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), cancel);
	m_choose = gtk_button_new_with_mnemonic("C_hoose");
	gtk_widget_add_css_class(m_choose, "suggested-action");
	connect(m_choose, "clicked", [this] {
		if (auto machine = selected())
			choose(*machine);
	});
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), m_choose);
	GtkWidget *search_button = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(search_button), "system-search-symbolic");
	gtk_widget_set_tooltip_text(search_button, "Search Machines");
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), search_button);

	// The search bar. Typing anywhere in the picker starts a search.
	GtkWidget *search_bar = gtk_search_bar_new();
	m_entry = gtk_search_entry_new();
	gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(m_entry), "Search machines");
	gtk_widget_set_hexpand(m_entry, TRUE);
	GtkWidget *clamp = adw_clamp_new();
	adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 400);
	adw_clamp_set_child(ADW_CLAMP(clamp), m_entry);
	gtk_search_bar_set_child(GTK_SEARCH_BAR(search_bar), clamp);
	gtk_search_bar_connect_entry(GTK_SEARCH_BAR(search_bar), GTK_EDITABLE(m_entry));
	gtk_search_bar_set_key_capture_widget(GTK_SEARCH_BAR(search_bar), GTK_WIDGET(m_dialog));
	g_object_bind_property(search_button, "active", search_bar, "search-mode-enabled",
			GBindingFlags(G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE));
	m_connections.add(m_entry, connect(m_entry, "search-changed", [this] {
		std::string text = gtk_editable_get_text(GTK_EDITABLE(m_entry));
		bool searching = text.find_first_not_of(" \t") != std::string::npos;
		m_searching = searching;
		gtk_widget_set_sensitive(GTK_WIDGET(m_sections), !searching);
		// while searching, the results come from every maker
		if (searching)
			gtk_list_box_unselect_all(m_sections);
		else if (m_section < int(sections().size()))
			gtk_list_box_select_row(m_sections, gtk_list_box_get_row_at_index(m_sections, m_section));
		show_machines();
	}));

	// The makers.
	m_sections = GTK_LIST_BOX(gtk_list_box_new());
	gtk_widget_add_css_class(GTK_WIDGET(m_sections), "navigation-sidebar");
	for (const auto &section : sections())
	{
		GtkWidget *label = gtk_label_new(section.name.c_str());
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
		gtk_list_box_append(m_sections, label);
	}
	GtkWidget *sidebar = scrolled(GTK_WIDGET(m_sections));
	gtk_widget_set_size_request(sidebar, 210, -1);

	// Their machines, or a status page when there are none.
	m_machines = GTK_LIST_BOX(gtk_list_box_new());
	gtk_list_box_set_selection_mode(m_machines, GTK_SELECTION_SINGLE);
	// a click selects a machine; a double click or Enter chooses it
	gtk_list_box_set_activate_on_single_click(m_machines, FALSE);
	m_empty = ADW_STATUS_PAGE(adw_status_page_new());
	m_stack = GTK_STACK(gtk_stack_new());
	GtkWidget *list = scrolled(GTK_WIDGET(m_machines));
	gtk_widget_set_hexpand(list, TRUE);
	gtk_stack_add_named(m_stack, list, "list");
	gtk_stack_add_named(m_stack, GTK_WIDGET(m_empty), "empty");

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_append(GTK_BOX(content), sidebar);
	gtk_box_append(GTK_BOX(content), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
	gtk_box_append(GTK_BOX(content), GTK_WIDGET(m_stack));

	GtkWidget *toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), search_bar);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), content);
	adw_dialog_set_child(m_dialog, toolbar);

	m_connections.add(m_sections, g_signal_connect(m_sections, "row-selected", G_CALLBACK(on_section_selected), this));
	m_connections.add(m_machines, g_signal_connect(m_machines, "row-selected", G_CALLBACK(on_machine_selected), this));
	m_connections.add(m_machines, g_signal_connect(m_machines, "row-activated", G_CALLBACK(on_machine_activated), this));
	m_runner_changed = m_app.runner().signal_changed().connect([this] { update_choose(); });

	// selecting the first maker shows its machines
	if (!sections().empty())
		gtk_list_box_select_row(m_sections, gtk_list_box_get_row_at_index(m_sections, 0));
	else
		show_machines();
}

void MachinePicker::show_machines()
{
	gtk_list_box_remove_all(m_machines);
	bool shows_maker = true;
	if (m_searching)
		m_shown = m_app.core().catalogue().search(gtk_editable_get_text(GTK_EDITABLE(m_entry)));
	else if (m_section < int(sections().size()))
	{
		m_shown = sections()[m_section].machines;
		shows_maker = sections()[m_section].shows_maker;
	}
	else
		m_shown.clear();

	for (const auto &machine : m_shown)
	{
		GtkWidget *row = adw_action_row_new();
		adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), machine.name.c_str());
		if (shows_maker && !machine.manufacturer.starts_with("<"))
			adw_action_row_set_subtitle(ADW_ACTION_ROW(row), machine.manufacturer.c_str());
		if (machine.preliminary)
		{
			GtkWidget *partly = gtk_label_new("Partly working");
			gtk_widget_add_css_class(partly, "warning");
			gtk_widget_add_css_class(partly, "caption");
			gtk_widget_set_tooltip_text(partly,
					"The emulation of this machine is not complete. Some software will not run correctly.");
			adw_action_row_add_suffix(ADW_ACTION_ROW(row), partly);
		}
		GtkWidget *year = gtk_label_new(machine.year.find('?') != std::string::npos ? "" : machine.year.c_str());
		gtk_widget_add_css_class(year, "dim-label");
		gtk_widget_add_css_class(year, "numeric");
		adw_action_row_add_suffix(ADW_ACTION_ROW(row), year);
		gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
		gtk_list_box_append(m_machines, row);
	}

	if (!m_shown.empty())
		gtk_stack_set_visible_child_name(m_stack, "list");
	else
	{
		if (m_searching)
		{
			adw_status_page_set_icon_name(m_empty, "edit-find-symbolic");
			adw_status_page_set_title(m_empty, "No Results Found");
			adw_status_page_set_description(m_empty, "Try a different search.");
		}
		else
		{
			adw_status_page_set_icon_name(m_empty, "dialog-warning-symbolic");
			adw_status_page_set_title(m_empty, "No Machines");
			adw_status_page_set_description(m_empty, "The emulator reported no machines.");
		}
		gtk_stack_set_visible_child_name(m_stack, "empty");
	}
	update_choose();
}

const core::Catalogue::Machine *MachinePicker::selected()
{
	GtkListBoxRow *row = gtk_list_box_get_selected_row(m_machines);
	if (!row)
		return nullptr;
	int index = gtk_list_box_row_get_index(row);
	return index >= 0 && index < int(m_shown.size()) ? &m_shown[index] : nullptr;
}

void MachinePicker::update_choose()
{
	bool running = m_app.runner().running();
	gtk_widget_set_sensitive(m_choose, selected() && !running);
	gtk_widget_set_tooltip_text(m_choose, running ? running_tooltip : nullptr);
}

void MachinePicker::choose(const core::Catalogue::Machine &machine)
{
	if (m_app.runner().running())
		return;
	auto chosen = machine;
	auto on_choose = m_on_choose;
	adw_dialog_close(m_dialog);
	Glib::signal_idle().connect_once([on_choose, chosen] { on_choose(chosen); });
}

void MachinePicker::on_section_selected(GtkListBox *, GtkListBoxRow *row, gpointer data)
{
	auto self = static_cast<MachinePicker *>(data);
	if (!row || self->m_searching)
		return;
	self->m_section = gtk_list_box_row_get_index(row);
	self->show_machines();
}

void MachinePicker::on_machine_selected(GtkListBox *, GtkListBoxRow *, gpointer data)
{
	static_cast<MachinePicker *>(data)->update_choose();
}

void MachinePicker::on_machine_activated(GtkListBox *, GtkListBoxRow *row, gpointer data)
{
	auto self = static_cast<MachinePicker *>(data);
	int index = gtk_list_box_row_get_index(row);
	if (index >= 0 && index < int(self->m_shown.size()))
		self->choose(self->m_shown[index]);
}

} // namespace

void present_machine_picker(GtkWidget *parent, Application &app,
		std::function<void(const core::Catalogue::Machine &)> on_choose)
{
	auto picker = new MachinePicker(app, std::move(on_choose));
	g_object_set_data_full(G_OBJECT(picker->dialog()), "pinesy-picker", picker,
			[](gpointer data) { delete static_cast<MachinePicker *>(data); });
	adw_dialog_present(picker->dialog(), parent);
}

} // namespace pinesy::gtk
