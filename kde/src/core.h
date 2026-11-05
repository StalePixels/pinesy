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
	Q_PROPERTY(QString userRomFolder READ userRomFolder NOTIFY settingsChanged)
	Q_PROPERTY(QString debugger READ debugger WRITE setDebugger NOTIFY settingsChanged)
	Q_PROPERTY(int gdbPort READ gdbPort WRITE setGdbPort NOTIFY settingsChanged)
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
	QString copyCommandLine(const QStringList &arguments, bool debug);

	// .pinesy files: an empty machine and arguments when the text is not
	// a configuration the core reads.
	static bool readDocument(const QByteArray &text, QString *machine, QStringList *arguments);
	static QByteArray writeDocument(const QString &machine, const QStringList &arguments);

	QStringList romFolders() const { return m_romFolders; }
	QString dataFolderSetting() const { return m_dataFolder; }
	QString dataFolder() const;
	QString userRomFolder() const;
	QString debugger() const;
	int gdbPort() const;
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
};
