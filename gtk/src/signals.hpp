// Connects C++ functions to the signals of libadwaita widgets, which
// gtkmm does not wrap. Each connection owns its function and frees it
// when the signal handler is disconnected or the object is finalized.

#ifndef PINESY_GTK_SIGNALS_HPP
#define PINESY_GTK_SIGNALS_HPP

#include <glib-object.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace pinesy::gtk {

// A signal with no arguments besides the instance, such as "clicked",
// "activated" or "closed".
gulong connect(gpointer instance, const char *signal, std::function<void()> fn);

// A property change ("notify::<property>").
gulong connect_notify(gpointer instance, const char *property, std::function<void()> fn);

// A signal with one string argument, such as AdwAlertDialog's "response".
gulong connect_string(gpointer instance, const char *signal, std::function<void(const std::string &)> fn);

// Signal handlers that must not outlive the object they call. A
// dialog's widgets can be disposed after the dialog itself, and can
// emit signals as they go, so their handlers are disconnected when the
// object that owns them is destroyed. Each instance is kept alive
// until then.
class Connections
{
public:
	Connections() = default;
	Connections(const Connections &) = delete;
	Connections &operator=(const Connections &) = delete;
	~Connections();

	// Keeps a handler of an instance; returns the handler id.
	gulong add(gpointer instance, gulong handler);

private:
	std::vector<std::pair<GObject *, gulong>> m_handlers;
};

} // namespace pinesy::gtk

#endif
