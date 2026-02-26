#pragma once

#include "items/EdgeItem.h"
#include "model/GraphDocument.h"

#include <QGraphicsScene>
#include <QPointF>
#include <QString>

class QUndoStack;
class NodeItem;
class PortItem;
class NodeMoveCommand;
class NodeRenameCommand;
class NodePropertyCommand;

enum class InteractionMode {
    Select,
    Connect,
    Place
};

class EditorScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit EditorScene(QObject* parent = nullptr);

    NodeItem* createNode(const QString& typeName, const QPointF& scenePos);
    EdgeItem* createEdge(PortItem* outputPort, PortItem* inputPort);

    NodeItem* createNodeWithUndo(const QString& typeName, const QPointF& scenePos);
    EdgeItem* createEdgeWithUndo(PortItem* outputPort, PortItem* inputPort);
    void deleteSelectionWithUndo();
    bool renameNodeWithUndo(const QString& nodeId, const QString& newName);
    bool moveNodeWithUndo(const QString& nodeId, const QPointF& newPos);
    bool setNodePropertyWithUndo(const QString& nodeId, const QString& key, const QString& value);

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
    void updateCounterFromId(const QString& id, int* counter);
    QPointF snapPoint(const QPointF& p) const;
    NodeItem* findNodeByIdInternal(const QString& nodeId) const;
    bool applyNodeRenameInternal(const QString& nodeId, const QString& newName, bool emitGraphChanged);
    bool applyNodePositionInternal(const QString& nodeId, const QPointF& newPos, bool emitGraphChanged);
    bool applyNodePropertyInternal(const QString& nodeId, const QString& key, const QString& value, bool emitGraphChanged);
    bool canConnect(PortItem* a, PortItem* b) const;
    bool hasEdgeBetweenPorts(PortItem* outputPort, PortItem* inputPort) const;
    bool inputPortHasConnection(PortItem* inputPort) const;
    PortItem* pickPortAt(const QPointF& scenePos) const;
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
    bool m_snapToGrid = true;

    PortItem* m_pendingPort = nullptr;
    EdgeItem* m_previewEdge = nullptr;
    QUndoStack* m_undoStack = nullptr;
    InteractionMode m_mode = InteractionMode::Select;
    QString m_placementType;
    EdgeRoutingMode m_edgeRoutingMode = EdgeRoutingMode::Manhattan;
};
