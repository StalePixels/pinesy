// The item a running machine is shown in: each frame MAME draws, as a
// texture at the frame's shape, centred, on the window's black. Keys go
// to MAME by their position, and the mouse over the item goes to MAME.

#pragma once

#include <QImage>
#include <QQuickItem>

class MachineView : public QQuickItem
{
	Q_OBJECT

public:
	explicit MachineView(QQuickItem *parent = nullptr);

	// pixels is BGRX, top row first; it is valid only during the call.
	void setFrame(const void *pixels, int width, int height, int pitch);

	// Tells MAME the item's size, in logical units and in pixels.
	void sendViewSize();

protected:
	QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override;
	void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void hoverEnterEvent(QHoverEvent *event) override;
	void hoverMoveEvent(QHoverEvent *event) override;
	void hoverLeaveEvent(QHoverEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private:
	// Sends the pointer's movement from its last position, and whether
	// it is over the item.
	void pointerMoved(const QPointF &position);

	QImage m_frame;
	bool m_frameChanged = false;
	// the pointer's last position over the item, to send its movement
	bool m_hasPointer = false;
	QPointF m_pointer;
};
