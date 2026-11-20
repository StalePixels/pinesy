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
	// The running machine holds the pointer in its view.
	Q_PROPERTY(bool pointerHeld READ pointerHeld NOTIFY pointerHeldChanged)

public:
	static MachineRunner *instance();
	static MachineRunner *create(QQmlEngine *, QJSEngine *);

	QString running() const { return m_running; }
	bool pointerHeld() const { return m_pointerHeld; }

	// Starts a configuration's command line. The core adds the fixed
	// options and, with debug, the debugger options from the settings.
	// windowSize is the configuration's window size. takesMouse is the
	// summary's mouse and mouse the configuration's mouse setting.
	// piPaused is the configuration's Pi pause setting. Returns false
	// when a machine is already running.
	bool start(const QString &title, const QStringList &arguments, bool debug, int windowSize, bool takesMouse,
			bool mouse, bool piPaused);

	// Asks the running machine to stop. Its window closes when it has.
	Q_INVOKABLE void stop();

	// Resets the running machine. A hard reset starts it again from
	// power-on.
	Q_INVOKABLE void reset(bool hard);

	// Releases a held pointer, or grabs it again.
	Q_INVOKABLE void togglePointer();

	// Asks the running machine to exit and quits the app when it has.
	// Returns false when no machine runs.
	Q_INVOKABLE bool quitWhenStopped();

Q_SIGNALS:
	void runningChanged();
	void pointerHeldChanged();
	// A run ended. errors is MAME's error text when it stopped with an
	// error, else empty.
	void finished(const QString &title, int code, const QString &errors);

private:
	explicit MachineRunner(QObject *parent = nullptr);
	void run(const QStringList &arguments, bool debug, bool takesMouse, bool mouse, bool piPaused);
	void setPointerHeld(bool held);

	QString m_running;
	bool m_pointerHeld = false;
	RunWindow *m_window = nullptr;
	bool m_quitAfterRun = false;
};
