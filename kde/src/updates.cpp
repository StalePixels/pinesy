#include "updates.h"

#include "core.h"

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>

namespace {

const QString portalName = QStringLiteral("org.freedesktop.portal.Flatpak");
const QString portalPath = QStringLiteral("/org/freedesktop/portal/Flatpak");
const QString portalInterface = QStringLiteral("org.freedesktop.portal.Flatpak");
const QString monitorInterface = QStringLiteral("org.freedesktop.portal.Flatpak.UpdateMonitor");
// Spawn's flag FLATPAK_SPAWN_FLAGS_LATEST_VERSION: the newest installed build.
constexpr uint spawnLatestVersion = 2;
// The status of the UpdateMonitor's Progress signal.
constexpr uint statusDone = 2;
constexpr uint statusFailed = 3;

using Fds = QMap<uint, QDBusUnixFileDescriptor>;
using Envs = QMap<QString, QString>;

QString text(const QVariantMap &texts, const char *key)
{
	return texts.value(QLatin1String(key)).toString();
}

} // namespace

Q_DECLARE_METATYPE(Fds)
Q_DECLARE_METATYPE(Envs)

Updates::Updates(QObject *parent)
	: QObject(parent)
	, m_kind(Core::instance()->buildKind())
{
}

void Updates::start()
{
	if (!Core::instance()->updateChecks())
		return;
	if (m_kind == QLatin1String("flatpak"))
		startFlatpak();
	else if (m_kind == QLatin1String("deb"))
		startDeb();
}

//------------------------------------------------------------
//  Flatpak: the portal's UpdateMonitor
//------------------------------------------------------------

void Updates::startFlatpak()
{
	QDBusConnection bus = QDBusConnection::sessionBus();
	if (!bus.isConnected())
		return;

	// The update monitor needs version 2 of the portal.
	QDBusMessage get = QDBusMessage::createMethodCall(portalName, portalPath,
			QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
	get << portalInterface << QStringLiteral("version");
	auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(get), this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
		call->deleteLater();
		QDBusPendingReply<QDBusVariant> version = *call;
		if (version.isError() || version.value().variant().toUInt() < 2)
			return;

		QDBusMessage create = QDBusMessage::createMethodCall(portalName, portalPath, portalInterface,
				QStringLiteral("CreateUpdateMonitor"));
		create << QVariantMap();
		auto *created = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(create), this);
		connect(created, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
			call->deleteLater();
			QDBusPendingReply<QDBusObjectPath> monitor = *call;
			if (monitor.isError())
				return;
			m_monitor = monitor.value().path();
			QDBusConnection bus = QDBusConnection::sessionBus();
			bus.connect(portalName, m_monitor, monitorInterface, QStringLiteral("UpdateAvailable"), this,
					SLOT(updateAvailable(QVariantMap)));
			bus.connect(portalName, m_monitor, monitorInterface, QStringLiteral("Progress"), this,
					SLOT(progress(QVariantMap)));
		});
	});
}

void Updates::updateAvailable(const QVariantMap &info)
{
	if (!m_updating)
		notice(info.value(QStringLiteral("remote-commit")).toString(), true);
}

void Updates::updateFlatpak()
{
	m_updating = true;
	QDBusMessage update = QDBusMessage::createMethodCall(portalName, m_monitor, monitorInterface, QStringLiteral("Update"));
	update << QString() << QVariantMap();
	auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(update), this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
		call->deleteLater();
		QDBusPendingReply<> reply = *call;
		if (reply.isError()) {
			m_updating = false;
			failed(reply.error().name(), reply.error().message());
		}
	});
}

void Updates::progress(const QVariantMap &info)
{
	const uint status = info.value(QStringLiteral("status")).toUInt();
	if (status == statusDone) {
		m_updating = false;
		spawnLatest();
	} else if (status == statusFailed) {
		m_updating = false;
		failed(info.value(QStringLiteral("error")).toString(), info.value(QStringLiteral("error_message")).toString());
	}
}

