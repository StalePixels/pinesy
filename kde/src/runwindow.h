// The window a machine runs in, and the pinesy_host that ties it to
// MAME: frames, the size MAME asks for, focus, MAME's error lines and
// the event pump. Closing the window asks MAME to exit; the window
// stays until MAME has.

#pragma once

#include "pinesy.h"

#include <QQuickWindow>
#include <QStringList>

class MachineView;

class RunWindow : public QQuickWindow
{
	Q_OBJECT

public:
	RunWindow();

	// The host for pinesy_run(), valid while the window is.
	const pinesy_host *host() const { return &m_host; }

	// MAME's error lines from the run.
	QStringList errors() const { return m_errors; }

	// Call before pinesy_run(): MAME takes the view's size from here.
	void prepare();

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	static void onFrame(void *context, const void *pixels, int width, int height, int pitch);
	static void onOutput(void *context, int channel, const char *text);
	static void onSize(void *context, int width, int height);
	static void onPump(void *context);

	void resizeToFit(int width, int height);

	MachineView *m_view = nullptr;
	pinesy_host m_host = {};
	QStringList m_errors;
	bool m_sized = false;
};
