#include "GraphView.h"

#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtGlobal>
#include <QWheelEvent>

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QColor(250, 250, 250));
}

void GraphView::drawBackground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawBackground(painter, rect);

    constexpr int kMinorGrid = 20;
    constexpr int kMajorGrid = 100;

    const int left = static_cast<int>(std::floor(rect.left()));
    const int right = static_cast<int>(std::ceil(rect.right()));
    const int top = static_cast<int>(std::floor(rect.top()));
    const int bottom = static_cast<int>(std::ceil(rect.bottom()));

    QPen minorPen(QColor(235, 235, 235));
    minorPen.setWidth(1);
    painter->setPen(minorPen);
    for (int x = left - (left % kMinorGrid); x < right; x += kMinorGrid) {
        painter->drawLine(x, top, x, bottom);
    }
    for (int y = top - (top % kMinorGrid); y < bottom; y += kMinorGrid) {
        painter->drawLine(left, y, right, y);
    }

    QPen majorPen(QColor(215, 215, 215));
    majorPen.setWidth(1);
    painter->setPen(majorPen);
    for (int x = left - (left % kMajorGrid); x < right; x += kMajorGrid) {
        painter->drawLine(x, top, x, bottom);
    }
    for (int y = top - (top % kMajorGrid); y < bottom; y += kMajorGrid) {
        painter->drawLine(left, y, right, y);
    }
}

void GraphView::wheelEvent(QWheelEvent* event) {
    if ((event->modifiers() & Qt::ControlModifier) == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint anchorPos = event->position().toPoint();
#else
    const QPoint anchorPos = event->pos();
#endif
    applyZoom(factor, anchorPos);
    event->accept();
}

void GraphView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphView::applyZoom(qreal factor, const QPoint& anchorPos) {
    const QPointF before = mapToScene(anchorPos);
    scale(factor, factor);
    const QPointF after = mapToScene(anchorPos);
    const QPointF delta = after - before;
    translate(delta.x(), delta.y());
}
