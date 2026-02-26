#pragma once

#include <QGraphicsPathItem>
#include <QPointF>
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

enum class EdgeBundleScope {
    Global,
    PerLayer,
    PerGroup
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
    EdgeBundleScope bundleScope() const;
    qreal bundleSpacing() const;
    bool passthrough() const;

    void setTargetPort(PortItem* port);
    void setPreviewEnd(const QPointF& scenePos);
    void setRoutingMode(EdgeRoutingMode mode);
    void setRoutingProfile(EdgeRoutingProfile profile);
    void setBundlePolicy(EdgeBundlePolicy policy);
    void setBundleScope(EdgeBundleScope scope);
    void setBundleSpacing(qreal spacing);
    void setPassthrough(bool enabled);
    void setSourceEndpointOverride(const QPointF& scenePos);
    void clearSourceEndpointOverride();
    void setTargetEndpointOverride(const QPointF& scenePos);
    void clearTargetEndpointOverride();
    void clearEndpointOverrides();
    void updatePath();

private:
    QString m_edgeId;
    QPointer<PortItem> m_sourcePort;
    QPointer<PortItem> m_targetPort;
    QPointF m_previewEnd;
    EdgeRoutingMode m_routingMode = EdgeRoutingMode::Manhattan;
    EdgeRoutingProfile m_routingProfile = EdgeRoutingProfile::Balanced;
    EdgeBundlePolicy m_bundlePolicy = EdgeBundlePolicy::Centered;
    EdgeBundleScope m_bundleScope = EdgeBundleScope::Global;
    qreal m_bundleSpacing = 18.0;
    bool m_passthrough = false;
    bool m_hasSourceOverride = false;
    bool m_hasTargetOverride = false;
    QPointF m_sourceOverride;
    QPointF m_targetOverride;
};
