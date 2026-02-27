#include "NodeItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

NodeItem::NodeItem(const QString& nodeId,
                   const QString& typeName,
                   const QString& displayName,
                   const QSizeF& size,
                   QGraphicsItem* parent)
    : QGraphicsObject(parent),
      m_nodeId(nodeId),
      m_typeName(typeName),
      m_displayName(displayName),
      m_size(size) {
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setZValue(1.0);
    setTransformOriginPoint(m_size.width() * 0.5, m_size.height() * 0.5);
}

QRectF NodeItem::boundingRect() const {
    return QRectF(0.0, 0.0, m_size.width(), m_size.height());
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = boundingRect();
    const QRectF titleRect(0.0, 0.0, body.width(), 22.0);

    QColor border = isSelected() ? QColor(36, 105, 230) : QColor(129, 142, 173);
    QColor bodyColor(223, 232, 246);
    QColor titleColor = isSelected() ? QColor(196, 216, 247) : QColor(209, 224, 245);

    painter->setPen(QPen(border, isSelected() ? 1.8 : 1.2));
    painter->setBrush(bodyColor);
    painter->drawRoundedRect(body, 4.0, 4.0);

    painter->setPen(Qt::NoPen);
    painter->setBrush(titleColor);
    painter->drawRoundedRect(titleRect, 4.0, 4.0);
    painter->fillRect(QRectF(0.0, 18.0, body.width(), 4.0), titleColor);

    painter->setPen(QColor(35, 63, 189));
    painter->drawText(QRectF(8.0, 2.0, body.width() - 12.0, 18.0), Qt::AlignVCenter | Qt::AlignLeft, m_displayName);
}

const QString& NodeItem::nodeId() const {
    return m_nodeId;
}

const QString& NodeItem::typeName() const {
    return m_typeName;
}

const QString& NodeItem::displayName() const {
    return m_displayName;
}

const QSizeF& NodeItem::nodeSize() const {
    return m_size;
}

void NodeItem::setDisplayName(const QString& name) {
    if (m_displayName == name) {
        return;
    }
    m_displayName = name;
    update();
}

const QString& NodeItem::groupId() const {
    return m_groupId;
}

void NodeItem::setGroupId(const QString& groupId) {
    m_groupId = groupId;
}

const QString& NodeItem::layerId() const {
    return m_layerId;
}

void NodeItem::setLayerId(const QString& layerId) {
    m_layerId = layerId;
}

PortItem* NodeItem::addPort(const QString& portId, const QString& name, PortDirection direction) {
    PortItem* port = new PortItem(portId, name, direction, this, this);
    if (direction == PortDirection::Input) {
        m_inputPorts.append(port);
    } else {
        m_outputPorts.append(port);
    }
    layoutPorts();
    return port;
}

const QVector<PortItem*>& NodeItem::inputPorts() const {
    return m_inputPorts;
}

const QVector<PortItem*>& NodeItem::outputPorts() const {
    return m_outputPorts;
}

PortItem* NodeItem::firstInputPort() const {
    return m_inputPorts.isEmpty() ? nullptr : m_inputPorts.first();
}

PortItem* NodeItem::firstOutputPort() const {
    return m_outputPorts.isEmpty() ? nullptr : m_outputPorts.first();
}

const QVector<PropertyData>& NodeItem::properties() const {
    return m_properties;
}

void NodeItem::setProperties(const QVector<PropertyData>& properties) {
    m_properties = properties;
}

QString NodeItem::propertyValue(const QString& key) const {
    for (const PropertyData& p : m_properties) {
        if (p.key == key) {
            return p.value;
        }
    }
    return QString();
}

QString NodeItem::propertyType(const QString& key) const {
    for (const PropertyData& p : m_properties) {
        if (p.key == key) {
            return p.type;
        }
    }
    return QStringLiteral("string");
}

bool NodeItem::setPropertyValue(const QString& key, const QString& value) {
    for (PropertyData& p : m_properties) {
        if (p.key == key) {
            if (p.value == value) {
                return false;
            }
            p.value = value;
            return true;
        }
    }
    return false;
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == QGraphicsItem::ItemPositionHasChanged || change == QGraphicsItem::ItemRotationHasChanged ||
        change == QGraphicsItem::ItemTransformHasChanged || change == QGraphicsItem::ItemScenePositionHasChanged) {
        for (PortItem* port : m_inputPorts) {
            if (port) {
                port->updateConnectedEdges();
            }
        }
        for (PortItem* port : m_outputPorts) {
            if (port) {
                port->updateConnectedEdges();
            }
        }
        emit nodeMoved(this);
    }
    return QGraphicsObject::itemChange(change, value);
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = pos();
    }
    QGraphicsObject::mousePressEvent(event);
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const QPointF endPos = pos();
        if (m_dragStartPos != endPos) {
            emit nodeDragFinished(this, m_dragStartPos, endPos);
        }
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void NodeItem::layoutPorts() {
    auto layoutSide = [this](const QVector<PortItem*>& ports, qreal xPos) {
        if (ports.isEmpty()) {
            return;
        }
        const qreal top = 30.0;
        const qreal bottom = m_size.height() - 10.0;
        const qreal step = ports.size() == 1 ? 0.0 : (bottom - top) / static_cast<qreal>(ports.size() - 1);
        for (int i = 0; i < ports.size(); ++i) {
            const qreal y = (ports.size() == 1) ? (top + bottom) * 0.5 : top + static_cast<qreal>(i) * step;
            ports[i]->setPos(xPos, y);
        }
    };

    layoutSide(m_inputPorts, 0.0);
    layoutSide(m_outputPorts, m_size.width());
}
