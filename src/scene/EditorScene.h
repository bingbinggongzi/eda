#pragma once

#include "items/EdgeItem.h"
#include "model/GraphDocument.h"

#include <QGraphicsScene>
#include <QHash>
#include <QPointF>
#include <QSet>
#include <QString>

class QUndoStack;
class NodeItem;
class PortItem;
class NodeMoveCommand;
class NodeRenameCommand;
class NodePropertyCommand;
class QGraphicsItemGroup;

enum class InteractionMode {
    Select,
    Connect,
    Place
};

enum class AutoLayoutMode {
    Layered,
    Grid
};

class EditorScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit EditorScene(QObject* parent = nullptr);
    ~EditorScene() override;

    NodeItem* createNode(const QString& typeName, const QPointF& scenePos);
    EdgeItem* createEdge(PortItem* outputPort, PortItem* inputPort);

    NodeItem* createNodeWithUndo(const QString& typeName, const QPointF& scenePos);
    EdgeItem* createEdgeWithUndo(PortItem* outputPort, PortItem* inputPort);
    void deleteSelectionWithUndo();
    bool renameNodeWithUndo(const QString& nodeId, const QString& newName);
    bool moveNodeWithUndo(const QString& nodeId, const QPointF& newPos);
    bool setNodePropertyWithUndo(const QString& nodeId, const QString& key, const QString& value);
    bool autoLayoutWithUndo(bool selectedOnly = true);
    bool rotateSelectionWithUndo(qreal deltaDegrees);
    bool bringSelectionToFrontWithUndo();
    bool sendSelectionToBackWithUndo();
    bool bringSelectionForwardWithUndo();
    bool sendSelectionBackwardWithUndo();
    QString createLayerWithUndo(const QString& name = QString());
    bool renameLayerWithUndo(const QString& layerId, const QString& name);
    bool setLayerVisibleWithUndo(const QString& layerId, bool visible);
    bool setLayerLockedWithUndo(const QString& layerId, bool locked);
    bool moveLayerWithUndo(const QString& layerId, int targetIndex);
    bool deleteLayerWithUndo(const QString& layerId);
    bool setActiveLayerWithUndo(const QString& layerId);
    bool moveSelectionToLayerWithUndo(const QString& layerId);
    bool groupSelectionWithUndo();
    bool ungroupSelectionWithUndo();
    bool collapseSelectionWithUndo();
    bool expandSelectionWithUndo();

    NodeItem* createNodeFromData(const NodeData& nodeData);
    EdgeItem* createEdgeFromData(const EdgeData& edgeData);
    void clearGraph();

    GraphDocument toDocument() const;
    bool fromDocument(const GraphDocument& document);

    void setSnapToGrid(bool enabled);
    bool snapToGrid() const;
    int gridSize() const;
    void setUndoStack(QUndoStack* stack);
    void setInteractionMode(InteractionMode mode);
    InteractionMode interactionMode() const;
    void setPlacementType(const QString& typeName);
    void setEdgeRoutingMode(EdgeRoutingMode mode);
    EdgeRoutingMode edgeRoutingMode() const;
    void setEdgeRoutingProfile(EdgeRoutingProfile profile);
    EdgeRoutingProfile edgeRoutingProfile() const;
    void setEdgeBundlePolicy(EdgeBundlePolicy policy);
    EdgeBundlePolicy edgeBundlePolicy() const;
    void setEdgeBundleScope(EdgeBundleScope scope);
    EdgeBundleScope edgeBundleScope() const;
    void setEdgeBundleSpacing(qreal spacing);
    qreal edgeBundleSpacing() const;
    void setAutoLayoutMode(AutoLayoutMode mode);
    AutoLayoutMode autoLayoutMode() const;
    void setAutoLayoutSpacing(qreal horizontal, qreal vertical);
    qreal autoLayoutHorizontalSpacing() const;
    qreal autoLayoutVerticalSpacing() const;
    QVector<LayerData> layers() const;
    QString activeLayerId() const;
    int layerNodeCount(const QString& layerId) const;

signals:
    void selectionInfoChanged(const QString& itemType,
                              const QString& itemId,
                              const QString& displayName,
                              const QPointF& pos,
                              int inputCount,
                              int outputCount);
    void graphChanged();
    void connectionStateChanged(bool active);
    void layerStateChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private slots:
    void onPortConnectionStart(PortItem* port);
    void onPortConnectionRelease(PortItem* port);
    void onSelectionChangedInternal();
    void onNodeDragFinished(NodeItem* node, const QPointF& oldPos, const QPointF& newPos);

