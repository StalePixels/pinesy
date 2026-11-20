// The window a machine runs in, and the pinesy_host that ties it to
// MAME: frames, the frame's size, focus, MAME's error lines, the
// pointer, the event pump, and the Continue button, shown while the
// Next is stopped in the debugger or the Pi waits. Closing the window
// asks MAME to exit; the window stays until MAME has.

#pragma once

#include "pinesy_core.h"

#include <QQuickWindow>
#include <QStringList>

class MachineView;
class QQmlEngine;
class QQuickItem;

class RunWindow : public QQuickWindow
{
	Q_OBJECT

public:
	// windowSize is the configuration's window size, as
	// pinesy_core_window_size() takes it.
	explicit RunWindow(int windowSize = PINESY_CORE_WINDOW_FIT);

	// The host for pinesy_run(), valid while the window is.
	const pinesy_host *host() const { return &m_host; }

	// MAME's error lines from the run.
	QStringList errors() const { return m_errors; }

	// Call before pinesy_run(): MAME takes the view's size from here.
	void prepare();

Q_SIGNALS:
	// MAME holds the pointer in the view (true), or lets it go.
	void pointerHeldChanged(bool held);

protected:
	void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
	void continueMachine();

private:
	static void onFrame(void *context, const void *pixels, int width, int height, int pitch);
	static void onOutput(void *context, int channel, const char *text);
	static void onSize(void *context, int width, int height);
	static void onPump(void *context);
	static void onPointer(void *context, int hide, int capture);
	static void onStopped(void *context, int stopped);

	void resizeForFrame(int frameWidth, int frameHeight);

	MachineView *m_view = nullptr;
	QQmlEngine *m_qml = nullptr;
	QQuickItem *m_continue = nullptr;
	int m_windowSize = PINESY_CORE_WINDOW_FIT;
	pinesy_host m_host = {};
	QStringList m_errors;
	bool m_sized = false;
	// the last frame size MAME gave, to size the window again when it
	// leaves full screen
	QSize m_frame;
	bool m_fullScreen = false;
};
