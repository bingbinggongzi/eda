#pragma once

#include <QGraphicsPathItem>
#include <QString>

class PortItem;

class EdgeItem : public QGraphicsPathItem {
public:
    explicit EdgeItem(const QString& edgeId, PortItem* sourcePort, QGraphicsItem* parent = nullptr);
    ~EdgeItem() override;

    const QString& edgeId() const;
    PortItem* sourcePort() const;
    PortItem* targetPort() const;

    void setTargetPort(PortItem* port);
    void setPreviewEnd(const QPointF& scenePos);
    void updatePath();

private:
    QString m_edgeId;
    PortItem* m_sourcePort = nullptr;
    PortItem* m_targetPort = nullptr;
    QPointF m_previewEnd;
};
