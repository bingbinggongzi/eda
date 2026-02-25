#include "PortItem.h"

#include "EdgeItem.h"

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

PortItem::PortItem(const QString& id,
                   const QString& name,
                   PortDirection direction,
                   NodeItem* ownerNode,
                   QGraphicsItem* parent)
    : QGraphicsObject(parent),
      m_id(id),
      m_name(name),
      m_direction(direction),
      m_ownerNode(ownerNode) {
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
    setCursor(QCursor(Qt::CrossCursor));
    setZValue(3.0);
}

QRectF PortItem::boundingRect() const {
    return QRectF(-6.0, -6.0, 12.0, 12.0);
}

void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor fill = (m_direction == PortDirection::Input) ? QColor(132, 161, 96) : QColor(84, 139, 220);
    if (m_hovered) {
        fill = fill.lighter(120);
    }
    if (isUnderMouse()) {
        fill = fill.lighter(125);
    }

    painter->setPen(QPen(QColor(70, 70, 70), 1.0));
    painter->setBrush(fill);
    painter->drawEllipse(boundingRect().adjusted(1.0, 1.0, -1.0, -1.0));
}

const QString& PortItem::portId() const {
    return m_id;
}

const QString& PortItem::portName() const {
    return m_name;
}

PortDirection PortItem::direction() const {
    return m_direction;
}

NodeItem* PortItem::ownerNode() const {
    return m_ownerNode;
}

void PortItem::addEdge(EdgeItem* edge) {
    if (!m_edges.contains(edge)) {
        m_edges.append(edge);
    }
}

void PortItem::removeEdge(EdgeItem* edge) {
    m_edges.removeAll(edge);
}

void PortItem::updateConnectedEdges() {
    for (EdgeItem* edge : m_edges) {
        if (edge) {
            edge->updatePath();
        }
    }
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    m_hovered = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    m_hovered = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit connectionStart(this);
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit connectionRelease(this);
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}
