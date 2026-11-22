#include "recent_window.hpp"

#include "application.hpp"
#include "signals.hpp"

#include <giomm/file.h>

namespace pinesy::gtk {

namespace {

constexpr const char *data_key = "pinesy-recent";
constexpr const char *mime_type = "application/x-pinesy";

} // namespace

RecentWindow *RecentWindow::create(Application &app, std::vector<core::recent::Entry> entries,
		std::function<void()> closed)
{
	auto self = new RecentWindow(app, std::move(entries), std::move(closed));
	g_object_set_data_full(G_OBJECT(self->m_window), data_key, self,
			[](gpointer data) { delete static_cast<RecentWindow *>(data); });
	return self;
}

RecentWindow::RecentWindow(Application &app, std::vector<core::recent::Entry> entries, std::function<void()> closed)
	: m_app(app)
	, m_entries(std::move(entries))
	, m_closed(std::move(closed))
{
	m_window = ADW_APPLICATION_WINDOW(adw_application_window_new(app.gobj()));
	gtk_window_set_title(GTK_WINDOW(m_window), "Recent Configurations");
	gtk_window_set_default_size(GTK_WINDOW(m_window), 560, 400);
	gtk_widget_set_size_request(GTK_WIDGET(m_window), 360, 300);

	GtkWidget *header = adw_header_bar_new();

	// the configurations, newest first; activating one opens it
	m_list = GTK_LIST_BOX(gtk_list_box_new());
	gtk_list_box_set_selection_mode(m_list, GTK_SELECTION_SINGLE);
	gtk_list_box_set_activate_on_single_click(m_list, FALSE);
	gtk_widget_add_css_class(GTK_WIDGET(m_list), "boxed-list");
	gtk_widget_set_valign(GTK_WIDGET(m_list), GTK_ALIGN_START);
	GIcon *icon = g_content_type_get_icon(mime_type);
	for (const auto &entry : m_entries)
	{
		GtkWidget *row = adw_action_row_new();
		adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), entry.name.c_str());
		adw_action_row_set_subtitle(ADW_ACTION_ROW(row), entry.folder.c_str());
		adw_action_row_set_title_lines(ADW_ACTION_ROW(row), 1);
		adw_action_row_set_subtitle_lines(ADW_ACTION_ROW(row), 1);
		GtkWidget *image = gtk_image_new_from_gicon(icon);
		gtk_image_set_icon_size(GTK_IMAGE(image), GTK_ICON_SIZE_LARGE);
		adw_action_row_add_prefix(ADW_ACTION_ROW(row), image);
		gtk_widget_set_tooltip_text(row, entry.path.c_str());
		gtk_list_box_append(m_list, row);
	}
	g_object_unref(icon);

	GtkWidget *clamp = adw_clamp_new();
	adw_clamp_set_child(ADW_CLAMP(clamp), GTK_WIDGET(m_list));
	gtk_widget_set_margin_top(clamp, 12);
	gtk_widget_set_margin_bottom(clamp, 12);
	gtk_widget_set_margin_start(clamp, 12);
	gtk_widget_set_margin_end(clamp, 12);
	GtkWidget *scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), clamp);
	gtk_widget_set_vexpand(scrolled, TRUE);

	// Create a New Machine… and Open
	GtkWidget *bar = gtk_action_bar_new();
	GtkWidget *create_machine = gtk_button_new_with_mnemonic("Create a _New Machine…");
	connect(create_machine, "clicked", [this] { m_app.new_configuration(); });
	gtk_action_bar_pack_start(GTK_ACTION_BAR(bar), create_machine);
	m_open = gtk_button_new_with_mnemonic("_Open");
	gtk_widget_add_css_class(m_open, "suggested-action");
	connect(m_open, "clicked", [this] { open_selected(); });
	gtk_action_bar_pack_end(GTK_ACTION_BAR(bar), m_open);

	GtkWidget *toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), scrolled);
	adw_toolbar_view_add_bottom_bar(ADW_TOOLBAR_VIEW(toolbar), bar);
	adw_application_window_set_content(m_window, toolbar);
	gtk_window_set_default_widget(GTK_WINDOW(m_window), m_open);

	connect(m_list, "selected-rows-changed", [this] {
		gtk_widget_set_sensitive(m_open, gtk_list_box_get_selected_row(m_list) != nullptr);
	});
	g_signal_connect(m_list, "row-activated", G_CALLBACK(+[](GtkListBox *, GtkListBoxRow *row, gpointer data) {
		auto self = static_cast<RecentWindow *>(data);
		gtk_list_box_select_row(self->m_list, row);
		self->open_selected();
	}), this);

	gtk_list_box_select_row(m_list, gtk_list_box_get_row_at_index(m_list, 0));
	gtk_widget_set_sensitive(m_open, gtk_list_box_get_selected_row(m_list) != nullptr);
}

RecentWindow::~RecentWindow()
{
	if (m_closed)
		m_closed();
}

void RecentWindow::present()
{
	gtk_window_present(GTK_WINDOW(m_window));
	if (GtkListBoxRow *row = gtk_list_box_get_selected_row(m_list))
		gtk_widget_grab_focus(GTK_WIDGET(row));
}

void RecentWindow::destroy()
{
	gtk_window_destroy(GTK_WINDOW(m_window));
}

void RecentWindow::open_selected()
{
	GtkListBoxRow *row = gtk_list_box_get_selected_row(m_list);
	if (!row)
		return;
	int index = gtk_list_box_row_get_index(row);
	if (index < 0 || std::size_t(index) >= m_entries.size())
		return;
	// opening the file closes this window, and with it this object
	m_app.open_file(Gio::File::create_for_path(m_entries[std::size_t(index)].path));
}

} // namespace pinesy::gtk
