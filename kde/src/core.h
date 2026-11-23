// The core for the UI: libpinesy's calls with Qt types, and the
// settings, stored with KConfig and given to the core.

#pragma once

#include "pinesy_core.h"

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariant>

class Core : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(QVariantList catalogue READ catalogue CONSTANT)
	Q_PROPERTY(QStringList romFolders READ romFolders NOTIFY settingsChanged)
	Q_PROPERTY(QString dataFolderSetting READ dataFolderSetting NOTIFY settingsChanged)
	Q_PROPERTY(QString dataFolder READ dataFolder NOTIFY settingsChanged)
	Q_PROPERTY(QString debugger READ debugger WRITE setDebugger NOTIFY settingsChanged)
	Q_PROPERTY(int gdbPort READ gdbPort WRITE setGdbPort NOTIFY settingsChanged)
	Q_PROPERTY(int piGdbPort READ piGdbPort WRITE setPiGdbPort NOTIFY settingsChanged)
	Q_PROPERTY(bool startPaused READ startPaused WRITE setStartPaused NOTIFY settingsChanged)
	Q_PROPERTY(int minGdbPort READ minGdbPort CONSTANT)
	Q_PROPERTY(int maxGdbPort READ maxGdbPort CONSTANT)

public:
	// The core for this process. resourcesFolder and defaultDataFolder
	// are the platform's part, as pinesy_core_create() takes them.
	static void createInstance(const QString &resourcesFolder, const QString &defaultDataFolder);
	static void destroyInstance();
	static Core *instance();
	static Core *create(QQmlEngine *, QJSEngine *);

	pinesy_core *handle() const { return m_core; }

	QVariantList catalogue();
	Q_INVOKABLE QVariantList search(const QString &text);

	Q_INVOKABLE QStringList newConfiguration(const QString &machine);
	QStringList setDriver(const QStringList &arguments, const QString &driver);
	QStringList setRom(const QStringList &arguments, const QString &driver, const QVariant &bios);
	QStringList setSlot(const QStringList &arguments, const QString &slot, const QString &card);
	QStringList setCardRom(const QStringList &arguments, const QString &slot, const QString &bios);
	QStringList setMedia(const QStringList &arguments, const QString &media, const QString &path);
	// Why MAME's SD card cannot use the image at path; empty when it can.
	static QString sdCardProblem(const QString &path);
	QVariantMap summary(const QStringList &arguments, const QString &machineHint);
	// takesMouse is the summary's mouse and mouse the configuration's
	// mouse setting; the core turns them into the mouse option.
	QString copyCommandLine(const QStringList &arguments, bool debug, bool takesMouse, bool mouse);

	// .pinesy files. readDocument returns false when the text is not a
	// configuration the core reads. windowSize is the machine window's
	// size, as pinesy_core_window_size() takes it; mouse is the mouse
	// setting; piPaused the Pi pause setting.
	static bool readDocument(const QByteArray &text, QString *machine, QStringList *arguments, int *windowSize,
			bool *mouse, bool *piPaused);
	static QByteArray writeDocument(const QString &machine, const QStringList &arguments, int windowSize, bool mouse,
			bool piPaused);

	QStringList romFolders() const { return m_romFolders; }
	QString dataFolderSetting() const { return m_dataFolder; }
	QString dataFolder() const;
	QString debugger() const;
	int gdbPort() const;
	int piGdbPort() const { return m_piGdbPort; }
	bool startPaused() const { return m_startPaused; }
	int minGdbPort() const { return PINESY_CORE_MIN_GDB_PORT; }
	int maxGdbPort() const { return PINESY_CORE_MAX_GDB_PORT; }

	// True when the folder was added; false when the core says it cannot
	// be a ROM folder.
	Q_INVOKABLE bool addRomFolder(const QString &path);
	Q_INVOKABLE void removeRomFolder(const QString &path);
	// "" for the default.
	Q_INVOKABLE void setDataFolder(const QString &path);
	void setDebugger(const QString &debugger);
	void setGdbPort(int port);
	void setPiGdbPort(int port);
	void setStartPaused(bool paused);

	// Updates (the core's rules): true when this CI build looks for
	// updates; true when the out-of-date notice shows for a build found
	// on the server (a version, or with newerKnown a Flatpak remote
	// commit); the notice's texts (pinesy_core_update_texts());
	// and a choice of the notice (PINESY_CORE_UPDATE_*), stored.
	bool updateChecks() const;
	bool updateNotice(const QString &build, bool newerKnown) const;
	QVariantMap updateTexts(const QString &build, bool newerKnown) const;
	// The build's kind ("flatpak", "deb", ...), empty for a local build,
	// the track's apt index for a "deb" build, and the app's ref file in
	// the track's Flatpak repository for a "flatpak" build, from the text
	// of PINESY_CORE_FLATPAK_INFO.
	QString buildKind() const;
	QString aptPackagesUrl() const;
	QString flatpakRefUrl(const QByteArray &flatpakInfo) const;
	void updateChoice(int choice, const QString &build);

	// Turns a file URL from a dialog into a local path.
	Q_INVOKABLE static QString localPath(const QUrl &url);

	static QByteArray toJson(const QStringList &list);
	static QStringList fromJsonList(const char *json);

Q_SIGNALS:
	void settingsChanged();

private:
	Core(const QString &resourcesFolder, const QString &defaultDataFolder);
	~Core() override;

	void loadSettings();
	void storeSettings();
	void applySettings();
	QStringList edited(char *result, const QStringList &unchanged);

	pinesy_core *m_core = nullptr;
	QVariantList m_catalogue;
	bool m_catalogueRead = false;
	QStringList m_romFolders;
	QString m_dataFolder;
	QString m_debugger;
	int m_gdbPort = PINESY_CORE_DEFAULT_GDB_PORT;
	int m_piGdbPort = PINESY_CORE_DEFAULT_PI_GDB_PORT;
	bool m_updateChecks = true;
	QString m_ignoredBuild;
	bool m_startPaused = false;
};
