// Runs one machine at a time, in its own window. MAME keeps its own
// loop inside pinesy_run() and pumps the app's events while it runs.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

class RunWindow;

class MachineRunner : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	// The title of the running machine's window; empty when none runs.
	Q_PROPERTY(QString running READ running NOTIFY runningChanged)

public:
	static MachineRunner *instance();
	static MachineRunner *create(QQmlEngine *, QJSEngine *);

	QString running() const { return m_running; }

	// Starts a configuration's command line. The core adds the fixed
	// options and, with debug, the debugger options from the settings.
	// Returns false when a machine is already running.
	bool start(const QString &title, const QStringList &arguments, bool debug);

	// Asks the running machine to stop. Its window closes when it has.
	Q_INVOKABLE void stop();

	// Resets the running machine. A hard reset starts it again from
	// power-on.
	Q_INVOKABLE void reset(bool hard);

	// Asks the running machine to exit and quits the app when it has.
	// Returns false when no machine runs.
	Q_INVOKABLE bool quitWhenStopped();

Q_SIGNALS:
	void runningChanged();
	// A run ended. errors is MAME's error text when it stopped with an
	// error, else empty.
	void finished(const QString &title, int code, const QString &errors);

private:
	explicit MachineRunner(QObject *parent = nullptr);
	void run(const QStringList &arguments, bool debug);

	QString m_running;
	RunWindow *m_window = nullptr;
	bool m_quitAfterRun = false;
};
