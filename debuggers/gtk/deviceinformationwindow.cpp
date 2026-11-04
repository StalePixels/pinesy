// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "deviceinformationwindow.h"

#include "memorywindow.h"

#include "util/xmlfile.h"

#include <string>


namespace osd::debugger::gtk {

namespace {

char const *const SPACE_KEY = "space";
char const *const COLUMN_KEY = "column";
char const *const MEMORY_KEY = "memory";


GtkWidget *grid_label(char const *text)
{
	GtkWidget *const label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
	return label;
}


int item_space(gpointer item)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), SPACE_KEY));
}


void on_setup_cell(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data)
{
	gtk_list_item_set_child(GTK_LIST_ITEM(item), grid_label(nullptr));
}


// The address spaces table: the first column is the space number, which the
// Qt debugger shows as the vertical header
void on_bind_cell(GtkSignalListItemFactory *factory, GObject *item, gpointer user_data)
{
	int const column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(factory), COLUMN_KEY));
	device_memory_interface &memory = *static_cast<device_memory_interface *>(g_object_get_data(G_OBJECT(factory), MEMORY_KEY));
	address_space &s = memory.space(item_space(gtk_list_item_get_item(GTK_LIST_ITEM(item))));

	std::string text;
	switch (column)
	{
	case 0: text = std::to_string(s.spacenum()); break;
	case 1: text = s.name(); break;
	case 2: text = std::to_string(s.data_width()); break;
	case 3: text = std::to_string(s.addr_width()); break;
	case 4: text = std::to_string(s.addr_shift()); break;
	case 5: text = std::to_string(s.alignment()); break;
	case 6: text = (s.endianness() == std::endian::big) ? "big" : "little"; break;
	}
	gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(item))), text.c_str());
}

} // anonymous namespace


device_information_window::device_information_window(debugger_gtk &debugger, device_t *device, window_gtk *parent) :
	window_gtk(debugger, parent),
	m_device(device),
	m_memory(nullptr),
	m_addrspaces_view(nullptr),
	m_addrspaces_model(nullptr)
{
	if (parent)
		gtk_window_set_default_size(window(), 600, 400);

	if (m_device)
		fill_device_information();
}


device_information_window::~device_information_window()
{
	if (m_addrspaces_view)
	{
		g_signal_handlers_disconnect_by_data(m_addrspaces_view, this);
		g_object_unref(m_addrspaces_view);
	}
	if (m_addrspaces_model)
		g_object_unref(m_addrspaces_model);
}


void device_information_window::set_device(const char *tag)
{
	m_device = m_machine.root_device().subdevice(tag);
	if (!m_device)
		m_device = &m_machine.root_device();
	fill_device_information();
}


void device_information_window::on_activate(GtkColumnView *view, guint position, gpointer user_data)
{
	device_information_window &self = *static_cast<device_information_window *>(user_data);
	gpointer const item = g_list_model_get_item(G_LIST_MODEL(self.m_addrspaces_model), position);
	if (!item)
		return;
	int const index = item_space(item);
	g_object_unref(item);
	self.open_address_space(index);
}


void device_information_window::open_address_space(int index)
{
	memory_window *const foo = new memory_window(m_debugger, this);
	foo->select_space(m_memory->space(index));
}


void device_information_window::restore_configuration(util::xml::data_node const &node)
{
	window_gtk::restore_configuration(node);

	auto const tag = node.get_attribute_string(ATTR_WINDOW_DEVICE_TAG, ":");
	set_device(tag);
}


void device_information_window::save_configuration_to_node(util::xml::data_node &node)
{
	window_gtk::save_configuration_to_node(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_DEVICE_INFO_VIEWER);

	node.set_attribute(ATTR_WINDOW_DEVICE_TAG, m_device->tag());
}


void device_information_window::add_space_column(char const *title, int column)
{
	GtkListItemFactory *const factory = gtk_signal_list_item_factory_new();
	g_object_set_data(G_OBJECT(factory), COLUMN_KEY, GINT_TO_POINTER(column));
	g_object_set_data(G_OBJECT(factory), MEMORY_KEY, m_memory);
	g_signal_connect(factory, "setup", G_CALLBACK(on_setup_cell), nullptr);
	g_signal_connect(factory, "bind", G_CALLBACK(on_bind_cell), nullptr);
	GtkColumnViewColumn *const view_column = gtk_column_view_column_new(title, factory);
	gtk_column_view_column_set_resizable(view_column, TRUE);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(m_addrspaces_view), view_column);
	g_object_unref(view_column);
}


