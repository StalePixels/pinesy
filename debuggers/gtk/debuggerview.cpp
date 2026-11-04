// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "debuggerview.h"

#include "modules/debugger/xmlconfig.h"

#include "modules/lib/osdobj_common.h"

#include "xmlfile.h"

#include <pango/pangocairo.h>

#include <algorithm>
#include <cstring>
#include <string>


namespace osd::debugger::gtk {

namespace {

struct view_palette
{
	GdkRGBA text;
	GdkRGBA window;
	GdkRGBA base;
	GdkRGBA highlight;
	GdkRGBA accent;
};


GdkRGBA rgb(int r, int g, int b)
{
	return GdkRGBA{ float(r) / 255.0F, float(g) / 255.0F, float(b) / 255.0F, 1.0F };
}


// The theme colours that stand in for the Qt palette: window text, window,
// base, highlight and accent.  GTK has no other way to read a named theme
// colour than the deprecated style context, so the defaults are the GTK
// default theme's values for when the theme does not name them.
view_palette get_palette(GtkWidget *widget)
{
	view_palette palette;
	gtk_widget_get_color(widget, &palette.text);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	GtkStyleContext *const context = gtk_widget_get_style_context(widget);
	if (!gtk_style_context_lookup_color(context, "theme_bg_color", &palette.window))
		palette.window = rgb(0xf6, 0xf5, 0xf4);
	if (!gtk_style_context_lookup_color(context, "theme_base_color", &palette.base))
		palette.base = rgb(0xff, 0xff, 0xff);
	if (!gtk_style_context_lookup_color(context, "theme_selected_bg_color", &palette.highlight))
		palette.highlight = rgb(0x35, 0x84, 0xe4);
	if (!gtk_style_context_lookup_color(context, "accent_color", &palette.accent))
		palette.accent = palette.highlight;
	G_GNUC_END_IGNORE_DEPRECATIONS

	palette.window.alpha = palette.base.alpha = palette.highlight.alpha = palette.accent.alpha = 1.0F;
	return palette;
}


void append_char(std::string &text, u8 ch)
{
	// view bytes are Latin-1, as QChar takes them
	if (0x80 > ch)
	{
		text += char(ch);
	}
	else
	{
		text += char(0xc0 | (ch >> 6));
		text += char(0x80 | (ch & 0x3f));
	}
}

} // anonymous namespace


debugger_view::debugger_view(debug_view_type type, running_machine &machine) :
	m_machine(machine),
	m_view(nullptr),
	m_root(gtk_grid_new()),
	m_area(gtk_drawing_area_new()),
	m_vadjustment(gtk_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0)),
	m_hadjustment(gtk_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0)),
	m_vscroll(nullptr),
	m_hscroll(nullptr),
	m_popover(nullptr),
	m_keys(gtk_event_controller_key_new()),
	m_click(gtk_gesture_click_new()),
	m_scroll(gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES)),
	m_actions(g_simple_action_group_new()),
	m_paste_action(nullptr),
	m_font(pango_font_description_new()),
	m_alive(std::make_shared<debugger_view *>(this)),
	m_font_width(1.0),
	m_font_height(1),
	m_content_width(0),
	m_content_height(0),
	m_idle(0),
	m_prefer_bottom(false),
	m_in_layout(false),
	m_in_resize(false),
	m_in_draw(false)
{
	g_object_ref_sink(m_root);
	g_object_ref_sink(m_vadjustment);
	g_object_ref_sink(m_hadjustment);

	// The font is set per view, so it does not change the menu fonts
	osd_options const &options = downcast<osd_options &>(m_machine.options());
	char const *const selected_font = options.debugger_font();
	float const selected_font_size = options.debugger_font_size();
	std::string family((!*selected_font || !strcmp(selected_font, OSDOPTVAL_AUTO)) ? "Courier New" : selected_font);
	family += ",monospace";
	pango_font_description_set_family(m_font, family.c_str());
	pango_font_description_set_size(m_font, int((selected_font_size <= 0) ? 11 : selected_font_size) * PANGO_SCALE);

	// The character grid and its scroll bars; the vertical bar is always shown
	m_vscroll = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, m_vadjustment);
	m_hscroll = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, m_hadjustment);
	gtk_widget_set_visible(m_hscroll, FALSE);
	gtk_widget_set_hexpand(m_area, TRUE);
	gtk_widget_set_vexpand(m_area, TRUE);
	gtk_widget_set_focusable(m_area, TRUE);
	gtk_grid_attach(GTK_GRID(m_root), m_area, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(m_root), m_vscroll, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(m_root), m_hscroll, 0, 1, 1, 1);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_area), &debugger_view::on_draw, this, nullptr);
	g_signal_connect(m_area, "resize", G_CALLBACK(on_resize), this);
	g_signal_connect(m_vadjustment, "value-changed", G_CALLBACK(on_vertical_scroll), this);
	g_signal_connect(m_hadjustment, "value-changed", G_CALLBACK(on_horizontal_scroll), this);

	g_signal_connect(m_keys, "key-pressed", G_CALLBACK(on_key_pressed), this);
	gtk_widget_add_controller(m_area, m_keys);

	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(m_click), 0);
	g_signal_connect(m_click, "pressed", G_CALLBACK(on_pressed), this);
	gtk_widget_add_controller(m_area, GTK_EVENT_CONTROLLER(m_click));

	g_signal_connect(m_scroll, "scroll", G_CALLBACK(on_scroll), this);
	gtk_widget_add_controller(m_area, m_scroll);

	// Context menu
	GSimpleAction *const copy_action = g_simple_action_new("copy-visible", nullptr);
	g_signal_connect(copy_action, "activate", G_CALLBACK(on_copy_visible), this);
	g_action_map_add_action(G_ACTION_MAP(m_actions), G_ACTION(copy_action));
	g_object_unref(copy_action);
	m_paste_action = g_simple_action_new("paste", nullptr);
	g_signal_connect(m_paste_action, "activate", G_CALLBACK(on_paste), this);
	g_action_map_add_action(G_ACTION_MAP(m_actions), G_ACTION(m_paste_action));
	g_object_unref(m_paste_action);
	gtk_widget_insert_action_group(m_root, "view", G_ACTION_GROUP(m_actions));

	// the grid's layout manager places the popover; the drawing area would not
	m_popover = gtk_popover_menu_new_from_model(nullptr);
	gtk_popover_set_has_arrow(GTK_POPOVER(m_popover), FALSE);
	gtk_widget_set_halign(m_popover, GTK_ALIGN_START);
	gtk_widget_set_parent(m_popover, m_root);

	m_view = m_machine.debug_view().alloc_view(
			type,
			debugger_view::debugger_view_update,
			this);
}


