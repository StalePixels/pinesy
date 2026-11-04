#include "signals.hpp"

namespace pinesy::gtk {

namespace {

template <typename F>
void free_function(gpointer data, GClosure *)
{
	delete static_cast<F *>(data);
}

void call_plain(gpointer, gpointer data)
{
	(*static_cast<std::function<void()> *>(data))();
}

void call_notify(gpointer, GParamSpec *, gpointer data)
{
	(*static_cast<std::function<void()> *>(data))();
}

void call_string(gpointer, const char *text, gpointer data)
{
	(*static_cast<std::function<void(const std::string &)> *>(data))(text ? text : "");
}

} // namespace

gulong connect(gpointer instance, const char *signal, std::function<void()> fn)
{
	using F = std::function<void()>;
	return g_signal_connect_data(instance, signal, G_CALLBACK(call_plain), new F(std::move(fn)),
			free_function<F>, GConnectFlags(0));
}

gulong connect_notify(gpointer instance, const char *property, std::function<void()> fn)
{
	using F = std::function<void()>;
	std::string signal = std::string("notify::") + property;
	return g_signal_connect_data(instance, signal.c_str(), G_CALLBACK(call_notify), new F(std::move(fn)),
			free_function<F>, GConnectFlags(0));
}

gulong connect_string(gpointer instance, const char *signal, std::function<void(const std::string &)> fn)
{
	using F = std::function<void(const std::string &)>;
	return g_signal_connect_data(instance, signal, G_CALLBACK(call_string), new F(std::move(fn)),
			free_function<F>, GConnectFlags(0));
}

Connections::~Connections()
{
	for (auto [instance, handler] : m_handlers)
	{
		if (g_signal_handler_is_connected(instance, handler))
			g_signal_handler_disconnect(instance, handler);
		g_object_unref(instance);
	}
}

gulong Connections::add(gpointer instance, gulong handler)
{
	m_handlers.emplace_back(G_OBJECT(g_object_ref(instance)), handler);
	return handler;
}

} // namespace pinesy::gtk
