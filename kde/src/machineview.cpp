#include "machineview.h"

#include "pinesy.h"

#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>

MachineView::MachineView(QQuickItem *parent)
	: QQuickItem(parent)
{
	setFlag(ItemHasContents);
	setFlag(ItemIsFocusScope);
	setActiveFocusOnTab(false);
	setFocus(true);
	setAcceptHoverEvents(true);
	setAcceptedMouseButtons(Qt::AllButtons);
}

void MachineView::setFrame(const void *pixels, int width, int height, int pitch)
{
	if (!pixels || width <= 0 || height <= 0)
		return;
	// BGRX bytes are a little-endian 0xffRRGGBB word, Qt's RGB32
	m_frame = QImage(static_cast<const uchar *>(pixels), width, height, pitch, QImage::Format_RGB32).copy();
	m_frameChanged = true;
	update();
}

void MachineView::sendViewSize()
{
	const qreal ratio = window() ? window()->effectiveDevicePixelRatio() : 1.0;
	const int w = qMax(1, qRound(width()));
	const int h = qMax(1, qRound(height()));
	pinesy_view_size(w, h, qRound(w * ratio), qRound(h * ratio));
}

void MachineView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
	QQuickItem::geometryChange(newGeometry, oldGeometry);
	if (newGeometry.size() != oldGeometry.size() && !newGeometry.isEmpty())
		sendViewSize();
}

QSGNode *MachineView::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
	// black until the first frame
	if (m_frame.isNull()) {
		auto *rect = dynamic_cast<QSGSimpleRectNode *>(node);
		if (!rect) {
			delete node;
			rect = new QSGSimpleRectNode(boundingRect(), Qt::black);
		}
		rect->setRect(boundingRect());
		return rect;
	}

	auto *texture = dynamic_cast<QSGSimpleTextureNode *>(node);
	if (!texture) {
		delete node;
		texture = new QSGSimpleTextureNode;
		texture->setOwnsTexture(true);
		m_frameChanged = true;
	}
	if (m_frameChanged) {
		texture->setTexture(window()->createTextureFromImage(m_frame));
		m_frameChanged = false;
	}
	// The frame is a whole multiple of the machine's pixels. It keeps its
	// shape, centred, with the window's black where the item's shape
	// differs, and the GPU scales it: sharp where the drawn size is a
	// whole multiple of the frame, smoothed at any other size, which
	// would make uneven pixels.
	const qreal ratio = window() ? window()->effectiveDevicePixelRatio() : 1.0;
	const int viewWidth = qRound(width() * ratio), viewHeight = qRound(height() * ratio);
	int x = 0, y = 0, drawnWidth = 0, drawnHeight = 0;
	const bool whole = pinesy_frame_rect(m_frame.width(), m_frame.height(), viewWidth, viewHeight,
			&x, &y, &drawnWidth, &drawnHeight) != 0;
	texture->setFiltering(whole ? QSGTexture::Nearest : QSGTexture::Linear);
	texture->setRect(QRectF(x / ratio, y / ratio, drawnWidth / ratio, drawnHeight / ratio));
	return texture;
}

void MachineView::keyPressEvent(QKeyEvent *event)
{
	event->accept();
	if (!event->isAutoRepeat())
		pinesy_key(int(event->nativeScanCode()), 1);
	for (char32_t ch : event->text().toUcs4())
		pinesy_char(unsigned(ch));
}

void MachineView::keyReleaseEvent(QKeyEvent *event)
{
	event->accept();
	if (!event->isAutoRepeat())
		pinesy_key(int(event->nativeScanCode()), 0);
}

void MachineView::pointerMoved(const QPointF &position)
{
	if (m_hasPointer)
		pinesy_mouse_move(position.x() - m_pointer.x(), position.y() - m_pointer.y());
	m_hasPointer = true;
	m_pointer = position;
	pinesy_mouse_inside(boundingRect().contains(position) ? 1 : 0);
}

void MachineView::hoverEnterEvent(QHoverEvent *event)
{
	m_hasPointer = true;
	m_pointer = event->position();
	pinesy_mouse_inside(1);
}

void MachineView::hoverMoveEvent(QHoverEvent *event)
{
	pointerMoved(event->position());
}

void MachineView::hoverLeaveEvent(QHoverEvent *)
{
	m_hasPointer = false;
	pinesy_mouse_inside(0);
}

// MAME's mouse buttons: left, right, middle, then back and forward
static int mameButton(Qt::MouseButton button)
{
	switch (button) {
	case Qt::LeftButton: return 0;
	case Qt::RightButton: return 1;
	case Qt::MiddleButton: return 2;
	case Qt::BackButton: return 3;
	case Qt::ForwardButton: return 4;
	default: return -1;
	}
}

void MachineView::mousePressEvent(QMouseEvent *event)
{
	// accepted, so the item gets the moves and the release of the drag
	event->accept();
	if (const int button = mameButton(event->button()); button >= 0)
		pinesy_mouse_button(button, 1);
}

void MachineView::mouseMoveEvent(QMouseEvent *event)
{
	event->accept();
	pointerMoved(event->position());
}

void MachineView::mouseReleaseEvent(QMouseEvent *event)
{
	event->accept();
	if (const int button = mameButton(event->button()); button >= 0)
		pinesy_mouse_button(button, 0);
}

void MachineView::wheelEvent(QWheelEvent *event)
{
	event->accept();
	// 120 is one notch; inverted deltas (natural scrolling) are turned
	// back to the wheel's own direction
	const double notches = event->angleDelta().y() / 120.0;
	pinesy_mouse_wheel(event->inverted() ? -notches : notches);
}
