// Updates of a CI build. A Flatpak build asks the Flatpak portal; a
// Debian package reads its track's apt index. The core decides when
// the out-of-date notice shows, and what it says.

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVariantMap>

// The variable in the environment of a new build that the Flatpak
// portal starts after an update. That build takes the app's name on the
// session bus from the running instance (DesktopService).
inline constexpr const char *replaceVariable = "PINESY_REPLACE";

class Updates : public QObject
{
	Q_OBJECT

public:
	explicit Updates(QObject *parent = nullptr);

	// Looks for a newer build, once, when the core says this build looks
	// for updates (a CI build with update checks on).
	void start();

Q_SIGNALS:
	// The new build has started; the app quits.
	void quitRequested();

private Q_SLOTS:
	void updateAvailable(const QVariantMap &info);
	void progress(const QVariantMap &info);

private:
	QString m_kind;
	QString m_monitor;   // the portal's UpdateMonitor object
	bool m_updating = false;
	QNetworkAccessManager m_network;

	void startFlatpak();
	void startDeb();
	void notice(const QString &build, bool newerKnown);
	void updateFlatpak();
	void failed(const QString &error, const QString &message);
	void spawnLatest();
	void alert(const QString &title, const QString &text);
};