debugger_view::~debugger_view()
{
	if (m_idle)
		g_source_remove(m_idle);
	m_alive.reset();

	if (m_view)
		m_machine.debug_view().free_view(*m_view);
	m_view = nullptr;

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_area), nullptr, nullptr, nullptr);
	g_signal_handlers_disconnect_by_data(m_area, this);
	g_signal_handlers_disconnect_by_data(m_vadjustment, this);
	g_signal_handlers_disconnect_by_data(m_hadjustment, this);
	g_signal_handlers_disconnect_by_data(m_keys, this);
	g_signal_handlers_disconnect_by_data(m_click, this);
	g_signal_handlers_disconnect_by_data(m_scroll, this);
	char **const names = g_action_group_list_actions(G_ACTION_GROUP(m_actions));
	for (char **name = names; *name; ++name)
		g_signal_handlers_disconnect_by_data(g_action_map_lookup_action(G_ACTION_MAP(m_actions), *name), this);
	g_strfreev(names);

	gtk_widget_unparent(m_popover);
	g_object_unref(m_actions);
	g_object_unref(m_vadjustment);
	g_object_unref(m_hadjustment);
	pango_font_description_free(m_font);
	g_object_unref(m_root);
}


void debugger_view::set_focusable(bool focusable)
{
	gtk_widget_set_focusable(m_area, focusable ? TRUE : FALSE);
}


void debugger_view::set_margins(int left, int top, int right, int bottom)
{
	gtk_widget_set_margin_start(m_root, left);
	gtk_widget_set_margin_top(m_root, top);
	gtk_widget_set_margin_end(m_root, right);
	gtk_widget_set_margin_bottom(m_root, bottom);
}


int debugger_view::source_index() const
{
	if (m_view)
	{
		debug_view_source const *const source = m_view->source();
		if (source)
			return m_view->source_index(*source);
	}
	return -1;
}


void debugger_view::update()
{
	if (m_in_draw || m_in_resize)
		schedule_layout();
	else
		update_layout();
	gtk_widget_queue_draw(m_area);
}


