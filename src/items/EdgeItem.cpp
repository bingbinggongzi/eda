#include "EdgeItem.h"

#include "NodeItem.h"
#include "PortItem.h"

#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>

#include <cmath>
#include <limits>

namespace {
qreal computeDetourY(QGraphicsScene* scene,
                     const QPointF& start,
                     const QPointF& end,
                     const NodeItem* sourceNode,
                     const NodeItem* targetNode) {
    if (!scene) {
        return std::numeric_limits<qreal>::quiet_NaN();
    }

    const qreal minX = std::min(start.x(), end.x());
    const qreal maxX = std::max(start.x(), end.x());
    const qreal lineY = (start.y() + end.y()) * 0.5;
    QRectF sweep(minX, lineY - 4.0, maxX - minX, 8.0);
    if (sweep.width() < 1.0) {
        sweep.setWidth(1.0);
    }

    qreal detourY = std::numeric_limits<qreal>::quiet_NaN();
    for (QGraphicsItem* item : scene->items(sweep, Qt::IntersectsItemShape, Qt::AscendingOrder)) {
        const NodeItem* node = dynamic_cast<const NodeItem*>(item);
        if (!node || node == sourceNode || node == targetNode) {
            continue;
        }
        const QRectF rect = node->sceneBoundingRect();
        if (rect.intersects(sweep)) {
            detourY = std::max(std::isnan(detourY) ? rect.bottom() + 24.0 : detourY, rect.bottom() + 24.0);
        }
    }
    return detourY;
}

qreal computeMidX(QGraphicsScene* scene,
                  const QPointF& start,
                  const QPointF& end,
                  const NodeItem* sourceNode,
                  const NodeItem* targetNode,
                  qreal initialMidX) {
    if (!scene) {
        return initialMidX;
    }
    const qreal top = std::min(start.y(), end.y());
    const qreal bottom = std::max(start.y(), end.y());
    QRectF sweep(initialMidX - 3.0, top, 6.0, std::max<qreal>(1.0, bottom - top));
    qreal midX = initialMidX;

    for (QGraphicsItem* item : scene->items(sweep, Qt::IntersectsItemShape, Qt::AscendingOrder)) {
        const NodeItem* node = dynamic_cast<const NodeItem*>(item);
        if (!node || node == sourceNode || node == targetNode) {
            continue;
        }
        const QRectF rect = node->sceneBoundingRect();
        if (rect.intersects(sweep)) {
            midX = std::max(midX, rect.right() + 40.0);
        }
    }
    return midX;
}
}  // namespace

EdgeItem::EdgeItem(const QString& edgeId, PortItem* sourcePort, QGraphicsItem* parent)
    : QGraphicsPathItem(parent),
      m_edgeId(edgeId),
      m_sourcePort(sourcePort),
      m_previewEnd(sourcePort ? sourcePort->scenePos() : QPointF()) {
    setZValue(-1.0);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    if (m_sourcePort) {
        m_sourcePort->addEdge(this);
    }
    updatePath();
}

EdgeItem::~EdgeItem() {
    if (m_sourcePort) {
        m_sourcePort->removeEdge(this);
    }
    if (m_targetPort) {
        m_targetPort->removeEdge(this);
    }
}

const QString& EdgeItem::edgeId() const {
    return m_edgeId;
}

PortItem* EdgeItem::sourcePort() const {
    return m_sourcePort;
}

PortItem* EdgeItem::targetPort() const {
    return m_targetPort;
}

void EdgeItem::setTargetPort(PortItem* port) {
    if (m_targetPort == port) {
        return;
    }
    if (m_targetPort) {
        m_targetPort->removeEdge(this);
    }
    m_targetPort = port;
    if (m_targetPort) {
        m_targetPort->addEdge(this);
    }
    updatePath();
}

void EdgeItem::setPreviewEnd(const QPointF& scenePos) {
    m_previewEnd = scenePos;
    updatePath();
}

void EdgeItem::updatePath() {
    if (!m_sourcePort) {
        return;
    }

    const QPointF start = m_sourcePort->scenePos();
    const QPointF end = m_targetPort ? m_targetPort->scenePos() : m_previewEnd;
    const NodeItem* sourceNode = m_sourcePort->ownerNode();
    const NodeItem* targetNode = m_targetPort ? m_targetPort->ownerNode() : nullptr;

    qreal midX = (start.x() + end.x()) * 0.5;
    midX = computeMidX(scene(), start, end, sourceNode, targetNode, midX);
    const qreal detourY = computeDetourY(scene(), start, end, sourceNode, targetNode);

    QPainterPath p(start);
    if (!std::isnan(detourY)) {
        const qreal xOffset = (end.x() >= start.x()) ? 24.0 : -24.0;
        p.lineTo(start.x() + xOffset, start.y());
        p.lineTo(start.x() + xOffset, detourY);
        p.lineTo(end.x() - xOffset, detourY);
        p.lineTo(end.x() - xOffset, end.y());
    } else {
        p.lineTo(midX, start.y());
        p.lineTo(midX, end.y());
    }
    p.lineTo(end);
    setPath(p);

    QPen pen(QColor(83, 83, 83), isSelected() ? 2.0 : 1.4);
    if (!m_targetPort) {
        pen.setStyle(Qt::DashLine);
        pen.setColor(QColor(64, 145, 255));
    }
    setPen(pen);
}
