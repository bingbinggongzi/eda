#pragma once

#include "items/EdgeItem.h"
#include "model/GraphDocument.h"

#include <QGraphicsScene>
#include <QHash>
#include <QPointF>
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
    bool groupSelectionWithUndo();
    bool ungroupSelectionWithUndo();

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

signals:
    void selectionInfoChanged(const QString& itemType,
                              const QString& itemId,
                              const QString& displayName,
                              const QPointF& pos,
                              int inputCount,
                              int outputCount);
    void graphChanged();
    void connectionStateChanged(bool active);

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
    void updateCounterFromId(const QString& id, int* counter);
    void rebuildNodeGroups();
    void clearNodeGroups();
    QGraphicsItemGroup* owningGroupItem(QGraphicsItem* item) const;
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
    bool m_snapToGrid = true;

    PortItem* m_pendingPort = nullptr;
    EdgeItem* m_previewEdge = nullptr;
    QHash<QString, QGraphicsItemGroup*> m_nodeGroups;
    QGraphicsItemGroup* m_draggingGroup = nullptr;
    QPointF m_draggingGroupStartPos;
    GraphDocument m_draggingGroupBefore;
    bool m_draggingGroupTracked = false;
    QUndoStack* m_undoStack = nullptr;
    InteractionMode m_mode = InteractionMode::Select;
    QString m_placementType;
    EdgeRoutingMode m_edgeRoutingMode = EdgeRoutingMode::Manhattan;
};
