#include "core.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {

Core *s_instance = nullptr;

// Takes text the core returned and frees it.
QByteArray take(char *text)
{
	if (!text)
		return {};
	QByteArray result(text);
	pinesy_core_free(text);
	return result;
}

QVariant parse(char *text)
{
	const QByteArray json = take(text);
	if (json.isEmpty())
		return {};
	return QJsonDocument::fromJson(json).toVariant();
}

KConfigGroup settingsGroup(const char *name)
{
	return KSharedConfig::openConfig(QStringLiteral("pinesyrc"))->group(QString::fromLatin1(name));
}

} // namespace

Core::Core(const QString &resourcesFolder, const QString &defaultDataFolder)
	: m_core(pinesy_core_create(resourcesFolder.toUtf8().constData(), defaultDataFolder.toUtf8().constData()))
{
	loadSettings();
	applySettings();
}

Core::~Core()
{
	pinesy_core_destroy(m_core);
}

void Core::createInstance(const QString &resourcesFolder, const QString &defaultDataFolder)
{
	if (!s_instance)
		s_instance = new Core(resourcesFolder, defaultDataFolder);
}

void Core::destroyInstance()
{
	delete s_instance;
	s_instance = nullptr;
}

Core *Core::instance()
{
	return s_instance;
}

Core *Core::create(QQmlEngine *, QJSEngine *)
{
	QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
	return s_instance;
}

QByteArray Core::toJson(const QStringList &list)
{
	return QJsonDocument(QJsonArray::fromStringList(list)).toJson(QJsonDocument::Compact);
}

QStringList Core::fromJsonList(const char *json)
{
	QStringList result;
	for (const QJsonValue &value : QJsonDocument::fromJson(json).array())
		result.append(value.toString());
	return result;
}

QString Core::localPath(const QUrl &url)
{
	return url.toLocalFile();
}

//------------------------------------------------------------
//  The catalogue and configurations
//------------------------------------------------------------

QVariantList Core::catalogue()
{
	if (!m_catalogueRead) {
		m_catalogue = parse(pinesy_core_catalogue(m_core)).toList();
		m_catalogueRead = true;
	}
	return m_catalogue;
}

QVariantList Core::search(const QString &text)
{
	return parse(pinesy_core_search(m_core, text.toUtf8().constData())).toList();
}

QStringList Core::newConfiguration(const QString &machine)
{
	const QByteArray json = take(pinesy_core_new_configuration(m_core, machine.toUtf8().constData()));
	return json.isEmpty() ? QStringList() : fromJsonList(json.constData());
}

// An edit's result: the new command line, or the old one when the core
// returned none (an unknown slot).
QStringList Core::edited(char *result, const QStringList &unchanged)
{
	const QByteArray json = take(result);
	return json.isEmpty() ? unchanged : fromJsonList(json.constData());
}

QStringList Core::setDriver(const QStringList &arguments, const QString &driver)
{
	return edited(pinesy_core_set_driver(m_core, toJson(arguments).constData(), driver.toUtf8().constData()), arguments);
}

QStringList Core::setRom(const QStringList &arguments, const QString &driver, const QVariant &bios)
{
	const QByteArray biosText = bios.isNull() ? QByteArray() : bios.toString().toUtf8();
	return edited(pinesy_core_set_rom(m_core, toJson(arguments).constData(), driver.toUtf8().constData(),
				bios.isNull() ? nullptr : biosText.constData()),
			arguments);
}

QStringList Core::setSlot(const QStringList &arguments, const QString &slot, const QString &card)
{
	return edited(pinesy_core_set_slot(m_core, toJson(arguments).constData(), slot.toUtf8().constData(),
				card.toUtf8().constData()),
			arguments);
}

QStringList Core::setCardRom(const QStringList &arguments, const QString &slot, const QString &bios)
{
	return edited(pinesy_core_set_card_rom(m_core, toJson(arguments).constData(), slot.toUtf8().constData(),
				bios.toUtf8().constData()),
			arguments);
}

QStringList Core::setMedia(const QStringList &arguments, const QString &media, const QString &path)
{
	const QByteArray pathText = path.toUtf8();
	return edited(pinesy_core_set_media(toJson(arguments).constData(), media.toUtf8().constData(),
				path.isEmpty() ? nullptr : pathText.constData()),
			arguments);
}

QString Core::sdCardProblem(const QString &path)
{
	return QString::fromUtf8(take(pinesy_core_sd_card_problem(path.toUtf8().constData())));
}

