#pragma once

#include "PortItem.h"

#include <QGraphicsObject>
#include <QSizeF>
#include <QString>
#include <QVector>

class QGraphicsSceneMouseEvent;

class NodeItem : public QGraphicsObject {
    Q_OBJECT

public:
    NodeItem(const QString& nodeId,
             const QString& typeName,
             const QString& displayName,
             const QSizeF& size,
             QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    const QString& nodeId() const;
    const QString& typeName() const;
    const QString& displayName() const;
    const QSizeF& nodeSize() const;
    void setDisplayName(const QString& name);

    PortItem* addPort(const QString& portId, const QString& name, PortDirection direction);
    const QVector<PortItem*>& inputPorts() const;
    const QVector<PortItem*>& outputPorts() const;

    PortItem* firstInputPort() const;
    PortItem* firstOutputPort() const;

signals:
    void nodeMoved(NodeItem* node);
    void nodeDragFinished(NodeItem* node, const QPointF& oldPos, const QPointF& newPos);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void layoutPorts();

    QString m_nodeId;
    QString m_typeName;
    QString m_displayName;
    QSizeF m_size;
    QVector<PortItem*> m_inputPorts;
    QVector<PortItem*> m_outputPorts;
    QPointF m_dragStartPos;
};
