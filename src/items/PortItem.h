#pragma once

#include <QGraphicsObject>
#include <QList>
#include <QString>

class EdgeItem;
class NodeItem;

enum class PortDirection {
    Input,
    Output
};

class PortItem : public QGraphicsObject {
    Q_OBJECT

public:
    PortItem(const QString& id,
             const QString& name,
             PortDirection direction,
             NodeItem* ownerNode,
             QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    const QString& portId() const;
    const QString& portName() const;
    PortDirection direction() const;
    NodeItem* ownerNode() const;

    void addEdge(EdgeItem* edge);
    void removeEdge(EdgeItem* edge);
    void updateConnectedEdges();

signals:
    void connectionStart(PortItem* port);
    void connectionRelease(PortItem* port);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QString m_id;
    QString m_name;
    PortDirection m_direction = PortDirection::Input;
    NodeItem* m_ownerNode = nullptr;
    QList<EdgeItem*> m_edges;
    bool m_hovered = false;
};
