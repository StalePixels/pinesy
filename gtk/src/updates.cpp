#include "updates.hpp"

#include "application.hpp"
#include "settings.hpp"

#include <adwaita.h>
#include <giomm/dbuserror.h>
#include <libsoup/soup.h>

#include <unistd.h>

#include <memory>
#include <vector>

namespace pinesy::gtk {

namespace {

namespace updates = core::updates;

constexpr const char *portal_name = "org.freedesktop.portal.Flatpak";
constexpr const char *portal_path = "/org/freedesktop/portal/Flatpak";
constexpr const char *portal_interface = "org.freedesktop.portal.Flatpak";
constexpr const char *monitor_interface = "org.freedesktop.portal.Flatpak.UpdateMonitor";
// Spawn's flag FLATPAK_SPAWN_FLAGS_LATEST_VERSION: the newest installed build.
constexpr guint32 spawn_latest_version = 2;
// The status of the UpdateMonitor's Progress signal.
constexpr guint32 status_done = 2;
constexpr guint32 status_failed = 3;

// A string member of an a{sv} dictionary; empty when it has none.
std::string string_member(const Glib::VariantBase &dictionary, const char *key)
{
	GVariant *value = g_variant_lookup_value(const_cast<GVariant *>(dictionary.gobj()), key, G_VARIANT_TYPE_STRING);
	if (!value) return {};
	std::string text = g_variant_get_string(value, nullptr);
	g_variant_unref(value);
	return text;
}

guint32 uint_member(const Glib::VariantBase &dictionary, const char *key, guint32 otherwise)
{
	guint32 result = otherwise;
	g_variant_lookup(const_cast<GVariant *>(dictionary.gobj()), key, "u", &result);
	return result;
}

} // anonymous namespace

Updates::Updates(Application &app) : m_app(app)
{
}

void Updates::start()
{
	const updates::Identity identity = m_app.core().identity();
	if (!updates::checks(identity, m_app.core().settings())) return;
	if (identity.kind == "flatpak")
		start_flatpak();
	else if (identity.kind == "deb")
		start_deb();
}

//------------------------------------------------------------
//  Flatpak: the portal's UpdateMonitor
//------------------------------------------------------------

void Updates::start_flatpak()
{
	m_bus = m_app.get_dbus_connection();
	if (!m_bus) return;

	// The update monitor needs version 2 of the portal.
	m_bus->call(portal_path, "org.freedesktop.DBus.Properties", "Get",
			Glib::VariantContainerBase::create_tuple({ Glib::Variant<Glib::ustring>::create(portal_interface),
					Glib::Variant<Glib::ustring>::create("version") }),
			[this](const Glib::RefPtr<Gio::AsyncResult> &result) {
				guint32 version = 0;
				try
				{
					auto reply = m_bus->call_finish(result);
					Glib::Variant<Glib::VariantBase> boxed;
					reply.get_child(boxed, 0);
					GVariant *inner = boxed.get().gobj();
					if (g_variant_is_of_type(inner, G_VARIANT_TYPE_UINT32)) version = g_variant_get_uint32(inner);
				}
				catch (const Glib::Error &)
				{
					return;
				}
				if (version < 2) return;

				m_bus->call(portal_path, portal_interface, "CreateUpdateMonitor",
						Glib::VariantContainerBase(g_variant_new_parsed("(@a{sv} {},)"), false),
						[this](const Glib::RefPtr<Gio::AsyncResult> &created) {
							try
							{
								auto reply = m_bus->call_finish(created);
								Glib::Variant<Glib::DBusObjectPathString> path;
								reply.get_child(path, 0);
								m_monitor = path.get();
							}
							catch (const Glib::Error &)
							{
								return;
							}
							m_bus->signal_subscribe(
									[this](const Glib::RefPtr<Gio::DBus::Connection> &, const Glib::ustring &, const Glib::ustring &,
											const Glib::ustring &, const Glib::ustring &, const Glib::VariantContainerBase &parameters) {
										Glib::VariantBase info;
										parameters.get_child(info, 0);
										const std::string commit = string_member(info, "remote-commit");
										if (!m_updating) notice(commit, true);
									},
									portal_name, monitor_interface, "UpdateAvailable", m_monitor);
							m_bus->signal_subscribe(
									[this](const Glib::RefPtr<Gio::DBus::Connection> &, const Glib::ustring &, const Glib::ustring &,
											const Glib::ustring &, const Glib::ustring &, const Glib::VariantContainerBase &parameters) {
										progress(parameters);
									},
									portal_name, monitor_interface, "Progress", m_monitor);
						},
						portal_name);
			},
			portal_name);
}

void Updates::update_flatpak()
{
	m_updating = true;
	m_bus->call(m_monitor, monitor_interface, "Update",
			Glib::VariantContainerBase(g_variant_new_parsed("(%s, @a{sv} {})", ""), false),
			[this](const Glib::RefPtr<Gio::AsyncResult> &result) {
				try
				{
					m_bus->call_finish(result);
				}
				catch (const Glib::Error &error)
				{
					m_updating = false;
					gchar *name = g_dbus_error_get_remote_error(error.gobj());
					const std::string remote = name ? name : "";
					g_free(name);
					GError *copy = g_error_copy(error.gobj());
					g_dbus_error_strip_remote_error(copy);
					const std::string message = copy->message;
					g_error_free(copy);
					failed(remote, message);
				}
			},
			portal_name);
}

void Updates::progress(const Glib::VariantContainerBase &parameters)
{
	Glib::VariantBase info;
	parameters.get_child(info, 0);
	const guint32 status = uint_member(info, "status", 0);
	if (status == status_done)
	{
		m_updating = false;
		spawn_latest();
	}
	else if (status == status_failed)
	{
		m_updating = false;
		failed(string_member(info, "error"), string_member(info, "error_message"));
	}
}

void Updates::failed(const std::string &error, const std::string &message)
{
	const updates::Texts texts = updates::texts(m_app.core().identity(), "", true);
	if (error == "org.freedesktop.DBus.Error.NotSupported")
		m_app.alert(texts.title, texts.new_permissions);
	else
		m_app.alert(texts.title, message.empty() ? texts.failed : texts.failed + " " + message);
}

// Starts the newest installed build, which replaces this instance on the
// session bus, and quits.
void Updates::spawn_latest()
{
	char *cwd = g_get_current_dir();
	GVariantBuilder argv;
	g_variant_builder_init(&argv, G_VARIANT_TYPE("aay"));
	g_variant_builder_add(&argv, "^ay", "pinesy");
	GVariantBuilder envs;
	g_variant_builder_init(&envs, G_VARIANT_TYPE("a{ss}"));
	g_variant_builder_add(&envs, "{ss}", replace_variable, "1");
	GVariantBuilder fds;
	g_variant_builder_init(&fds, G_VARIANT_TYPE("a{uh}"));
	GVariant *parameters = g_variant_new("(^ayaaya{uh}a{ss}u@a{sv})", cwd, &argv, &fds, &envs, spawn_latest_version,
			g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0));
	g_free(cwd);
	m_bus->call(portal_path, portal_interface, "Spawn", Glib::VariantContainerBase(parameters, false),
			[this](const Glib::RefPtr<Gio::AsyncResult> &result) {
				try
				{
					m_bus->call_finish(result);
				}
				catch (const Glib::Error &error)
				{
					failed("", error.what());
					return;
				}
				m_app.quit_after_update();
			},
			portal_name);
}

