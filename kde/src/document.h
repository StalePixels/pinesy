// A configuration document: a machine and its command line, read from
// and saved to a .pinesy file. What the configuration page shows is the
// core's summary of the command line, asked again after every change.

#pragma once

#include "pinesy_core.h"

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
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
	Q_PROPERTY(QString commandLine READ commandLine NOTIFY commandLineChanged)
	// The mouse setting: turns the mouse of a machine that takes one
	// (the summary's mouse) on or off.
	Q_PROPERTY(bool mouse READ mouse WRITE setMouse NOTIFY mouseChanged)
	// The machine window's size, as pinesy_core_window_size() takes it:
	// full screen, fit, or a magnification of the frame, 1 or more.
	Q_PROPERTY(int windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged)
	// The window size picker: its choices in order, the one that shows
	// windowSize (pinesy_core_window_choice()), and the values of fit,
	// full screen and Custom among the choices. Every other choice is
	// a magnification.
	Q_PROPERTY(QVariantList windowChoices READ windowChoices CONSTANT)
	Q_PROPERTY(int windowChoice READ windowChoice NOTIFY windowSizeChanged)
	Q_PROPERTY(int windowFit READ windowFit CONSTANT)
	Q_PROPERTY(int windowFullScreen READ windowFullScreen CONSTANT)
	Q_PROPERTY(int windowCustom READ windowCustom CONSTANT)

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
	int windowSize() const { return m_windowSize; }
	void setWindowSize(int windowSize);
	bool mouse() const { return m_mouse; }
	void setMouse(bool mouse);
	QVariantList windowChoices() const;
	int windowChoice() const;
	int windowFit() const { return PINESY_CORE_WINDOW_FIT; }
	int windowFullScreen() const { return PINESY_CORE_WINDOW_FULL_SCREEN; }
	int windowCustom() const { return PINESY_CORE_WINDOW_CUSTOM; }

	// Takes a choice of the window size picker. Custom keeps a
	// magnification and shows the magnification field.
	Q_INVOKABLE void chooseWindowSize(int choice);
	// Takes the number entered in the magnification field.
	Q_INVOKABLE void setCustomWindowSize(int magnification);

	// Starts a new configuration for a machine of the catalogue.
	Q_INVOKABLE void choose(const QString &machine);

	Q_INVOKABLE void setDriver(const QString &driver);
	Q_INVOKABLE void setRom(const QString &driver, const QVariant &bios);
	Q_INVOKABLE void setSlot(const QString &slot, const QString &card);
	Q_INVOKABLE void setCardRom(const QString &slot, const QString &bios);
	// An empty path empties the media device.
	// Puts a file in a media device. An SD card image that MAME's SD card
	// cannot use is refused: the reason is returned, and nothing changes.
	Q_INVOKABLE QString setMedia(const QString &media, const QString &path);

	// Read and write a .pinesy file. On failure they return the reason.
	Q_INVOKABLE QString load(const QUrl &url);
	Q_INVOKABLE QString save(const QUrl &url);

	Q_INVOKABLE void run(bool debug);
	Q_INVOKABLE void copyCommandLine();
	// Deletes the NVRAM MAME saved for the configuration's driver, and
	// nothing else. False when a file could not be deleted.
	Q_INVOKABLE bool clearNvram();

Q_SIGNALS:
	void changed();
	void urlChanged();
	void titleChanged();
	void modifiedChanged();
	void summaryChanged();
	void canRunChanged();
	void windowSizeChanged();
	void mouseChanged();
	void commandLineChanged();
	// This document's run stopped with an error.
	void runFailed(const QString &title, const QString &message);

private:
	void setArguments(const QStringList &arguments);
	void setModified(bool modified);
	void refresh();
	// The machine takes a mouse, so the mouse setting applies.
	bool takesMouse() const;

	QString m_machine;
	QStringList m_arguments;
	int m_windowSize = PINESY_CORE_WINDOW_FIT;
	// Custom was chosen for the window size, so the magnification field
	// stays while the user enters a listed magnification in it.
	bool m_customWindowSize = false;
	bool m_mouse = true;
	QUrl m_url;
	bool m_modified = false;
	QVariantMap m_summary;
	bool m_waiting = false;
	bool m_runningThis = false;
};