void debugger_view::measure_font()
{
	PangoLayout *const layout = gtk_widget_create_pango_layout(m_area, std::string(100, '_').c_str());
	pango_layout_set_font_description(layout, m_font);
	PangoRectangle logical;
	pango_layout_get_extents(layout, nullptr, &logical);
	g_object_unref(layout);

	m_font_width = double(logical.width) / PANGO_SCALE / 100.0;
	if (0.0 >= m_font_width)
		m_font_width = 1.0;
	m_font_height = std::max(1, PANGO_PIXELS_CEIL(logical.height));
}


// What the Qt debugger does at the start of each paint: tell the MAME debug
// view how much space is available and set up the scroll bars.  GTK does not
// allow this while drawing, so it runs on resize and on each view update.
void debugger_view::update_layout()
{
	if (!m_view || m_in_layout)
		return;

	int const area_width = gtk_widget_get_width(m_area);
	int const area_height = gtk_widget_get_height(m_area);
	if ((0 >= area_width) || (0 >= area_height))
		return;

	m_in_layout = true;
	measure_font();

	int hbar_height = 0;
	gtk_widget_measure(m_hscroll, GTK_ORIENTATION_VERTICAL, -1, nullptr, &hbar_height, nullptr, nullptr);
	bool const hbar_shown = gtk_widget_get_visible(m_hscroll);
	int const full_height = area_height + (hbar_shown ? hbar_height : 0);

	int const line_width = int(area_width / m_font_width);
	bool const full_width = line_width >= m_view->total_size().x;
	int const content_height = full_height - (full_width ? 0 : hbar_height);
	m_view->set_visible_size(debug_view_xy(line_width, content_height / m_font_height));
	m_content_width = area_width;
	m_content_height = content_height;

	debug_view_xy const total = m_view->total_size();
	debug_view_xy const visible = m_view->visible_size();
	debug_view_xy const position = m_view->visible_position();

	// Handle the scroll bars
	gtk_adjustment_configure(
			m_hadjustment,
			position.x,
			0.0,
			(std::max)(total.x, visible.x),
			1.0,
			(std::max)(1, line_width - 1),
			visible.x);

	bool const at_end = gtk_adjustment_get_value(m_vadjustment) >= (gtk_adjustment_get_upper(m_vadjustment) - gtk_adjustment_get_page_size(m_vadjustment));
	gtk_adjustment_configure(
			m_vadjustment,
			position.y,
			0.0,
			(std::max)(total.y, visible.y),
			1.0,
			(std::max)(1, (content_height / m_font_height) - 1),
			visible.y);
	if (m_prefer_bottom && at_end)
		gtk_adjustment_set_value(m_vadjustment, (std::max)(0, total.y - visible.y));

	// the horizontal bar is shown only when it is needed
	if (hbar_shown == full_width)
	{
		if (m_in_resize)
			schedule_layout();
		else
			gtk_widget_set_visible(m_hscroll, full_width ? FALSE : TRUE);
	}

	m_in_layout = false;
	gtk_widget_queue_draw(m_area);
}


void debugger_view::schedule_layout()
{
	if (!m_idle)
		m_idle = g_idle_add(&debugger_view::on_idle, this);
}


gboolean debugger_view::on_idle(gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	self.m_idle = 0;
	self.update_layout();
	if (self.m_updated)
		self.m_updated();
	return G_SOURCE_REMOVE;
}