//------------------------------------------------------------
//  Debian package: the track's apt index
//------------------------------------------------------------

void Updates::start_deb()
{
	const updates::Identity identity = m_app.core().identity();
	const std::string url = updates::apt_packages_url(identity, updates::debian_architecture());
	SoupSession *session = soup_session_new();
	SoupMessage *message = soup_message_new("GET", url.c_str());
	if (!message)
	{
		g_object_unref(session);
		return;
	}
	struct Request
	{
		Updates *self;
		SoupMessage *message;
	};
	auto *request = new Request{ this, message };
	soup_session_send_and_read_async(session, message, G_PRIORITY_DEFAULT, nullptr,
			[](GObject *source, GAsyncResult *result, gpointer data) {
				std::unique_ptr<Request> request(static_cast<Request *>(data));
				GBytes *bytes = soup_session_send_and_read_finish(SOUP_SESSION(source), result, nullptr);
				const bool ok = bytes && soup_message_get_status(request->message) == SOUP_STATUS_OK;
				if (ok)
				{
					gsize size = 0;
					const char *text = static_cast<const char *>(g_bytes_get_data(bytes, &size));
					const std::string newest = updates::apt_newest_version(std::string_view(text ? text : "", size), "pinesy");
					request->self->notice(newest, false);
				}
				if (bytes) g_bytes_unref(bytes);
				g_object_unref(request->message);
				g_object_unref(source);
			},
			request);
}

