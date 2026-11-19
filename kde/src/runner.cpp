#include "runner.h"

#include "core.h"
#include "runwindow.h"

#include <QCoreApplication>
#include <QTimer>

MachineRunner::MachineRunner(QObject *parent)
	: QObject(parent)
{
}

MachineRunner *MachineRunner::instance()
{
	static MachineRunner *runner = new MachineRunner(QCoreApplication::instance());
	return runner;
}

MachineRunner *MachineRunner::create(QQmlEngine *, QJSEngine *)
{
	MachineRunner *runner = instance();
	QJSEngine::setObjectOwnership(runner, QJSEngine::CppOwnership);
	return runner;
}

bool MachineRunner::start(const QString &title, const QStringList &arguments, bool debug, int windowSize, bool mouse)
{
	if (!m_running.isEmpty() || arguments.isEmpty())
		return false;
	m_running = title;
	Q_EMIT runningChanged();

	m_window = new RunWindow(windowSize);
	m_window->setTitle(title);
	m_window->show();
	m_window->requestActivate();

	// pinesy_run() keeps its own loop; start it from the event loop, so the
	// action that started it returns first.
	QTimer::singleShot(0, this, [this, arguments, debug, mouse] { run(arguments, debug, mouse); });
	return true;
}

void MachineRunner::run(const QStringList &arguments, bool debug, bool mouse)
{
	m_window->prepare();
	const int code = pinesy_core_run(Core::instance()->handle(), Core::toJson(arguments).constData(),
			debug ? 1 : 0, mouse ? 1 : 0, m_window->host());

	const QString title = m_running;
	const QString errors = m_window->errors().join(QString()).trimmed();
	m_window->hide();
	m_window->deleteLater();
	m_window = nullptr;
	m_running.clear();
	Q_EMIT runningChanged();

	if (m_quitAfterRun) {
		QCoreApplication::quit();
		return;
	}
	Q_EMIT finished(title, code, code != 0 ? errors : QString());
}

void MachineRunner::stop()
{
	pinesy_request_exit();
}

void MachineRunner::reset(bool hard)
{
	pinesy_reset(hard ? 1 : 0);
}

bool MachineRunner::quitWhenStopped()
{
	if (m_running.isEmpty())
		return false;
	m_quitAfterRun = true;
	pinesy_request_exit();
	return true;
}