QVariantMap Core::summary(const QStringList &arguments, const QString &machineHint)
{
	const QByteArray hint = machineHint.toUtf8();
	return parse(pinesy_core_summary(m_core, toJson(arguments).constData(),
				machineHint.isEmpty() ? nullptr : hint.constData()))
		.toMap();
}

QString Core::copyCommandLine(const QStringList &arguments, bool debug)
{
	const QByteArray executable = QCoreApplication::applicationFilePath().toUtf8();
	return QString::fromUtf8(take(pinesy_core_copy_command_line(m_core, executable.constData(),
			toJson(arguments).constData(), debug ? 1 : 0)));
}

bool Core::readDocument(const QByteArray &text, QString *machine, QStringList *arguments)
{
	const QByteArray json = take(pinesy_core_read_document(text.constData()));
	if (json.isEmpty())
		return false;
	const QJsonObject object = QJsonDocument::fromJson(json).object();
	*machine = object.value(QLatin1String("machine")).toString();
	arguments->clear();
	for (const QJsonValue &value : object.value(QLatin1String("arguments")).toArray())
		arguments->append(value.toString());
	return true;
}

QByteArray Core::writeDocument(const QString &machine, const QStringList &arguments)
{
	return take(pinesy_core_write_document(machine.toUtf8().constData(), toJson(arguments).constData()));
}

//------------------------------------------------------------
//  Settings
//------------------------------------------------------------

void Core::loadSettings()
{
	const KConfigGroup general = settingsGroup("General");
	const KConfigGroup debugger = settingsGroup("Debugger");
	m_romFolders = general.readEntry("romFolders", QStringList());
	m_dataFolder = general.readEntry("dataFolder", QString());
	m_debugger = debugger.readEntry("debugger", QString());
	m_gdbPort = debugger.readEntry("gdbPort", int(PINESY_CORE_DEFAULT_GDB_PORT));
}

void Core::storeSettings()
{
	KConfigGroup general = settingsGroup("General");
	KConfigGroup debugger = settingsGroup("Debugger");
	general.writeEntry("romFolders", m_romFolders);
	general.writeEntry("dataFolder", m_dataFolder);
	debugger.writeEntry("debugger", m_debugger);
	debugger.writeEntry("gdbPort", m_gdbPort);
	general.sync();
}

// Gives the settings to the core, then takes back the values it uses,
// so a value the core does not accept shows as its default.
void Core::applySettings()
{
	QJsonObject settings;
	settings.insert(QLatin1String("romFolders"), QJsonArray::fromStringList(m_romFolders));
	settings.insert(QLatin1String("dataFolder"), m_dataFolder);
	settings.insert(QLatin1String("debugger"), m_debugger);
	settings.insert(QLatin1String("gdbPort"), m_gdbPort);
	pinesy_core_set_settings(m_core, QJsonDocument(settings).toJson(QJsonDocument::Compact).constData());

	const QJsonObject used = QJsonDocument::fromJson(take(pinesy_core_settings(m_core))).object();
	m_debugger = used.value(QLatin1String("debugger")).toString();
	m_gdbPort = used.value(QLatin1String("gdbPort")).toInt();
	Q_EMIT settingsChanged();
}

QString Core::dataFolder() const
{
	return QString::fromUtf8(take(pinesy_core_data_folder(m_core)));
}

QString Core::debugger() const
{
	return m_debugger;
}

int Core::gdbPort() const
{
	return m_gdbPort;
}

bool Core::addRomFolder(const QString &path)
{
	if (!pinesy_core_is_usable_rom_folder(path.toUtf8().constData()))
		return false;
	if (!m_romFolders.contains(path)) {
		m_romFolders.append(path);
		storeSettings();
		applySettings();
	}
	return true;
}

void Core::removeRomFolder(const QString &path)
{
	if (m_romFolders.removeAll(path) > 0) {
		storeSettings();
		applySettings();
	}
}

void Core::setDataFolder(const QString &path)
{
	if (path == m_dataFolder)
		return;
	m_dataFolder = path;
	storeSettings();
	applySettings();
}

void Core::setDebugger(const QString &debugger)
{
	if (debugger == m_debugger)
		return;
	m_debugger = debugger;
	storeSettings();
	applySettings();
}

void Core::setGdbPort(int port)
{
	if (port == m_gdbPort)
		return;
	m_gdbPort = port;
	storeSettings();
	applySettings();
}