void debugger_view::on_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	if (!self.m_view)
		return;

	self.m_in_draw = true;

	view_palette const palette = get_palette(GTK_WIDGET(area));
	auto const fill =
			[cr] (GdkRGBA const &color, double x, double y, double w, double h)
			{
				gdk_cairo_set_source_rgba(cr, &color);
				cairo_rectangle(cr, x, y, w, h);
				cairo_fill(cr);
			};

	// Draw the viewport widget
	fill(palette.window, 0, 0, width, height);

	PangoLayout *const layout = gtk_widget_create_pango_layout(GTK_WIDGET(area), nullptr);
	pango_layout_set_font_description(layout, self.m_font);

	double const font_width = self.m_font_width;
	int const font_height = self.m_font_height;
	int const content_width = width;
	int const content_height = height;

	debug_view_xy const visible_char_dims = self.m_view->visible_size();
	debug_view_char const *viewdata = self.m_view->viewdata();

	std::string text;
	for (int y = 0; y < visible_char_dims.y; y++, viewdata += visible_char_dims.x)
	{
		int run = 1;
		for (int x = 0; x < visible_char_dims.x; x += run)
		{
			u8 const text_attr = viewdata[x].attrib;

			// Text color handling
			GdkRGBA fg_color = palette.text;
			GdkRGBA bg_color = palette.window;

			if (text_attr & DCA_VISITED)
				bg_color = rgb(0xc6, 0xe2, 0xff);

			if (text_attr & DCA_ANCILLARY)
				bg_color = palette.base;

			if (text_attr & DCA_SELECTED)
				bg_color = rgb(0xcb, 0x4b, 0x16);

			if (text_attr & DCA_CURRENT)
				bg_color = palette.highlight;

			if ((text_attr & DCA_SELECTED) && (text_attr & DCA_CURRENT))
				bg_color = palette.accent;

			if (text_attr & DCA_CHANGED)
				fg_color = rgb(0xff, 0x00, 0x00);

			if (text_attr & DCA_INVALID)
				fg_color = rgb(0x00, 0x00, 0xff);

			if (text_attr & DCA_DISABLED)
			{
				fg_color.red = (fg_color.red + bg_color.red) / 2.0F;
				fg_color.green = (fg_color.green + bg_color.green) / 2.0F;
				fg_color.blue = (fg_color.blue + bg_color.blue) / 2.0F;
			}

			if (text_attr & DCA_COMMENT)
				fg_color = rgb(0x00, 0x80, 0x00);

			text.clear();
			append_char(text, viewdata[x].byte);
			for (run = 1; (x + run) < visible_char_dims.x; run++)
			{
				if (text_attr != viewdata[x + run].attrib)
					break;
				append_char(text, viewdata[x + run].byte);
			}

			// Characters are not guaranteed to fill the whole cell, so fill before.
			double const left = x * font_width;
			double const run_width = ((x + run) < visible_char_dims.x) ? (run * font_width) : (content_width - left);
			fill(bg_color, left, y * font_height, run_width, font_height);

			if (((y + 1) == visible_char_dims.y) && (content_height > (visible_char_dims.y * font_height)))
			{
				fill(
						(text_attr & DCA_ANCILLARY) ? palette.base : palette.window,
						left,
						visible_char_dims.y * font_height,
						run_width,
						content_height - (visible_char_dims.y * font_height));
			}

			gdk_cairo_set_source_rgba(cr, &fg_color);
			cairo_move_to(cr, left, y * font_height);
			pango_layout_set_text(layout, text.data(), text.size());
			pango_cairo_show_layout(cr, layout);
		}
	}

	g_object_unref(layout);
	self.m_in_draw = false;
}


void debugger_view::on_resize(GtkDrawingArea *area, int width, int height, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	self.m_in_resize = true;
	self.update_layout();
	self.m_in_resize = false;
}


void debugger_view::restore_configuration_from_node(util::xml::data_node const &node)
{
	if (m_view->cursor_supported())
	{
		util::xml::data_node const *const selection = node.get_child(NODE_WINDOW_SELECTION);
		if (selection)
		{
			debug_view_xy pos = m_view->cursor_position();
			m_view->set_cursor_visible(0 != selection->get_attribute_int(ATTR_SELECTION_CURSOR_VISIBLE, m_view->cursor_visible() ? 1 : 0));
			selection->get_attribute_int(ATTR_SELECTION_CURSOR_X, pos.x);
			selection->get_attribute_int(ATTR_SELECTION_CURSOR_Y, pos.y);
			m_view->set_cursor_position(pos);
		}
	}
}


void debugger_view::save_configuration_to_node(util::xml::data_node &node)
{
	if (m_view->cursor_supported())
	{
		util::xml::data_node *const selection = node.add_child(NODE_WINDOW_SELECTION, nullptr);
		if (selection)
		{
			debug_view_xy const pos = m_view->cursor_position();
			selection->set_attribute_int(ATTR_SELECTION_CURSOR_VISIBLE, m_view->cursor_visible() ? 1 : 0);
			selection->set_attribute_int(ATTR_SELECTION_CURSOR_X, pos.x);
			selection->set_attribute_int(ATTR_SELECTION_CURSOR_Y, pos.y);
		}
	}
}


