#pragma once

#include <QGraphicsPathItem>
#include <QPointer>
#include <QString>

class PortItem;

enum class EdgeRoutingMode {
    Manhattan,
    ObstacleAvoiding
};

enum class EdgeBundlePolicy {
    Centered,
    Directional
};

enum class EdgeRoutingProfile {
    Balanced,
    Dense
};

class EdgeItem : public QGraphicsPathItem {
public:
    explicit EdgeItem(const QString& edgeId, PortItem* sourcePort, QGraphicsItem* parent = nullptr);
    ~EdgeItem() override;

    const QString& edgeId() const;
    PortItem* sourcePort() const;
    PortItem* targetPort() const;
    EdgeRoutingMode routingMode() const;
    EdgeRoutingProfile routingProfile() const;
    EdgeBundlePolicy bundlePolicy() const;
    qreal bundleSpacing() const;

    void setTargetPort(PortItem* port);
    void setPreviewEnd(const QPointF& scenePos);
    void setRoutingMode(EdgeRoutingMode mode);
    void setRoutingProfile(EdgeRoutingProfile profile);
    void setBundlePolicy(EdgeBundlePolicy policy);
    void setBundleSpacing(qreal spacing);
    void updatePath();

private:
    QString m_edgeId;
    QPointer<PortItem> m_sourcePort;
    QPointer<PortItem> m_targetPort;
    QPointF m_previewEnd;
    EdgeRoutingMode m_routingMode = EdgeRoutingMode::Manhattan;
    EdgeRoutingProfile m_routingProfile = EdgeRoutingProfile::Balanced;
    EdgeBundlePolicy m_bundlePolicy = EdgeBundlePolicy::Centered;
    qreal m_bundleSpacing = 18.0;
};