//------------------------------------------------------------
//  The notice
//------------------------------------------------------------

void Updates::notice(const std::string &build, bool newer_known)
{
	const updates::Identity identity = m_app.core().identity();
	if (!updates::notice(identity, m_app.core().settings(), build, newer_known)) return;
	const updates::Texts texts = updates::texts(identity, build, newer_known);

	AdwDialog *dialog = adw_alert_dialog_new(texts.title.c_str(), texts.message.c_str());
	AdwAlertDialog *alert = ADW_ALERT_DIALOG(dialog);
	adw_alert_dialog_set_body_use_markup(alert, FALSE);
	adw_alert_dialog_add_response(alert, "never-remind", texts.never_remind.c_str());
	adw_alert_dialog_add_response(alert, "ignore-build", texts.ignore_build.c_str());
	adw_alert_dialog_add_response(alert, "update", texts.update.c_str());
	adw_alert_dialog_set_response_appearance(alert, "update", ADW_RESPONSE_SUGGESTED);
	adw_alert_dialog_set_default_response(alert, "update");
	adw_alert_dialog_set_close_response(alert, "close");
	adw_alert_dialog_set_prefer_wide_layout(alert, TRUE);

	struct Choice
	{
		Updates *self;
		std::string build;
	};
	g_signal_connect_data(dialog, "response",
			G_CALLBACK(+[](AdwAlertDialog *, const char *response, gpointer data) {
				auto *choice = static_cast<Choice *>(data);
				choice->self->chosen(response, choice->build);
			}),
			new Choice{ this, build }, [](gpointer data, GClosure *) { delete static_cast<Choice *>(data); },
			GConnectFlags(0));
	auto window = m_app.get_active_window();
	adw_dialog_present(dialog, window ? GTK_WIDGET(window->gobj()) : nullptr);
}

void Updates::chosen(const std::string &response, const std::string &build)
{
	const updates::Identity identity = m_app.core().identity();
	updates::Choice choice;
	if (response == "never-remind")
		choice = updates::Choice::never_remind;
	else if (response == "ignore-build")
		choice = updates::Choice::ignore_build;
	else if (response == "update")
		choice = updates::Choice::update;
	else
		return;

	const core::Settings changed = updates::after_choice(m_app.core().settings(), choice, build);
	m_app.settings()->set_boolean(key::update_checks, changed.update_checks);
	m_app.settings()->set_string(key::ignored_build, changed.ignored_build);
	m_app.core().set_settings(changed);

	if (choice != updates::Choice::update) return;
	if (identity.kind == "flatpak")
		update_flatpak();
	else
		m_app.alert(updates::texts(identity, build, false).title, updates::texts(identity, build, false).package_manager);
}

} // namespace pinesy::gtk