gboolean debugger_view::on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	if (!self.m_view)
		return FALSE;

	bool const ctrl_down = (state & GDK_CONTROL_MASK) != 0;

	int key_press = -1;
	switch (keyval)
	{
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
		key_press = DCH_UP;
		break;
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
		key_press = DCH_DOWN;
		break;
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
		key_press = DCH_LEFT;
		if (ctrl_down)
			key_press = DCH_CTRLLEFT;
		break;
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		key_press = DCH_RIGHT;
		if (ctrl_down)
			key_press = DCH_CTRLRIGHT;
		break;
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
		key_press = DCH_PUP;
		break;
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
		key_press = DCH_PDOWN;
		break;
	case GDK_KEY_Home:
	case GDK_KEY_KP_Home:
		key_press = DCH_HOME;
		if (ctrl_down)
			key_press = DCH_CTRLHOME;
		break;
	case GDK_KEY_End:
	case GDK_KEY_KP_End:
		key_press = DCH_END;
		if (ctrl_down)
			key_press = DCH_CTRLEND;
		break;
	case GDK_KEY_Menu:
		self.context_menu(gtk_widget_get_width(self.m_area) / 2, gtk_widget_get_height(self.m_area) / 2);
		return TRUE;
	default:
		if ((GDK_KEY_0 <= keyval) && (GDK_KEY_9 >= keyval))
			key_press = '0' + (keyval - GDK_KEY_0);
		else if ((GDK_KEY_KP_0 <= keyval) && (GDK_KEY_KP_9 >= keyval))
			key_press = '0' + (keyval - GDK_KEY_KP_0);
		else if ((GDK_KEY_a <= keyval) && (GDK_KEY_f >= keyval))
			key_press = 'a' + (keyval - GDK_KEY_a);
		else if ((GDK_KEY_A <= keyval) && (GDK_KEY_F >= keyval))
			key_press = 'a' + (keyval - GDK_KEY_A);
		else
			return FALSE;
		break;
	}

	self.m_view->set_cursor_visible(true);
	self.m_view->process_char(key_press);

	// Catch the view up with the cursor
	self.update();
	return TRUE;
}


void debugger_view::on_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	if (!self.m_view)
		return;

	if (gtk_widget_get_focusable(self.m_area))
		gtk_widget_grab_focus(self.m_area);

	debug_view_xy const top_left = self.m_view->visible_position();
	debug_view_xy const visible_char_dims = self.m_view->visible_size();
	debug_view_xy click_view_position;
	click_view_position.x = (std::min)(int(top_left.x + (x / self.m_font_width)), top_left.x + visible_char_dims.x - 1);
	click_view_position.y = (std::min)(int(top_left.y + (y / self.m_font_height)), top_left.y + visible_char_dims.y - 1);

	guint const button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
	if (GDK_BUTTON_PRIMARY == button)
	{
		self.m_view->process_click(DCK_LEFT_CLICK, click_view_position);
	}
	else if (GDK_BUTTON_MIDDLE == button)
	{
		self.m_view->process_click(DCK_MIDDLE_CLICK, click_view_position);
	}
	else if (GDK_BUTTON_SECONDARY == button)
	{
		if (self.m_view->cursor_supported())
		{
			self.m_view->set_cursor_position(click_view_position);
			self.m_view->set_cursor_visible(true);
		}
	}

	self.update();

	if (GDK_BUTTON_SECONDARY == button)
		self.context_menu(x, y);
}


// Qt scrolls three lines for each wheel step
gboolean debugger_view::on_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	double rows = dy;
	double columns = dx;
	if (GDK_SCROLL_UNIT_WHEEL == gtk_event_controller_scroll_get_unit(controller))
	{
		rows *= 3.0;
		columns *= 3.0;
	}
	else
	{
		rows /= self.m_font_height;
		columns /= self.m_font_width;
	}
	if (0.0 != rows)
		gtk_adjustment_set_value(self.m_vadjustment, gtk_adjustment_get_value(self.m_vadjustment) + rows);
	if (0.0 != columns)
		gtk_adjustment_set_value(self.m_hadjustment, gtk_adjustment_get_value(self.m_hadjustment) + columns);
	return TRUE;
}


void debugger_view::context_menu(double x, double y)
{
	GMenu *const menu = g_menu_new();
	add_items_to_context_menu(menu);
	gtk_popover_menu_set_menu_model(GTK_POPOVER_MENU(m_popover), G_MENU_MODEL(menu));
	g_object_unref(menu);

	GdkRectangle const rect{ int(x), int(y), 1, 1 };
	gtk_popover_set_pointing_to(GTK_POPOVER(m_popover), &rect);
	gtk_popover_popup(GTK_POPOVER(m_popover));
}


