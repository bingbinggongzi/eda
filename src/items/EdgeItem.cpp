#include "EdgeItem.h"

#include "PortItem.h"

#include <QPainterPath>
#include <QPen>

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
    const qreal midX = (start.x() + end.x()) * 0.5;

    QPainterPath p(start);
    p.lineTo(midX, start.y());
    p.lineTo(midX, end.y());
    p.lineTo(end);
    setPath(p);

    QPen pen(QColor(83, 83, 83), isSelected() ? 2.0 : 1.4);
    if (!m_targetPort) {
        pen.setStyle(Qt::DashLine);
        pen.setColor(QColor(64, 145, 255));
    }
    setPen(pen);
}
