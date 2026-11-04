// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_DEBUGGERVIEW_H
#define MAME_DEBUGGER_GTK_DEBUGGERVIEW_H

#pragma once

#include "debug/debugvw.h"

#include <gtk/gtk.h>

#include <functional>
#include <memory>


namespace osd::debugger::gtk {

class debugger_view
{
public:
	debugger_view(debug_view_type type, running_machine &machine);
	virtual ~debugger_view();

	// the widget to put in a layout: the character grid with its scroll bars
	GtkWidget *widget() const { return m_root; }

	// Setters and accessors
	void set_prefer_bottom(bool pb) { m_prefer_bottom = pb; }
	void set_focusable(bool focusable);
	void set_margins(int left, int top, int right, int bottom);
	void set_updated_callback(std::function<void ()> &&callback) { m_updated = std::move(callback); }
	debug_view *view() { return m_view; }
	template <typename T> T *view() { return downcast<T *>(m_view); }
	int source_index() const;

	// redraw and recompute the scroll bars
	void update();

	virtual void restore_configuration_from_node(util::xml::data_node const &node);
	virtual void save_configuration_to_node(util::xml::data_node &node);

protected:
	virtual void add_items_to_context_menu(GMenu *menu);

	GSimpleActionGroup *actions() const { return m_actions; }
	GtkWidget *area() const { return m_area; }

private:
	// Callback to allow MAME to refresh the view
	static void debugger_view_update(debug_view &debug_view, void *osd_private);

	static void on_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
	static void on_resize(GtkDrawingArea *area, int width, int height, gpointer user_data);
	static void on_vertical_scroll(GtkAdjustment *adjustment, gpointer user_data);
	static void on_horizontal_scroll(GtkAdjustment *adjustment, gpointer user_data);
	static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
	static void on_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
	static gboolean on_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data);
	static gboolean on_idle(gpointer user_data);
	static void on_copy_visible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
	static void on_paste(GSimpleAction *action, GVariant *parameter, gpointer user_data);
	static void on_paste_ready(GObject *source, GAsyncResult *result, gpointer user_data);

	void update_layout();
	void schedule_layout();
	void measure_font();
	void context_menu(double x, double y);
	void copy_visible();
	void paste();

	running_machine &m_machine;
	debug_view *m_view;

	GtkWidget *m_root;
	GtkWidget *m_area;
	GtkAdjustment *m_vadjustment;
	GtkAdjustment *m_hadjustment;
	GtkWidget *m_vscroll;
	GtkWidget *m_hscroll;
	GtkWidget *m_popover;
	GtkEventController *m_keys;
	GtkGesture *m_click;
	GtkEventController *m_scroll;
	GSimpleActionGroup *m_actions;
	GSimpleAction *m_paste_action;
	PangoFontDescription *m_font;

	std::function<void ()> m_updated;
	std::shared_ptr<debugger_view *> m_alive;

	double m_font_width;
	int m_font_height;
	int m_content_width;
	int m_content_height;
	guint m_idle;
	bool m_prefer_bottom;
	bool m_in_layout;
	bool m_in_resize;
	bool m_in_draw;
};

} // namespace osd::debugger::gtk

#endif // MAME_DEBUGGER_GTK_DEBUGGERVIEW_H
