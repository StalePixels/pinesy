#include "document.h"

#include "core.h"
#include "runner.h"

#include <KLocalizedString>

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSaveFile>

ConfigurationDocument::ConfigurationDocument(QObject *parent)
	: QObject(parent)
{
	MachineRunner *runner = MachineRunner::instance();
	connect(runner, &MachineRunner::runningChanged, this, [this, runner] {
		Q_EMIT canRunChanged();
		// pinesy_describe() must not run inside a running MAME
		if (runner->running().isEmpty() && m_waiting)
			refresh();
	});
	connect(runner, &MachineRunner::finished, this, [this](const QString &title, int code, const QString &errors) {
		if (!m_runningThis)
			return;
		m_runningThis = false;
		if (code != 0)
			Q_EMIT runFailed(title, errors.isEmpty() ? i18n("The emulator exited with code %1.", code) : errors);
	});
	connect(this, &ConfigurationDocument::urlChanged, this, &ConfigurationDocument::titleChanged);
	connect(this, &ConfigurationDocument::summaryChanged, this, &ConfigurationDocument::titleChanged);
	connect(this, &ConfigurationDocument::summaryChanged, this, &ConfigurationDocument::canRunChanged);
}

QString ConfigurationDocument::title() const
{
	if (m_url.isValid())
		return QFileInfo(m_url.toLocalFile()).completeBaseName();
	const QString name = m_summary.value(QStringLiteral("machine")).toMap().value(QStringLiteral("name")).toString();
	return name.isEmpty() ? i18n("Untitled") : name;
}

bool ConfigurationDocument::canRun() const
{
	return MachineRunner::instance()->running().isEmpty() && m_summary.value(QStringLiteral("canRun")).toBool();
}

QString ConfigurationDocument::commandLine() const
{
	if (m_arguments.isEmpty())
		return {};
	return Core::instance()->copyCommandLine(m_arguments, false);
}

void ConfigurationDocument::setArguments(const QStringList &arguments)
{
	if (arguments == m_arguments)
		return;
	m_arguments = arguments;
	setModified(true);
	Q_EMIT changed();
	refresh();
}

void ConfigurationDocument::setModified(bool modified)
{
	if (modified == m_modified)
		return;
	m_modified = modified;
	Q_EMIT modifiedChanged();
}

void ConfigurationDocument::refresh()
{
	if (m_arguments.isEmpty()) {
		m_summary.clear();
		m_waiting = false;
		Q_EMIT summaryChanged();
		return;
	}
	if (!MachineRunner::instance()->running().isEmpty()) {
		m_waiting = true;
		Q_EMIT summaryChanged();
		return;
	}
	m_waiting = false;
	m_summary = Core::instance()->summary(m_arguments, m_machine);
	const QString machine = m_summary.value(QStringLiteral("machine")).toMap().value(QStringLiteral("id")).toString();
	if (!machine.isEmpty() && machine != m_machine) {
		m_machine = machine;
		Q_EMIT changed();
	}
	Q_EMIT summaryChanged();
}

void ConfigurationDocument::choose(const QString &machine)
{
	const QStringList arguments = Core::instance()->newConfiguration(machine);
	if (arguments.isEmpty())
		return;
	m_machine = machine;
	setArguments(arguments);
}

void ConfigurationDocument::setDriver(const QString &driver)
{
	setArguments(Core::instance()->setDriver(m_arguments, driver));
}

void ConfigurationDocument::setRom(const QString &driver, const QVariant &bios)
{
	setArguments(Core::instance()->setRom(m_arguments, driver, bios));
}

void ConfigurationDocument::setSlot(const QString &slot, const QString &card)
{
	setArguments(Core::instance()->setSlot(m_arguments, slot, card));
}

void ConfigurationDocument::setCardRom(const QString &slot, const QString &bios)
{
	setArguments(Core::instance()->setCardRom(m_arguments, slot, bios));
}

QString ConfigurationDocument::setMedia(const QString &media, const QString &path)
{
	if (!path.isEmpty())
		for (const QVariant &row : m_summary.value(QStringLiteral("media")).toList())
		{
			const QVariantMap device = row.toMap().value(QStringLiteral("media")).toMap();
			if (device.value(QStringLiteral("name")).toString() != media
					|| device.value(QStringLiteral("interface")).toString() != QStringLiteral("sdcard"))
				continue;
			if (QString problem = Core::sdCardProblem(path); !problem.isEmpty())
				return problem;
		}
	setArguments(Core::instance()->setMedia(m_arguments, media, path));
	return {};
}

QString ConfigurationDocument::load(const QUrl &url)
{
	QFile file(url.toLocalFile());
	if (!file.open(QIODevice::ReadOnly))
		return file.errorString();
	QString machine;
	QStringList arguments;
	if (!Core::readDocument(file.readAll(), &machine, &arguments))
		return i18n("This file is not a Pinesy configuration, or it is from a newer version of Pinesy.");
	m_machine = machine;
	m_arguments = arguments;
	m_url = url;
	setModified(false);
	Q_EMIT changed();
	Q_EMIT urlChanged();
	refresh();
	return {};
}

QString ConfigurationDocument::save(const QUrl &url)
{
	QSaveFile file(url.toLocalFile());
	if (!file.open(QIODevice::WriteOnly))
		return file.errorString();
	file.write(Core::writeDocument(m_machine, m_arguments));
	if (!file.commit())
		return file.errorString();
	if (url != m_url) {
		m_url = url;
		Q_EMIT urlChanged();
	}
	setModified(false);
	return {};
}

void ConfigurationDocument::run(bool debug)
{
	if (!canRun())
		return;
	m_runningThis = MachineRunner::instance()->start(title(), m_arguments, debug);
}

void ConfigurationDocument::copyCommandLine()
{
	if (!m_arguments.isEmpty())
		QGuiApplication::clipboard()->setText(commandLine());
}
