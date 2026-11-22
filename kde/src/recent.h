// The recent configurations: the .pinesy files opened or saved last,
// newest first. The list is kept in the app's KConfig file, pinesyrc,
// in the group "Recent Files", with the keys KRecentFilesAction uses
// (File1, Name1, ...). The core decides which of them are shown and
// what a launch opens.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>

class RecentConfigurations : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	// The recent configurations to show, newest first: each a map with
	// url, path, name (without the extension) and folder.
	Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)

public:
	static RecentConfigurations *instance();
	static RecentConfigurations *create(QQmlEngine *, QJSEngine *);

	QVariantList entries() const;

	// Puts a configuration that was opened or saved first in the list.
	Q_INVOKABLE void note(const QUrl &url);
	Q_INVOKABLE void clear();

	// True when a launch with no file to open shows the recent
	// configurations; false when it opens a new configuration.
	Q_INVOKABLE bool launchShowsRecent() const;

Q_SIGNALS:
	void changed();

private:
	explicit RecentConfigurations(QObject *parent = nullptr);

	QStringList paths() const;
	void store(const QStringList &paths);
};
