#pragma once

#include <QGraphicsPathItem>
#include <QPointer>
#include <QString>

class PortItem;

enum class EdgeRoutingMode {
    Manhattan,
    ObstacleAvoiding
};

class EdgeItem : public QGraphicsPathItem {
public:
    explicit EdgeItem(const QString& edgeId, PortItem* sourcePort, QGraphicsItem* parent = nullptr);
    ~EdgeItem() override;

    const QString& edgeId() const;
    PortItem* sourcePort() const;
    PortItem* targetPort() const;
    EdgeRoutingMode routingMode() const;

    void setTargetPort(PortItem* port);
    void setPreviewEnd(const QPointF& scenePos);
    void setRoutingMode(EdgeRoutingMode mode);
    void updatePath();

private:
    QString m_edgeId;
    QPointer<PortItem> m_sourcePort;
    QPointer<PortItem> m_targetPort;
    QPointF m_previewEnd;
    EdgeRoutingMode m_routingMode = EdgeRoutingMode::Manhattan;
};
