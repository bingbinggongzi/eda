#pragma once

#include <QGraphicsView>
#include <QPoint>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class GraphView : public QGraphicsView {
    Q_OBJECT

public:
    explicit GraphView(QWidget* parent = nullptr);

signals:
    void paletteItemDropped(const QString& typeName, const QPointF& scenePos);
    void zoomChanged(int percent);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void applyZoom(qreal factor, const QPoint& anchorPos);

    bool m_panning = false;
    QPoint m_lastPanPoint;
    qreal m_zoom = 1.0;
    const qreal m_minZoom = 0.25;
    const qreal m_maxZoom = 3.0;
};