void Updates::failed(const QString &error, const QString &message)
{
	const QVariantMap texts = Core::instance()->updateTexts(QString(), true);
	if (error == QLatin1String("org.freedesktop.DBus.Error.NotSupported"))
		alert(text(texts, "title"), text(texts, "newPermissions"));
	else
		alert(text(texts, "title"), message.isEmpty() ? text(texts, "failed") : text(texts, "failed") + QLatin1Char(' ') + message);
}

// Starts the newest installed build, which takes the app's name on the
// session bus, and quits.
void Updates::spawnLatest()
{
	qDBusRegisterMetaType<QList<QByteArray>>();
	qDBusRegisterMetaType<Fds>();
	qDBusRegisterMetaType<Envs>();
	QDBusMessage spawn = QDBusMessage::createMethodCall(portalName, portalPath, portalInterface, QStringLiteral("Spawn"));
	QByteArray cwd = QDir::currentPath().toLocal8Bit();
	cwd.append('\0');
	QList<QByteArray> argv{ QByteArray("pinesy-kde", sizeof("pinesy-kde")) };
	Envs envs{ { QString::fromLatin1(replaceVariable), QStringLiteral("1") } };
	spawn << cwd << QVariant::fromValue(argv) << QVariant::fromValue(Fds()) << QVariant::fromValue(envs)
		  << spawnLatestVersion << QVariantMap();
	auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(spawn), this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
		call->deleteLater();
		QDBusPendingReply<uint> reply = *call;
		if (reply.isError()) {
			failed(QString(), reply.error().message());
			return;
		}
		Q_EMIT quitRequested();
	});
}

//------------------------------------------------------------
//  Debian package: the track's apt index
//------------------------------------------------------------

void Updates::startDeb()
{
	const QString url = Core::instance()->aptPackagesUrl();
	if (url.isEmpty())
		return;
	QNetworkReply *reply = m_network.get(QNetworkRequest(QUrl(url)));
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError)
			return;
		const QByteArray packages = reply->readAll();
		char *newest = pinesy_core_apt_newest_version(packages.constData(), "pinesy-kde");
		const QString version = QString::fromUtf8(newest ? newest : "");
		pinesy_core_free(newest);
		notice(version, false);
	});
}

//------------------------------------------------------------
//  The notice
//------------------------------------------------------------

void Updates::notice(const QString &build, bool newerKnown)
{
	Core *core = Core::instance();
	if (!core->updateNotice(build, newerKnown))
		return;
	const QVariantMap texts = core->updateTexts(build, newerKnown);

	auto *box = new QMessageBox(QMessageBox::Information, text(texts, "title"), text(texts, "title"));
	box->setInformativeText(text(texts, "message"));
	box->setAttribute(Qt::WA_DeleteOnClose);
	QPushButton *never = box->addButton(text(texts, "neverRemind"), QMessageBox::RejectRole);
	QPushButton *ignore = box->addButton(text(texts, "ignoreBuild"), QMessageBox::NoRole);
	QPushButton *update = box->addButton(text(texts, "update"), QMessageBox::AcceptRole);
	box->setDefaultButton(update);
	connect(box, &QMessageBox::finished, this, [this, box, never, ignore, update, build, texts] {
		QAbstractButton *clicked = box->clickedButton();
		int choice;
		if (clicked == never)
			choice = PINESY_CORE_UPDATE_NEVER_REMIND;
		else if (clicked == ignore)
			choice = PINESY_CORE_UPDATE_IGNORE_BUILD;
		else if (clicked == update)
			choice = PINESY_CORE_UPDATE_NOW;
		else
			return;
		Core::instance()->updateChoice(choice, build);
		if (choice != PINESY_CORE_UPDATE_NOW)
			return;
		if (m_kind == QLatin1String("flatpak"))
			updateFlatpak();
		else
			alert(text(texts, "title"), text(texts, "packageManager"));
	});
	box->open();
}

void Updates::alert(const QString &title, const QString &text)
{
	auto *box = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::Close);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->open();
}