void device_information_window::fill_device_information()
{
	set_title(util::string_format("Debug: Device %s", m_device->tag()).c_str());

	if (m_addrspaces_view)
	{
		g_signal_handlers_disconnect_by_data(m_addrspaces_view, this);
		g_object_unref(m_addrspaces_view);
	}
	if (m_addrspaces_model)
		g_object_unref(m_addrspaces_model);
	m_addrspaces_view = nullptr;
	m_addrspaces_model = nullptr;
	m_memory = nullptr;

	GtkWidget *const v_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_widget_set_margin_start(v_layout, 2);
	gtk_widget_set_margin_top(v_layout, 2);
	gtk_widget_set_margin_end(v_layout, 2);
	gtk_widget_set_margin_bottom(v_layout, 2);

	GtkWidget *const primary_frame = gtk_frame_new(nullptr);
	GtkWidget *const gl1 = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(gl1), 6);
	gtk_grid_attach(GTK_GRID(gl1), grid_label("Tag"), 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(gl1), grid_label(m_device->tag()), 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(gl1), grid_label("Name"), 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(gl1), grid_label(m_device->name()), 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(gl1), grid_label("Shortname"), 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(gl1), grid_label(m_device->shortname()), 1, 2, 1, 1);

	int cpos = 3;
	device_interface *intf = m_device->interfaces().first();
	if (intf)
	{
		gtk_grid_attach(GTK_GRID(gl1), grid_label("Interfaces"), 0, cpos, 1, 1);
		while (intf)
		{
			gtk_grid_attach(GTK_GRID(gl1), grid_label(intf->interface_type()), 1, cpos, 1, 1);
			cpos++;
			intf = intf->interface_next();
		}
	}

	gtk_frame_set_child(GTK_FRAME(primary_frame), gl1);
	gtk_box_append(GTK_BOX(v_layout), primary_frame);

	device_memory_interface *d_memory;
	if (m_device->interface(d_memory))
	{
		for (int i = 0; i < d_memory->max_space_count(); i++)
		{
			if (d_memory->has_space(i))
			{
				m_memory = d_memory;

				GListStore *const spaces = g_list_store_new(G_TYPE_OBJECT);
				for (int j = 0; j < m_memory->max_space_count(); j++)
				{
					if (m_memory->has_space(j))
					{
						GObject *const space = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
						g_object_set_data(space, SPACE_KEY, GINT_TO_POINTER(j));
						g_list_store_append(spaces, space);
						g_object_unref(space);
					}
				}
				m_addrspaces_model = gtk_single_selection_new(G_LIST_MODEL(spaces));
				g_object_ref(m_addrspaces_model);

				GtkWidget *const f = gtk_frame_new(nullptr);
				GtkWidget *const vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
				gtk_box_append(GTK_BOX(vb), grid_label("Address spaces"));

				m_addrspaces_view = gtk_column_view_new(GTK_SELECTION_MODEL(m_addrspaces_model));
				g_object_ref_sink(m_addrspaces_view);
				add_space_column("", 0);
				add_space_column("Name", 1);
				add_space_column("Data width", 2);
				add_space_column("Address width", 3);
				add_space_column("Address shift", 4);
				add_space_column("Alignment", 5);
				add_space_column("Endianness", 6);
				g_signal_connect(m_addrspaces_view, "activate", G_CALLBACK(on_activate), this);

				GtkWidget *const t = gtk_scrolled_window_new();
				gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(t), TRUE);
				gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(t), m_addrspaces_view);
				gtk_box_append(GTK_BOX(vb), t);
				gtk_frame_set_child(GTK_FRAME(f), vb);
				gtk_box_append(GTK_BOX(v_layout), f);
				break;
			}
		}
	}

	// the stretch below the frames
	GtkWidget *const stretch = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_vexpand(stretch, TRUE);
	gtk_box_append(GTK_BOX(v_layout), stretch);

	set_content(v_layout);
}

} // namespace osd::debugger::gtk
