// The item a running machine is shown in: each frame MAME draws, as a
// texture filling the item. Keys go to MAME by their position.

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

private:
	QImage m_frame;
	bool m_frameChanged = false;
};
