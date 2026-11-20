#include "runwindow.h"

#include "machineview.h"

#include <KLocalizedString>

#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScreen>

RunWindow::RunWindow(int windowSize)
	: m_windowSize(windowSize)
{
	// the window takes its size from the first frame
	resize(640, 480);
	setColor(Qt::black);

	m_view = new MachineView(contentItem());
	m_view->setSize(size());
	connect(this, &QWindow::widthChanged, this, [this](int w) { m_view->setWidth(w); });
	connect(this, &QWindow::heightChanged, this, [this](int h) { m_view->setHeight(h); });
	connect(this, &QWindow::activeChanged, this, [this] { pinesy_focus(isActive() ? 1 : 0); });
	connect(this, &QWindow::windowStateChanged, this, [this](Qt::WindowState state) {
		const bool left = m_fullScreen && state != Qt::WindowFullScreen;
		m_fullScreen = state == Qt::WindowFullScreen;
		if (left && m_frame.isValid())
			resizeForFrame(m_frame.width(), m_frame.height());
	});

	m_host.view = this;
	m_host.frame = onFrame;
	m_host.output = onOutput;
	m_host.size = onSize;
	m_host.pump = onPump;
	m_host.pointer = onPointer;
	m_host.stopped = onStopped;
	m_host.context = this;

	// the Continue button, at the view's top right, over the picture
	m_qml = new QQmlEngine(this);
	QQmlComponent button(m_qml);
	button.setData(QByteArrayLiteral("import QtQuick\n"
									 "import QtQuick.Controls\n"
									 "Button {\n"
									 "    property string tip\n"
									 "    icon.name: \"media-playback-start\"\n"
									 "    ToolTip.visible: hovered\n"
									 "    ToolTip.text: tip\n"
									 "    visible: false\n"
									 "    anchors.top: parent ? parent.top : undefined\n"
									 "    anchors.right: parent ? parent.right : undefined\n"
									 "    anchors.margins: 8\n"
									 "}\n"),
			QUrl());
	m_continue = qobject_cast<QQuickItem *>(button.create());
	if (m_continue) {
		m_continue->setParent(this);
		m_continue->setParentItem(contentItem());
		m_continue->setZ(1);
		m_continue->setProperty("text", i18n("Continue"));
		m_continue->setProperty("tip", i18n("Continue the stopped Next or Pi"));
		connect(m_continue, SIGNAL(clicked()), this, SLOT(continueMachine()));
	}
}

void RunWindow::continueMachine()
{
	pinesy_continue();
}

void RunWindow::onStopped(void *context, int stopped)
{
	auto self = static_cast<RunWindow *>(context);
	if (self->m_continue)
		self->m_continue->setVisible(stopped != 0);
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
	static_cast<RunWindow *>(context)->resizeForFrame(width, height);
}

void RunWindow::onPump(void *)
{
	QCoreApplication::processEvents();
}

// Qt has no call that holds the pointer in a window, so capture is not
// done: the pointer is hidden over the view, and the machine gets the
// mouse's movement while the pointer is over the view or a drag that
// started there goes on. MAME's request to capture still counts as the
// pointer held, for the Grab/Release Pointer item.
void RunWindow::onPointer(void *context, int hide, int capture)
{
	auto self = static_cast<RunWindow *>(context);
	if (hide)
		self->m_view->setCursor(Qt::BlankCursor);
	else
		self->m_view->unsetCursor();
	Q_EMIT self->pointerHeldChanged(capture != 0);
}

// Takes the size the core gives for a frame of frameWidth x frameHeight
// pixels, the window size and the screen. The window starts centred at
// its first size, and goes to full screen from there when the window
// size says so. A window in full screen keeps the screen's size until
// it leaves full screen.
void RunWindow::resizeForFrame(int frameWidth, int frameHeight)
{
	m_frame = QSize(frameWidth, frameHeight);
	if (windowState() == Qt::WindowFullScreen)
		return;
	QSize usable;
	if (QScreen *s = screen()) {
		const QMargins frame = frameMargins();
		usable = s->availableGeometry().size().shrunkBy(frame);
	}
	int width = 0, height = 0;
	if (pinesy_core_window_size(frameWidth, frameHeight, m_windowSize, devicePixelRatio(),
			usable.width(), usable.height(), &width, &height) < 0)
		return;
	const QSize wanted(width, height);
	resize(wanted);
	if (!m_sized) {
		m_sized = true;
		if (QScreen *s = screen())
			setPosition(s->availableGeometry().center() - QPoint(wanted.width() / 2, wanted.height() / 2));
		if (pinesy_core_window_full_screen(m_windowSize))
			showFullScreen();
	}
}
