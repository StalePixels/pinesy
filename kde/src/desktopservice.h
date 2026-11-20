// org.freedesktop.Application on the session bus, under the app id. A
// desktop that starts the app over D-Bus (DBusActivatable in the
// .desktop file), such as a file manager opening a .pinesy file, calls
// it to activate the app and to hand it files. The files never come on
// the command line, where any argument starts headless mode.

#pragma once

#include <QDBusAbstractAdaptor>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class DesktopService : public QObject
{
	Q_OBJECT

public:
	explicit DesktopService(QObject *parent = nullptr);

	// Takes the app id's name on the session bus. False when the bus is
	// not reachable or another instance has the name; the app then runs
	// without it.
	bool registerOnBus();

Q_SIGNALS:
	void activateRequested();
	void openRequested(const QList<QUrl> &urls);
};

class DesktopServiceAdaptor : public QDBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")

public:
	explicit DesktopServiceAdaptor(DesktopService *service);

public Q_SLOTS:
	void Activate(const QVariantMap &platformData);
	void Open(const QStringList &uris, const QVariantMap &platformData);
	void ActivateAction(const QString &action, const QVariantList &parameter, const QVariantMap &platformData);

private:
	DesktopService *m_service;
};
