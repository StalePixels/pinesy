#include "runwindow.h"

#include "machineview.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QScreen>

RunWindow::RunWindow()
{
	// MAME sets the size and shape once it has the view
	resize(640, 480);
	setColor(Qt::black);

	m_view = new MachineView(contentItem());
	m_view->setSize(size());
	connect(this, &QWindow::widthChanged, this, [this](int w) { m_view->setWidth(w); });
	connect(this, &QWindow::heightChanged, this, [this](int h) { m_view->setHeight(h); });
	connect(this, &QWindow::activeChanged, this, [this] { pinesy_focus(isActive() ? 1 : 0); });

	m_host.view = this;
	m_host.frame = onFrame;
	m_host.output = onOutput;
	m_host.size = onSize;
	m_host.pump = onPump;
	m_host.context = this;
}

void RunWindow::prepare()
{
	m_view->forceActiveFocus();
	m_view->sendViewSize();
}

void RunWindow::closeEvent(QCloseEvent *event)
{
	// the window goes when MAME has exited
	event->ignore();
	pinesy_request_exit();
}

void RunWindow::onFrame(void *context, const void *pixels, int width, int height, int pitch)
{
	static_cast<RunWindow *>(context)->m_view->setFrame(pixels, width, height, pitch);
}

void RunWindow::onOutput(void *context, int channel, const char *text)
{
	// channel 0 is MAME's errors
	if (channel == 0 && text)
		static_cast<RunWindow *>(context)->m_errors.append(QString::fromUtf8(text));
}

void RunWindow::onSize(void *context, int width, int height)
{
	static_cast<RunWindow *>(context)->resizeToFit(width, height);
}

void RunWindow::onPump(void *)
{
	QCoreApplication::processEvents();
}

// Takes the size MAME asks for, scaled down to fit the screen. The first
// size is MAME's window size; the window starts centred at it.
void RunWindow::resizeToFit(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;
	QSize wanted(width, height);
	if (QScreen *s = screen()) {
		const QSize available = s->availableGeometry().size();
		const double scale = qMin(1.0, qMin(double(available.width()) / width, double(available.height()) / height));
		wanted = QSize(qRound(width * scale), qRound(height * scale));
	}
	resize(wanted);
	if (!m_sized) {
		m_sized = true;
		if (QScreen *s = screen())
			setPosition(s->availableGeometry().center() - QPoint(wanted.width() / 2, wanted.height() / 2));
	}
}
