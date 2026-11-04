// A configuration document: a machine and its command line, read from
// and saved to a .pinesy file. What the configuration page shows is the
// core's summary of the command line, asked again after every change.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

class ConfigurationDocument : public QObject
{
	Q_OBJECT
	QML_ELEMENT

	Q_PROPERTY(QString machine READ machine NOTIFY changed)
	Q_PROPERTY(QStringList arguments READ arguments NOTIFY changed)
	Q_PROPERTY(bool empty READ empty NOTIFY changed)
	Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
	Q_PROPERTY(QString title READ title NOTIFY titleChanged)
	Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
	// The core's summary; empty until the document has a machine. It is
	// not asked for while a machine runs.
	Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
	// The command line changed while a machine ran; the summary comes
	// when the run ends.
	Q_PROPERTY(bool waiting READ waiting NOTIFY summaryChanged)
	Q_PROPERTY(bool canRun READ canRun NOTIFY canRunChanged)
	// The headless command, as Copy Command Line gives it.
	Q_PROPERTY(QString commandLine READ commandLine NOTIFY changed)

public:
	explicit ConfigurationDocument(QObject *parent = nullptr);

	QString machine() const { return m_machine; }
	QStringList arguments() const { return m_arguments; }
	bool empty() const { return m_arguments.isEmpty(); }
	QUrl url() const { return m_url; }
	QString title() const;
	bool modified() const { return m_modified; }
	QVariantMap summary() const { return m_summary; }
	bool waiting() const { return m_waiting; }
	bool canRun() const;
	QString commandLine() const;

	// Starts a new configuration for a machine of the catalogue.
	Q_INVOKABLE void choose(const QString &machine);

	Q_INVOKABLE void setDriver(const QString &driver);
	Q_INVOKABLE void setRom(const QString &driver, const QVariant &bios);
	Q_INVOKABLE void setSlot(const QString &slot, const QString &card);
	Q_INVOKABLE void setCardRom(const QString &slot, const QString &bios);
	// An empty path empties the media device.
	Q_INVOKABLE void setMedia(const QString &media, const QString &path);

	// Read and write a .pinesy file. On failure they return the reason.
	Q_INVOKABLE QString load(const QUrl &url);
	Q_INVOKABLE QString save(const QUrl &url);

	Q_INVOKABLE void run(bool debug);
	Q_INVOKABLE void copyCommandLine();

Q_SIGNALS:
	void changed();
	void urlChanged();
	void titleChanged();
	void modifiedChanged();
	void summaryChanged();
	void canRunChanged();
	// This document's run stopped with an error.
	void runFailed(const QString &title, const QString &message);

private:
	void setArguments(const QStringList &arguments);
	void setModified(bool modified);
	void refresh();

	QString m_machine;
	QStringList m_arguments;
	QUrl m_url;
	bool m_modified = false;
	QVariantMap m_summary;
	bool m_waiting = false;
	bool m_runningThis = false;
};