void debugger_view::add_items_to_context_menu(GMenu *menu)
{
	GdkContentFormats *const formats = gdk_content_formats_union_deserialize_gtypes(
			gdk_content_formats_ref(gdk_clipboard_get_formats(gtk_widget_get_clipboard(m_area))));
	g_simple_action_set_enabled(m_paste_action, gdk_content_formats_contain_gtype(formats, G_TYPE_STRING));
	gdk_content_formats_unref(formats);

	GMenu *const section = g_menu_new();
	g_menu_append(section, "Copy Visible", "view.copy-visible");
	g_menu_append(section, "Paste", "view.paste");
	g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
	g_object_unref(section);
}


void debugger_view::on_vertical_scroll(GtkAdjustment *adjustment, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	if (self.m_view)
	{
		self.m_view->set_visible_position(debug_view_xy(int(gtk_adjustment_get_value(self.m_hadjustment)), int(gtk_adjustment_get_value(adjustment))));
		gtk_widget_queue_draw(self.m_area);
	}
}


void debugger_view::on_horizontal_scroll(GtkAdjustment *adjustment, gpointer user_data)
{
	debugger_view &self = *static_cast<debugger_view *>(user_data);
	if (self.m_view)
	{
		self.m_view->set_visible_position(debug_view_xy(int(gtk_adjustment_get_value(adjustment)), int(gtk_adjustment_get_value(self.m_vadjustment))));
		gtk_widget_queue_draw(self.m_area);
	}
}


void debugger_view::on_copy_visible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	static_cast<debugger_view *>(user_data)->copy_visible();
}


void debugger_view::on_paste(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	static_cast<debugger_view *>(user_data)->paste();
}


void debugger_view::copy_visible()
{
	// get visible text
	debug_view_xy const visarea = m_view->visible_size();
	debug_view_char const *viewdata = m_view->viewdata();
	if (!viewdata)
		return;

	// turn into a plain string, trimming trailing whitespace
	std::string text;
	for (uint32_t row = 0; row < visarea.y; row++, viewdata += visarea.x)
	{
		std::string::size_type const start = text.length();
		for (uint32_t col = 0; col < visarea.x; ++col)
			text += char(viewdata[col].byte);
		std::string::size_type const nonblank = text.find_last_not_of("\t\n\v\r ");
		if (nonblank != std::string::npos)
			text.resize((std::max)(start, nonblank + 1));
		text += "\n";
	}

	// copy to the clipboard
	gchar *const valid = g_utf8_make_valid(text.c_str(), text.length());
	gdk_clipboard_set_text(gtk_widget_get_clipboard(m_area), valid);
	g_free(valid);
}


void debugger_view::paste()
{
	gdk_clipboard_read_text_async(
			gtk_widget_get_clipboard(m_area),
			nullptr,
			&debugger_view::on_paste_ready,
			new std::weak_ptr<debugger_view *>(m_alive));
}


void debugger_view::on_paste_ready(GObject *source, GAsyncResult *result, gpointer user_data)
{
	std::unique_ptr<std::weak_ptr<debugger_view *> > const alive(static_cast<std::weak_ptr<debugger_view *> *>(user_data));
	GError *error = nullptr;
	char *const text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), result, &error);
	if (error)
		g_error_free(error);

	std::shared_ptr<debugger_view *> const self = alive->lock();
	if (text && self && (*self)->m_view)
	{
		for (char const *ch = text; *ch; ch = g_utf8_next_char(ch))
		{
			gunichar const code = g_utf8_get_char(ch);
			if ((32 <= code) && (127 >= code))
				(*self)->m_view->process_char(code);
		}
	}
	g_free(text);
}


void debugger_view::debugger_view_update(debug_view &debug_view, void *osd_private)
{
	// Get a handle to the debugger_view being updated and redraw
	debugger_view &self = *reinterpret_cast<debugger_view *>(osd_private);
	if (!self.m_view)
		return;

	if (self.m_in_draw || self.m_in_resize)
	{
		// layout, redraw and the updated callback run from the idle handler
		self.schedule_layout();
	}
	else
	{
		self.update_layout();
		gtk_widget_queue_draw(self.m_area);
		if (self.m_updated)
			self.m_updated();
	}
}

} // namespace osd::debugger::gtk
