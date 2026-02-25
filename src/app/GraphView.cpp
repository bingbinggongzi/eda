#include "GraphView.h"
#include "items/NodeItem.h"
#include "model/ComponentCatalog.h"
#include "scene/EditorScene.h"

#include <cmath>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDataStream>
#include <QDropEvent>
#include <QIODevice>
#include <QMap>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtGlobal>
#include <QVariant>
#include <QWheelEvent>

namespace {
QSizeF previewSizeForType(const QString& typeName) {
    const ComponentCatalog& catalog = ComponentCatalog::instance();
    const ComponentSpec* spec = catalog.find(typeName);
    return spec ? spec->size : catalog.fallback().size;
}

QString extractTypeNameFromMime(const QMimeData* mimeData) {
    if (!mimeData) {
        return QString();
    }

    if (mimeData->hasText()) {
        const QString text = mimeData->text().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    }

    static const QString kModelMime = QStringLiteral("application/x-qabstractitemmodeldatalist");
    if (!mimeData->hasFormat(kModelMime)) {
        return QString();
    }

    const QByteArray encoded = mimeData->data(kModelMime);
    QDataStream stream(encoded);
    while (!stream.atEnd()) {
        int row = -1;
        int column = -1;
        QMap<int, QVariant> roleDataMap;
        stream >> row >> column >> roleDataMap;

        const QString userRoleValue = roleDataMap.value(Qt::UserRole).toString().trimmed();
        if (!userRoleValue.isEmpty()) {
            return userRoleValue;
        }

        const QString displayRoleValue = roleDataMap.value(Qt::DisplayRole).toString().trimmed();
        if (!displayRoleValue.isEmpty()) {
            return displayRoleValue;
        }
    }
    return QString();
}
}  // namespace

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QColor(250, 250, 250));
    setAcceptDrops(true);
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

void GraphView::drawForeground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawForeground(painter, rect);

    if (!scene()) {
        return;
    }

    const QList<QGraphicsItem*> selected = scene()->selectedItems();
    if (selected.size() == 1) {
        NodeItem* node = dynamic_cast<NodeItem*>(selected.first());
        if (node) {
            const QPointF center = node->sceneBoundingRect().center();
            QPen guidePen(QColor(255, 153, 51, 150), 1.0, Qt::DashLine);
            painter->setPen(guidePen);
            painter->drawLine(QPointF(rect.left(), center.y()), QPointF(rect.right(), center.y()));
            painter->drawLine(QPointF(center.x(), rect.top()), QPointF(center.x(), rect.bottom()));
        }
    }

    if (!m_dropPreviewActive || m_dropPreviewType.isEmpty()) {
        return;
    }

    const QPointF previewPos = effectiveDropPreviewPos();
    const QSizeF previewSize = previewSizeForType(m_dropPreviewType);
    const QRectF previewRect(previewPos, previewSize);

    painter->save();
    painter->setPen(QPen(QColor(64, 145, 255, 210), 1.4, Qt::DashLine));
    painter->setBrush(QColor(64, 145, 255, 36));
    painter->drawRoundedRect(previewRect, 4.0, 4.0);

    painter->setPen(QPen(QColor(64, 145, 255, 130), 1.0, Qt::DotLine));
    painter->drawLine(QPointF(rect.left(), previewRect.center().y()), QPointF(rect.right(), previewRect.center().y()));
    painter->drawLine(QPointF(previewRect.center().x(), rect.top()), QPointF(previewRect.center().x(), rect.bottom()));

    painter->setPen(QColor(36, 97, 196));
    painter->drawText(previewRect.adjusted(8.0, 2.0, -6.0, -previewRect.height() + 20.0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      m_dropPreviewType);
    painter->restore();
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
    const qreal nextZoom = m_zoom * factor;
    if (nextZoom < m_minZoom || nextZoom > m_maxZoom) {
        return;
    }

    const QPointF before = mapToScene(anchorPos);
    scale(factor, factor);
    m_zoom = nextZoom;
    const QPointF after = mapToScene(anchorPos);
    const QPointF delta = after - before;
    translate(delta.x(), delta.y());
    emit zoomChanged(static_cast<int>(m_zoom * 100.0));
}

void GraphView::dragEnterEvent(QDragEnterEvent* event) {
    const QString typeName = extractTypeNameFromMime(event->mimeData());
    if (!typeName.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint viewPos = event->position().toPoint();
#else
        const QPoint viewPos = event->pos();
#endif
        m_dropPreviewActive = true;
        m_dropPreviewType = typeName;
        m_dropPreviewPos = mapToScene(viewPos);
        viewport()->update();
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void GraphView::dragLeaveEvent(QDragLeaveEvent* event) {
    m_dropPreviewActive = false;
    m_dropPreviewType.clear();
    viewport()->update();
    QGraphicsView::dragLeaveEvent(event);
}

void GraphView::dragMoveEvent(QDragMoveEvent* event) {
    const QString typeName = extractTypeNameFromMime(event->mimeData());
    if (!typeName.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint viewPos = event->position().toPoint();
#else
        const QPoint viewPos = event->pos();
#endif
        m_dropPreviewActive = true;
        m_dropPreviewType = typeName;
        m_dropPreviewPos = mapToScene(viewPos);
        viewport()->update();
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void GraphView::dropEvent(QDropEvent* event) {
    const QString typeName = extractTypeNameFromMime(event->mimeData());
    if (!typeName.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint viewPos = event->position().toPoint();
#else
        const QPoint viewPos = event->pos();
#endif
        m_dropPreviewPos = mapToScene(viewPos);
        emit paletteItemDropped(typeName, effectiveDropPreviewPos());
        m_dropPreviewActive = false;
        m_dropPreviewType.clear();
        viewport()->update();
        event->acceptProposedAction();
        return;
    }
    m_dropPreviewActive = false;
    m_dropPreviewType.clear();
    viewport()->update();
    QGraphicsView::dropEvent(event);
}

QPointF GraphView::effectiveDropPreviewPos() const {
    if (!scene()) {
        return m_dropPreviewPos;
    }

    QPointF snapped = m_dropPreviewPos;
    const EditorScene* editorScene = dynamic_cast<const EditorScene*>(scene());
    if (editorScene && editorScene->snapToGrid()) {
        const int g = editorScene->gridSize();
        snapped.setX(std::round(snapped.x() / g) * g);
        snapped.setY(std::round(snapped.y() / g) * g);
    }
    return snapped;
}
