// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "deviceswindow.h"

#include "deviceinformationwindow.h"

#include "util/xmlfile.h"


namespace osd::debugger::gtk {

namespace {

char const *const DEVICE_KEY = "device";


// the list items are plain objects carrying a device pointer
GObject *device_item(device_t &device)
{
	GObject *const item = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
	g_object_set_data(item, DEVICE_KEY, &device);
	return item;
}


device_t *item_device(gpointer item)
{
	return static_cast<device_t *>(g_object_get_data(G_OBJECT(item), DEVICE_KEY));
}


device_t *row_device(GObject *list_item)
{
	GtkTreeListRow *const row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(GTK_LIST_ITEM(list_item)));
	gpointer const item = gtk_tree_list_row_get_item(row);
	device_t *const dev = item_device(item);
	g_object_unref(item);
	return dev;
}

} // anonymous namespace


devices_window::devices_window(debugger_gtk &debugger, window_gtk *parent) :
	window_gtk(debugger, parent)
{
	set_title("Debug: All Devices");

	if (parent)
		gtk_window_set_default_size(window(), 600, 400);

	//
	// The tree widget
	//
	GListStore *const root = g_list_store_new(G_TYPE_OBJECT);
	GObject *const root_item = device_item(m_machine.root_device());
	g_list_store_append(root, root_item);
	g_object_unref(root_item);

	GtkTreeListModel *const model = gtk_tree_list_model_new(
			G_LIST_MODEL(root),
			FALSE,
			TRUE,
			&devices_window::create_children,
			nullptr,
			nullptr);
	m_selection = gtk_single_selection_new(G_LIST_MODEL(model));
	g_object_ref(m_selection);
	m_devices_view = gtk_column_view_new(GTK_SELECTION_MODEL(m_selection));
	g_object_ref_sink(m_devices_view);

	GtkListItemFactory *const tag_factory = gtk_signal_list_item_factory_new();
	g_signal_connect(tag_factory, "setup", G_CALLBACK(on_setup_tag), nullptr);
	g_signal_connect(tag_factory, "bind", G_CALLBACK(on_bind_tag), nullptr);
	GtkColumnViewColumn *const tag_column = gtk_column_view_column_new("Tag", tag_factory);
	gtk_column_view_column_set_resizable(tag_column, TRUE);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(m_devices_view), tag_column);
	g_object_unref(tag_column);

	GtkListItemFactory *const name_factory = gtk_signal_list_item_factory_new();
	g_signal_connect(name_factory, "setup", G_CALLBACK(on_setup_name), nullptr);
	g_signal_connect(name_factory, "bind", G_CALLBACK(on_bind_name), nullptr);
	GtkColumnViewColumn *const name_column = gtk_column_view_column_new("Name", name_factory);
	gtk_column_view_column_set_resizable(name_column, TRUE);
	gtk_column_view_column_set_expand(name_column, TRUE);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(m_devices_view), name_column);
	g_object_unref(name_column);

	g_signal_connect(m_devices_view, "activate", G_CALLBACK(on_activate), this);

	GtkWidget *const scroller = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), m_devices_view);
	set_content(scroller);
}


devices_window::~devices_window()
{
	g_signal_handlers_disconnect_by_data(m_devices_view, this);
	g_object_unref(m_devices_view);
	g_object_unref(m_selection);
}


GListModel *devices_window::create_children(gpointer item, gpointer user_data)
{
	device_t *const dparent = item_device(item);
	device_t *target = dparent->subdevices().first();
	if (!target)
		return nullptr;

	GListStore *const children = g_list_store_new(G_TYPE_OBJECT);
	for ( ; target; target = target->next())
	{
		GObject *const child = device_item(*target);
		g_list_store_append(children, child);
		g_object_unref(child);
	}
	return G_LIST_MODEL(children);
}


void devices_window::on_setup_tag(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data)
{
	GtkWidget *const expander = gtk_tree_expander_new();
	GtkWidget *const label = gtk_label_new(nullptr);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
	gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), label);
	gtk_list_item_set_child(GTK_LIST_ITEM(item), expander);
}


void devices_window::on_bind_tag(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data)
{
	GtkTreeExpander *const expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(item)));
	GtkTreeListRow *const row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(GTK_LIST_ITEM(item)));
	gtk_tree_expander_set_list_row(expander, row);

	device_t *const dev = row_device(item);
	GtkWidget *const label = gtk_tree_expander_get_child(expander);
	// only the root device has no owner
	gtk_label_set_text(GTK_LABEL(label), !dev->owner() ? "<root>" : dev->basetag());
}


void devices_window::on_setup_name(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data)
{
	GtkWidget *const label = gtk_label_new(nullptr);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
	gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}


void devices_window::on_bind_name(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data)
{
	device_t *const dev = row_device(item);
	gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(item))), dev->name());
}


void devices_window::on_activate(GtkColumnView *view, guint position, gpointer user_data)
{
	devices_window &self = *static_cast<devices_window *>(user_data);
	GtkTreeListRow *const row = GTK_TREE_LIST_ROW(g_list_model_get_item(G_LIST_MODEL(self.m_selection), position));
	if (!row)
		return;
	gpointer const item = gtk_tree_list_row_get_item(row);
	device_t *const dev = item_device(item);
	g_object_unref(item);
	g_object_unref(row);
	self.activated(dev);
}


void devices_window::activated(device_t *dev)
{
	(new device_information_window(m_debugger, dev, this))->show();
}


void devices_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_DEVICES_VIEWER);
}

} // namespace osd::debugger::gtk