private:
    friend class NodeMoveCommand;
    friend class NodeRenameCommand;
    friend class NodePropertyCommand;

    QString nextNodeId();
    QString nextPortId();
    QString nextEdgeId();
    QString nextGroupId();
    QString nextLayerId();
    void updateCounterFromId(const QString& id, int* counter);
    void ensureLayerModel();
    const LayerData* findLayerById(const QString& layerId) const;
    LayerData* findLayerByIdMutable(const QString& layerId);
    bool isLayerVisible(const QString& layerId) const;
    bool isLayerLocked(const QString& layerId) const;
    void sanitizeNodeLayers();
    void rebuildNodeGroups();
    void clearNodeGroups();
    QGraphicsItemGroup* owningGroupItem(QGraphicsItem* item) const;
    QSet<QString> collectSelectedGroupIds() const;
    void refreshCollapsedVisibility();
    bool toggleGroupCollapsedByIdWithUndo(const QString& groupId);
    QPointF snapPoint(const QPointF& p) const;
    NodeItem* findNodeByIdInternal(const QString& nodeId) const;
    bool applyNodeRenameInternal(const QString& nodeId, const QString& newName, bool emitGraphChanged);
    bool applyNodePositionInternal(const QString& nodeId, const QPointF& newPos, bool emitGraphChanged);
    bool applyNodePropertyInternal(const QString& nodeId, const QString& key, const QString& value, bool emitGraphChanged);
    bool canConnect(PortItem* a, PortItem* b) const;
    bool hasEdgeBetweenPorts(PortItem* outputPort, PortItem* inputPort) const;
    bool inputPortHasConnection(PortItem* inputPort) const;
    PortItem* pickPortAt(const QPointF& scenePos) const;
    QVector<NodeItem*> collectSelectedNodes() const;
    QVector<NodeItem*> collectLayoutNodes(bool selectedOnly) const;
    bool applyAutoLayout(const QVector<NodeItem*>& nodes);
    bool applyLayeredAutoLayout(const QVector<NodeItem*>& nodes, qreal xSpacing, qreal ySpacing);
    bool applyGridAutoLayout(const QVector<NodeItem*>& nodes, qreal xSpacing, qreal ySpacing);
    void finishConnectionAt(const QPointF& scenePos, PortItem* explicitTarget = nullptr);
    NodeItem* buildNodeByType(const QString& typeName);
    NodeItem* buildNode(const QString& nodeId,
                        const QString& typeName,
                        const QString& displayName,
                        const QSizeF& size,
                        const QVector<PortData>& ports,
                        const QVector<PropertyData>& properties);

    int m_nodeCounter = 1;
    int m_portCounter = 1;
    int m_edgeCounter = 1;
    int m_groupCounter = 1;
    int m_layerCounter = 1;
    bool m_snapToGrid = true;

    PortItem* m_pendingPort = nullptr;
    EdgeItem* m_previewEdge = nullptr;
    QHash<QString, QGraphicsItemGroup*> m_nodeGroups;
    QSet<QString> m_collapsedGroups;
    QGraphicsItemGroup* m_draggingGroup = nullptr;
    QPointF m_draggingGroupStartPos;
    GraphDocument m_draggingGroupBefore;
    bool m_draggingGroupTracked = false;
    QUndoStack* m_undoStack = nullptr;
    InteractionMode m_mode = InteractionMode::Select;
    QString m_placementType;
    EdgeRoutingMode m_edgeRoutingMode = EdgeRoutingMode::Manhattan;
    EdgeRoutingProfile m_edgeRoutingProfile = EdgeRoutingProfile::Balanced;
    EdgeBundlePolicy m_edgeBundlePolicy = EdgeBundlePolicy::Centered;
    EdgeBundleScope m_edgeBundleScope = EdgeBundleScope::Global;
    qreal m_edgeBundleSpacing = 18.0;
    AutoLayoutMode m_autoLayoutMode = AutoLayoutMode::Layered;
    qreal m_autoLayoutHorizontalSpacing = 240.0;
    qreal m_autoLayoutVerticalSpacing = 140.0;
    QVector<LayerData> m_layers;
    QString m_activeLayerId;
};
