#include "EditorScene.h"

#include "commands/DocumentStateCommand.h"
#include "commands/NodeEditCommands.h"
#include "items/EdgeItem.h"
#include "items/NodeItem.h"
#include "items/PortItem.h"
#include "model/ComponentCatalog.h"

#include <algorithm>
#include <cmath>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QColor>
#include <QMap>
#include <QPen>
#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>
#include <QUndoStack>

namespace {
bool areDocumentsEquivalent(const GraphDocument& a, const GraphDocument& b) {
    if (a.nodes.size() != b.nodes.size() || a.edges.size() != b.edges.size()) {
        return false;
    }
    if (a.autoLayoutMode != b.autoLayoutMode ||
        !qFuzzyCompare(a.autoLayoutXSpacing + 1.0, b.autoLayoutXSpacing + 1.0) ||
        !qFuzzyCompare(a.autoLayoutYSpacing + 1.0, b.autoLayoutYSpacing + 1.0)) {
        return false;
    }
    if (a.edgeBundlePolicy != b.edgeBundlePolicy ||
        !qFuzzyCompare(a.edgeBundleSpacing + 1.0, b.edgeBundleSpacing + 1.0)) {
        return false;
    }
    if (a.collapsedGroupIds.size() != b.collapsedGroupIds.size()) {
        return false;
    }
    QVector<QString> collapsedA = a.collapsedGroupIds;
    QVector<QString> collapsedB = b.collapsedGroupIds;
    std::sort(collapsedA.begin(), collapsedA.end());
    std::sort(collapsedB.begin(), collapsedB.end());
    if (collapsedA != collapsedB) {
        return false;
    }

    auto samePort = [](const PortData& p1, const PortData& p2) {
        return p1.id == p2.id && p1.name == p2.name && p1.direction == p2.direction;
    };
    auto sameNode = [&](const NodeData& n1, const NodeData& n2) {
        if (n1.id != n2.id || n1.type != n2.type || n1.name != n2.name || n1.position != n2.position || n1.size != n2.size ||
            !qFuzzyCompare(n1.rotationDegrees + 1.0, n2.rotationDegrees + 1.0) ||
            !qFuzzyCompare(n1.z + 1.0, n2.z + 1.0) || n1.groupId != n2.groupId || n1.ports.size() != n2.ports.size() ||
            n1.properties.size() != n2.properties.size()) {
            return false;
        }
        for (int i = 0; i < n1.ports.size(); ++i) {
            if (!samePort(n1.ports[i], n2.ports[i])) {
                return false;
            }
        }
        for (int i = 0; i < n1.properties.size(); ++i) {
            if (n1.properties[i].key != n2.properties[i].key || n1.properties[i].type != n2.properties[i].type ||
                n1.properties[i].value != n2.properties[i].value) {
                return false;
            }
        }
        return true;
    };
    auto sameEdge = [](const EdgeData& e1, const EdgeData& e2) {
        return e1.id == e2.id && e1.fromNodeId == e2.fromNodeId && e1.fromPortId == e2.fromPortId &&
               e1.toNodeId == e2.toNodeId && e1.toPortId == e2.toPortId;
    };

    for (int i = 0; i < a.nodes.size(); ++i) {
        if (!sameNode(a.nodes[i], b.nodes[i])) {
            return false;
        }
    }
    for (int i = 0; i < a.edges.size(); ++i) {
        if (!sameEdge(a.edges[i], b.edges[i])) {
            return false;
        }
    }
    return true;
}
}  // namespace

EditorScene::EditorScene(QObject* parent)
    : QGraphicsScene(parent) {
    connect(this, &QGraphicsScene::selectionChanged, this, &EditorScene::onSelectionChangedInternal);
}

EditorScene::~EditorScene() {
    disconnect(this, &QGraphicsScene::selectionChanged, this, &EditorScene::onSelectionChangedInternal);
}

NodeItem* EditorScene::createNode(const QString& typeName, const QPointF& scenePos) {
    NodeItem* node = buildNodeByType(typeName);
    if (!node) {
        return nullptr;
    }
    node->setPos(snapPoint(scenePos));
    addItem(node);
    emit graphChanged();
    return node;
}

NodeItem* EditorScene::createNodeWithUndo(const QString& typeName, const QPointF& scenePos) {
    const GraphDocument before = toDocument();
    NodeItem* node = createNode(typeName, scenePos);
    if (!node || !m_undoStack) {
        return node;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Add Node"), true));
    }
    return node;
}

NodeItem* EditorScene::createNodeFromData(const NodeData& nodeData) {
    NodeItem* node = buildNode(nodeData.id, nodeData.type, nodeData.name, nodeData.size, nodeData.ports, nodeData.properties);
    if (!node) {
        return nullptr;
    }
    node->setPos(nodeData.position);
    node->setRotation(nodeData.rotationDegrees);
    node->setZValue(nodeData.z);
    node->setGroupId(nodeData.groupId);
    addItem(node);

    updateCounterFromId(nodeData.id, &m_nodeCounter);
    if (!nodeData.groupId.isEmpty()) {
        updateCounterFromId(nodeData.groupId, &m_groupCounter);
    }
    for (const PortData& port : nodeData.ports) {
        updateCounterFromId(port.id, &m_portCounter);
    }
    return node;
}

EdgeItem* EditorScene::createEdge(PortItem* outputPort, PortItem* inputPort) {
    if (!outputPort || !inputPort) {
        return nullptr;
    }
    if (outputPort->direction() != PortDirection::Output || inputPort->direction() != PortDirection::Input) {
        return nullptr;
    }
    if (!canConnect(outputPort, inputPort)) {
        return nullptr;
    }

    EdgeItem* edge = new EdgeItem(nextEdgeId(), outputPort);
    edge->setRoutingMode(m_edgeRoutingMode);
    edge->setBundlePolicy(m_edgeBundlePolicy);
    edge->setBundleSpacing(m_edgeBundleSpacing);
    edge->setTargetPort(inputPort);
    addItem(edge);
    const NodeItem* sourceNode = outputPort->ownerNode();
    const NodeItem* targetNode = inputPort->ownerNode();
    if (sourceNode && targetNode) {
        for (QGraphicsItem* item : items()) {
            if (EdgeItem* existing = dynamic_cast<EdgeItem*>(item)) {
                if (!existing->sourcePort() || !existing->targetPort()) {
                    continue;
                }
                const NodeItem* existingSource = existing->sourcePort()->ownerNode();
                const NodeItem* existingTarget = existing->targetPort()->ownerNode();
                if (!existingSource || !existingTarget) {
                    continue;
                }
                if (existingSource->nodeId() == sourceNode->nodeId() && existingTarget->nodeId() == targetNode->nodeId()) {
                    existing->updatePath();
                }
            }
        }
    }
    emit graphChanged();
    return edge;
}

EdgeItem* EditorScene::createEdgeWithUndo(PortItem* outputPort, PortItem* inputPort) {
    const GraphDocument before = toDocument();
    EdgeItem* edge = createEdge(outputPort, inputPort);
    if (!edge || !m_undoStack) {
        return edge;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Connect"), true));
    }
    return edge;
}

bool EditorScene::renameNodeWithUndo(const QString& nodeId, const QString& newName) {
    NodeItem* target = findNodeByIdInternal(nodeId);
    if (!target || target->displayName() == newName) {
        return false;
    }

    const QString oldName = target->displayName();
    if (!applyNodeRenameInternal(nodeId, newName, true)) {
        return false;
    }

    if (m_undoStack) {
        m_undoStack->push(new NodeRenameCommand(this, nodeId, oldName, newName, true));
    }
    return true;
}

bool EditorScene::moveNodeWithUndo(const QString& nodeId, const QPointF& newPos) {
    NodeItem* target = findNodeByIdInternal(nodeId);
    if (!target) {
        return false;
    }

    const QPointF snapped = snapPoint(newPos);
    if (target->pos() == snapped) {
        return false;
    }

    const QPointF oldPos = target->pos();
    if (!applyNodePositionInternal(nodeId, snapped, true)) {
        return false;
    }

    if (m_undoStack) {
        m_undoStack->push(new NodeMoveCommand(this, nodeId, oldPos, snapped, true));
    }
    return true;
}

bool EditorScene::setNodePropertyWithUndo(const QString& nodeId, const QString& key, const QString& value) {
    NodeItem* target = findNodeByIdInternal(nodeId);
    if (!target) {
        return false;
    }

    const QString oldValue = target->propertyValue(key);
    if (oldValue == value) {
        return false;
    }

    if (!applyNodePropertyInternal(nodeId, key, value, true)) {
        return false;
    }

    if (m_undoStack) {
        m_undoStack->push(new NodePropertyCommand(this, nodeId, key, oldValue, value, true));
    }
    return true;
}

bool EditorScene::autoLayoutWithUndo(bool selectedOnly) {
    const QVector<NodeItem*> layoutNodes = collectLayoutNodes(selectedOnly);
    if (layoutNodes.size() < 2) {
        return false;
    }

    const GraphDocument before = toDocument();
    if (!applyAutoLayout(layoutNodes)) {
        return false;
    }

    const GraphDocument after = toDocument();
    if (areDocumentsEquivalent(before, after)) {
        return false;
    }

    if (m_undoStack) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Auto Layout"), true));
    }
    return true;
}

bool EditorScene::rotateSelectionWithUndo(qreal deltaDegrees) {
    if (qFuzzyIsNull(deltaDegrees)) {
        return false;
    }

    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selectedNodes.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    bool changed = false;
    for (NodeItem* node : selectedNodes) {
        if (!node) {
            continue;
        }
        const qreal nextRotation = node->rotation() + deltaDegrees;
        if (qFuzzyCompare(node->rotation() + 1.0, nextRotation + 1.0)) {
            continue;
        }
        node->setRotation(nextRotation);
        changed = true;
    }

    if (!changed) {
        return false;
    }

    emit graphChanged();
    onSelectionChangedInternal();

    if (!m_undoStack) {
        return true;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Rotate"), true));
    }
    return true;
}

bool EditorScene::bringSelectionToFrontWithUndo() {
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selectedNodes.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    qreal maxZ = 1.0;
    for (QGraphicsItem* item : items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            maxZ = std::max(maxZ, node->zValue());
        }
    }

    QVector<NodeItem*> ordered = selectedNodes;
    std::sort(ordered.begin(), ordered.end(), [](const NodeItem* a, const NodeItem* b) { return a->zValue() < b->zValue(); });

    bool changed = false;
    qreal next = maxZ + 1.0;
    for (NodeItem* node : ordered) {
        if (!node) {
            continue;
        }
        if (qFuzzyCompare(node->zValue() + 1.0, next + 1.0)) {
            next += 1.0;
            continue;
        }
        node->setZValue(next);
        next += 1.0;
        changed = true;
    }

    if (!changed) {
        return false;
    }

    emit graphChanged();
    if (!m_undoStack) {
        return true;
    }

    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Bring To Front"), true));
    }
    return true;
}

bool EditorScene::sendSelectionToBackWithUndo() {
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selectedNodes.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    qreal minZ = 1.0;
    bool initialized = false;
    for (QGraphicsItem* item : items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            minZ = initialized ? std::min(minZ, node->zValue()) : node->zValue();
            initialized = true;
        }
    }

    QVector<NodeItem*> ordered = selectedNodes;
    std::sort(ordered.begin(), ordered.end(), [](const NodeItem* a, const NodeItem* b) { return a->zValue() < b->zValue(); });

    bool changed = false;
    qreal next = minZ - static_cast<qreal>(ordered.size());
    for (NodeItem* node : ordered) {
        if (!node) {
            continue;
        }
        if (qFuzzyCompare(node->zValue() + 1.0, next + 1.0)) {
            next += 1.0;
            continue;
        }
        node->setZValue(next);
        next += 1.0;
        changed = true;
    }

    if (!changed) {
        return false;
    }

    emit graphChanged();
    if (!m_undoStack) {
        return true;
    }

    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Send To Back"), true));
    }
    return true;
}

bool EditorScene::bringSelectionForwardWithUndo() {
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selectedNodes.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    QVector<NodeItem*> ordered = selectedNodes;
    std::sort(ordered.begin(), ordered.end(), [](const NodeItem* a, const NodeItem* b) { return a->zValue() > b->zValue(); });

    bool changed = false;
    for (NodeItem* node : ordered) {
        if (!node) {
            continue;
        }
        const qreal next = node->zValue() + 1.0;
        if (qFuzzyCompare(node->zValue() + 1.0, next + 1.0)) {
            continue;
        }
        node->setZValue(next);
        changed = true;
    }

    if (!changed) {
        return false;
    }

    emit graphChanged();
    if (!m_undoStack) {
        return true;
    }

    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Bring Forward"), true));
    }
    return true;
}

bool EditorScene::sendSelectionBackwardWithUndo() {
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selectedNodes.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    QVector<NodeItem*> ordered = selectedNodes;
    std::sort(ordered.begin(), ordered.end(), [](const NodeItem* a, const NodeItem* b) { return a->zValue() < b->zValue(); });

    bool changed = false;
    for (NodeItem* node : ordered) {
        if (!node) {
            continue;
        }
        const qreal next = node->zValue() - 1.0;
        if (qFuzzyCompare(node->zValue() + 1.0, next + 1.0)) {
            continue;
        }
        node->setZValue(next);
        changed = true;
    }

    if (!changed) {
        return false;
    }

    emit graphChanged();
    if (!m_undoStack) {
        return true;
    }

    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Send Backward"), true));
    }
    return true;
}

bool EditorScene::groupSelectionWithUndo() {
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selectedNodes.size() < 2) {
        return false;
    }
    for (NodeItem* node : selectedNodes) {
        if (node && !node->groupId().isEmpty()) {
            return false;
        }
    }

    const GraphDocument before = toDocument();
    const QString groupId = nextGroupId();
    for (NodeItem* node : selectedNodes) {
        if (node) {
            node->setGroupId(groupId);
        }
    }

    rebuildNodeGroups();
    clearSelection();
    if (QGraphicsItemGroup* groupItem = m_nodeGroups.value(groupId, nullptr)) {
        groupItem->setSelected(true);
    }
    emit graphChanged();
    onSelectionChangedInternal();

    if (!m_undoStack) {
        return true;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Group"), true));
    }
    return true;
}

bool EditorScene::ungroupSelectionWithUndo() {
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    QSet<QString> targetGroupIds;
    for (NodeItem* node : selectedNodes) {
        if (node && !node->groupId().isEmpty()) {
            targetGroupIds.insert(node->groupId());
        }
    }
    if (targetGroupIds.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    bool changed = false;
    for (QGraphicsItem* item : items()) {
        NodeItem* node = dynamic_cast<NodeItem*>(item);
        if (!node || !targetGroupIds.contains(node->groupId())) {
            continue;
        }
        node->setGroupId(QString());
        changed = true;
    }
    for (const QString& groupId : targetGroupIds) {
        m_collapsedGroups.remove(groupId);
    }
    if (!changed) {
        return false;
    }

    rebuildNodeGroups();
    refreshCollapsedVisibility();
    emit graphChanged();
    onSelectionChangedInternal();

    if (!m_undoStack) {
        return true;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Ungroup"), true));
    }
    return true;
}

bool EditorScene::collapseSelectionWithUndo() {
    const QSet<QString> selectedGroups = collectSelectedGroupIds();
    if (selectedGroups.isEmpty()) {
        return false;
    }

    QSet<QString> toCollapse;
    for (const QString& groupId : selectedGroups) {
        if (!m_collapsedGroups.contains(groupId)) {
            toCollapse.insert(groupId);
        }
    }
    if (toCollapse.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    for (const QString& groupId : toCollapse) {
        m_collapsedGroups.insert(groupId);
    }
    refreshCollapsedVisibility();
    emit graphChanged();
    onSelectionChangedInternal();

    if (!m_undoStack) {
        return true;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Collapse Group"), true));
    }
    return true;
}

bool EditorScene::expandSelectionWithUndo() {
    const QSet<QString> selectedGroups = collectSelectedGroupIds();
    if (selectedGroups.isEmpty()) {
        return false;
    }

    QSet<QString> toExpand;
    for (const QString& groupId : selectedGroups) {
        if (m_collapsedGroups.contains(groupId)) {
            toExpand.insert(groupId);
        }
    }
    if (toExpand.isEmpty()) {
        return false;
    }

    const GraphDocument before = toDocument();
    for (const QString& groupId : toExpand) {
        m_collapsedGroups.remove(groupId);
    }
    refreshCollapsedVisibility();
    emit graphChanged();
    onSelectionChangedInternal();

    if (!m_undoStack) {
        return true;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Expand Group"), true));
    }
    return true;
}

void EditorScene::deleteSelectionWithUndo() {
    const QList<QGraphicsItem*> selected = selectedItems();
    const QVector<NodeItem*> selectedNodes = collectSelectedNodes();
    if (selected.isEmpty() && selectedNodes.isEmpty()) {
        return;
    }

    const GraphDocument before = toDocument();

    QSet<QString> deletedNodeIds;
    for (NodeItem* node : selectedNodes) {
        if (node) {
            deletedNodeIds.insert(node->nodeId());
        }
    }

    QSet<QGraphicsItem*> toDelete;
    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            if (!edge->sourcePort() || !edge->targetPort()) {
                continue;
            }
            const NodeItem* fromNode = edge->sourcePort()->ownerNode();
            const NodeItem* toNode = edge->targetPort()->ownerNode();
            if ((fromNode && deletedNodeIds.contains(fromNode->nodeId())) ||
                (toNode && deletedNodeIds.contains(toNode->nodeId())) || edge->isSelected()) {
                toDelete.insert(edge);
            }
        }
    }
    for (NodeItem* node : selectedNodes) {
        if (node) {
            toDelete.insert(node);
        }
    }

    const QList<QGraphicsItem*> toDeleteList = toDelete.values();
    for (QGraphicsItem* item : toDeleteList) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            m_nodeGroups.remove(node->groupId());
        }
    }
    for (QGraphicsItem* item : toDelete) {
        removeItem(item);
        delete item;
    }
    for (QGraphicsItem* item : items()) {
        if (EdgeItem* existing = dynamic_cast<EdgeItem*>(item)) {
            existing->updatePath();
        }
    }

    rebuildNodeGroups();

    emit graphChanged();

    if (!m_undoStack) {
        return;
    }
    const GraphDocument after = toDocument();
    if (!areDocumentsEquivalent(before, after)) {
        m_undoStack->push(new DocumentStateCommand(this, before, after, QStringLiteral("Delete"), true));
    }
}

EdgeItem* EditorScene::createEdgeFromData(const EdgeData& edgeData) {
    PortItem* outPort = nullptr;
    PortItem* inPort = nullptr;

    for (QGraphicsItem* item : items()) {
        PortItem* port = dynamic_cast<PortItem*>(item);
        if (!port) {
            continue;
        }
        if (port->portId() == edgeData.fromPortId) {
            outPort = port;
        } else if (port->portId() == edgeData.toPortId) {
            inPort = port;
        }
    }

    if (!outPort || !inPort) {
        return nullptr;
    }

    EdgeItem* edge = new EdgeItem(edgeData.id, outPort);
    edge->setRoutingMode(m_edgeRoutingMode);
    edge->setBundlePolicy(m_edgeBundlePolicy);
    edge->setBundleSpacing(m_edgeBundleSpacing);
    edge->setTargetPort(inPort);
    addItem(edge);
    const NodeItem* sourceNode = outPort->ownerNode();
    const NodeItem* targetNode = inPort->ownerNode();
    if (sourceNode && targetNode) {
        for (QGraphicsItem* item : items()) {
            if (EdgeItem* existing = dynamic_cast<EdgeItem*>(item)) {
                if (!existing->sourcePort() || !existing->targetPort()) {
                    continue;
                }
                const NodeItem* existingSource = existing->sourcePort()->ownerNode();
                const NodeItem* existingTarget = existing->targetPort()->ownerNode();
                if (!existingSource || !existingTarget) {
                    continue;
                }
                if (existingSource->nodeId() == sourceNode->nodeId() && existingTarget->nodeId() == targetNode->nodeId()) {
                    existing->updatePath();
                }
            }
        }
    }
    updateCounterFromId(edgeData.id, &m_edgeCounter);
    return edge;
}

void EditorScene::clearGraph() {
    if (m_previewEdge) {
        removeItem(m_previewEdge);
        delete m_previewEdge;
        m_previewEdge = nullptr;
    }
    m_pendingPort = nullptr;
    m_draggingGroup = nullptr;
    m_draggingGroupTracked = false;

    clearNodeGroups();

    clear();
    m_nodeCounter = 1;
    m_portCounter = 1;
    m_edgeCounter = 1;
    m_groupCounter = 1;
    m_collapsedGroups.clear();
    emit graphChanged();
}

GraphDocument EditorScene::toDocument() const {
    GraphDocument doc;
    doc.schemaVersion = 1;
    doc.autoLayoutMode = (m_autoLayoutMode == AutoLayoutMode::Grid) ? QStringLiteral("grid") : QStringLiteral("layered");
    doc.autoLayoutXSpacing = m_autoLayoutHorizontalSpacing;
    doc.autoLayoutYSpacing = m_autoLayoutVerticalSpacing;
    doc.edgeBundlePolicy =
        (m_edgeBundlePolicy == EdgeBundlePolicy::Directional) ? QStringLiteral("directional") : QStringLiteral("centered");
    doc.edgeBundleSpacing = m_edgeBundleSpacing;
    doc.collapsedGroupIds = m_collapsedGroups.values().toVector();

    for (QGraphicsItem* item : items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            NodeData nodeData;
            nodeData.id = node->nodeId();
            nodeData.type = node->typeName();
            nodeData.name = node->displayName();
            nodeData.position = node->scenePos();
            nodeData.size = node->nodeSize();
            nodeData.rotationDegrees = node->rotation();
            nodeData.z = node->zValue();
            nodeData.groupId = node->groupId();

            for (PortItem* port : node->inputPorts()) {
                PortData p;
                p.id = port->portId();
                p.name = port->portName();
                p.direction = QStringLiteral("input");
                nodeData.ports.append(p);
            }
            for (PortItem* port : node->outputPorts()) {
                PortData p;
                p.id = port->portId();
                p.name = port->portName();
                p.direction = QStringLiteral("output");
                nodeData.ports.append(p);
            }
            nodeData.properties = node->properties();
            doc.nodes.append(nodeData);
        }
    }

    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            if (!edge->sourcePort() || !edge->targetPort()) {
                continue;
            }
            EdgeData e;
            e.id = edge->edgeId();
            e.fromNodeId = edge->sourcePort()->ownerNode() ? edge->sourcePort()->ownerNode()->nodeId() : QString();
            e.fromPortId = edge->sourcePort()->portId();
            e.toNodeId = edge->targetPort()->ownerNode() ? edge->targetPort()->ownerNode()->nodeId() : QString();
            e.toPortId = edge->targetPort()->portId();
            doc.edges.append(e);
        }
    }

    std::sort(doc.nodes.begin(), doc.nodes.end(), [](const NodeData& a, const NodeData& b) { return a.id < b.id; });
    std::sort(doc.edges.begin(), doc.edges.end(), [](const EdgeData& a, const EdgeData& b) { return a.id < b.id; });
    for (NodeData& node : doc.nodes) {
        std::sort(node.ports.begin(), node.ports.end(), [](const PortData& a, const PortData& b) { return a.id < b.id; });
        std::sort(node.properties.begin(),
                  node.properties.end(),
                  [](const PropertyData& a, const PropertyData& b) { return a.key < b.key; });
    }
    std::sort(doc.collapsedGroupIds.begin(), doc.collapsedGroupIds.end());

    return doc;
}

bool EditorScene::fromDocument(const GraphDocument& document) {
    clearGraph();
    m_autoLayoutMode = document.autoLayoutMode.compare(QStringLiteral("grid"), Qt::CaseInsensitive) == 0
        ? AutoLayoutMode::Grid
        : AutoLayoutMode::Layered;
    m_autoLayoutHorizontalSpacing = std::max<qreal>(40.0, document.autoLayoutXSpacing);
    m_autoLayoutVerticalSpacing = std::max<qreal>(40.0, document.autoLayoutYSpacing);
    m_edgeBundlePolicy = document.edgeBundlePolicy.compare(QStringLiteral("directional"), Qt::CaseInsensitive) == 0
        ? EdgeBundlePolicy::Directional
        : EdgeBundlePolicy::Centered;
    m_edgeBundleSpacing = std::max<qreal>(0.0, document.edgeBundleSpacing);
    m_collapsedGroups = QSet<QString>(document.collapsedGroupIds.begin(), document.collapsedGroupIds.end());

    for (const NodeData& node : document.nodes) {
        if (!createNodeFromData(node)) {
            return false;
        }
    }
    for (const EdgeData& edge : document.edges) {
        createEdgeFromData(edge);
    }

    rebuildNodeGroups();
    refreshCollapsedVisibility();

    emit graphChanged();
    return true;
}

void EditorScene::setSnapToGrid(bool enabled) {
    m_snapToGrid = enabled;
}

bool EditorScene::snapToGrid() const {
    return m_snapToGrid;
}

int EditorScene::gridSize() const {
    return 20;
}

void EditorScene::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;
}

void EditorScene::setInteractionMode(InteractionMode mode) {
    m_mode = mode;
    if (m_mode != InteractionMode::Connect && m_previewEdge) {
        removeItem(m_previewEdge);
        delete m_previewEdge;
        m_previewEdge = nullptr;
        m_pendingPort = nullptr;
        emit connectionStateChanged(false);
    }
}

InteractionMode EditorScene::interactionMode() const {
    return m_mode;
}

void EditorScene::setPlacementType(const QString& typeName) {
    m_placementType = typeName;
}

void EditorScene::setEdgeRoutingMode(EdgeRoutingMode mode) {
    if (m_edgeRoutingMode == mode) {
        return;
    }
    m_edgeRoutingMode = mode;
    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            edge->setRoutingMode(mode);
        }
    }
    emit graphChanged();
}

EdgeRoutingMode EditorScene::edgeRoutingMode() const {
    return m_edgeRoutingMode;
}

void EditorScene::setEdgeBundlePolicy(EdgeBundlePolicy policy) {
    if (m_edgeBundlePolicy == policy) {
        return;
    }
    m_edgeBundlePolicy = policy;
    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            edge->setBundlePolicy(policy);
        }
    }
    emit graphChanged();
}

EdgeBundlePolicy EditorScene::edgeBundlePolicy() const {
    return m_edgeBundlePolicy;
}

void EditorScene::setEdgeBundleSpacing(qreal spacing) {
    const qreal clamped = std::max<qreal>(0.0, spacing);
    if (qFuzzyCompare(m_edgeBundleSpacing + 1.0, clamped + 1.0)) {
        return;
    }
    m_edgeBundleSpacing = clamped;
    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            edge->setBundleSpacing(clamped);
        }
    }
    emit graphChanged();
}

qreal EditorScene::edgeBundleSpacing() const {
    return m_edgeBundleSpacing;
}

void EditorScene::setAutoLayoutMode(AutoLayoutMode mode) {
    if (m_autoLayoutMode == mode) {
        return;
    }
    m_autoLayoutMode = mode;
    emit graphChanged();
}

AutoLayoutMode EditorScene::autoLayoutMode() const {
    return m_autoLayoutMode;
}

void EditorScene::setAutoLayoutSpacing(qreal horizontal, qreal vertical) {
    constexpr qreal kMinSpacing = 40.0;
    const qreal nextHorizontal = std::max(kMinSpacing, horizontal);
    const qreal nextVertical = std::max(kMinSpacing, vertical);
    if (qFuzzyCompare(m_autoLayoutHorizontalSpacing + 1.0, nextHorizontal + 1.0) &&
        qFuzzyCompare(m_autoLayoutVerticalSpacing + 1.0, nextVertical + 1.0)) {
        return;
    }
    m_autoLayoutHorizontalSpacing = nextHorizontal;
    m_autoLayoutVerticalSpacing = nextVertical;
    emit graphChanged();
}

qreal EditorScene::autoLayoutHorizontalSpacing() const {
    return m_autoLayoutHorizontalSpacing;
}

qreal EditorScene::autoLayoutVerticalSpacing() const {
    return m_autoLayoutVerticalSpacing;
}

void EditorScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    m_draggingGroup = nullptr;
    m_draggingGroupTracked = false;
    if (event->button() == Qt::LeftButton) {
        const QTransform viewTransform = views().isEmpty() ? QTransform() : views().first()->transform();
        if (QGraphicsItemGroup* group = owningGroupItem(itemAt(event->scenePos(), viewTransform))) {
            m_draggingGroup = group;
            m_draggingGroupStartPos = group->scenePos();
            if (m_undoStack) {
                m_draggingGroupBefore = toDocument();
                m_draggingGroupTracked = true;
            }
        }
    }

    if (event->button() == Qt::LeftButton && m_mode == InteractionMode::Place && !m_placementType.isEmpty()) {
        createNodeWithUndo(m_placementType, event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void EditorScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_previewEdge) {
        m_previewEdge->setPreviewEnd(event->scenePos());
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void EditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    bool connectionHandled = false;
    if (event->button() == Qt::LeftButton && m_pendingPort) {
        finishConnectionAt(event->scenePos());
        event->accept();
        connectionHandled = true;
    } else {
        QGraphicsScene::mouseReleaseEvent(event);
    }

    if (event->button() == Qt::LeftButton && m_draggingGroup) {
        const bool moved = m_draggingGroup->scenePos() != m_draggingGroupStartPos;
        if (moved) {
            emit graphChanged();
            if (m_undoStack && m_draggingGroupTracked) {
                const GraphDocument after = toDocument();
                if (!areDocumentsEquivalent(m_draggingGroupBefore, after)) {
                    m_undoStack->push(
                        new DocumentStateCommand(this, m_draggingGroupBefore, after, QStringLiteral("Move Group"), true));
                }
            }
        }
    }
    m_draggingGroup = nullptr;
    m_draggingGroupTracked = false;

    if (connectionHandled) {
        return;
    }
}

void EditorScene::onPortConnectionStart(PortItem* port) {
    if (!port) {
        return;
    }
    if (m_mode != InteractionMode::Select && m_mode != InteractionMode::Connect) {
        return;
    }

    m_pendingPort = port;
    if (m_previewEdge) {
        removeItem(m_previewEdge);
        delete m_previewEdge;
        m_previewEdge = nullptr;
    }

    m_previewEdge = new EdgeItem(QStringLiteral("__preview__"), port);
    m_previewEdge->setRoutingMode(m_edgeRoutingMode);
    addItem(m_previewEdge);
    emit connectionStateChanged(true);
}

void EditorScene::onPortConnectionRelease(PortItem* port) {
    if (!m_pendingPort) {
        return;
    }
    finishConnectionAt(port ? port->scenePos() : QPointF(), port);
}

void EditorScene::onSelectionChangedInternal() {
    const QList<QGraphicsItem*> selected = selectedItems();
    if (selected.isEmpty()) {
        emit selectionInfoChanged(QString(), QString(), QString(), QPointF(), 0, 0);
        return;
    }

    if (NodeItem* node = dynamic_cast<NodeItem*>(selected.first())) {
        emit selectionInfoChanged(QStringLiteral("node"),
                                  node->nodeId(),
                                  node->displayName(),
                                  node->scenePos(),
                                  node->inputPorts().size(),
                                  node->outputPorts().size());
        return;
    }

    if (EdgeItem* edge = dynamic_cast<EdgeItem*>(selected.first())) {
        emit selectionInfoChanged(QStringLiteral("edge"),
                                  edge->edgeId(),
                                  QStringLiteral("edge"),
                                  QPointF(),
                                  0,
                                  0);
        return;
    }

    emit selectionInfoChanged(QString(), QString(), QString(), QPointF(), 0, 0);
}

void EditorScene::onNodeDragFinished(NodeItem* node, const QPointF& oldPos, const QPointF& newPos) {
    if (!node) {
        return;
    }
    const QPointF snapped = snapPoint(newPos);
    if (oldPos == snapped) {
        return;
    }

    if (node->pos() != snapped) {
        applyNodePositionInternal(node->nodeId(), snapped, true);
    } else {
        emit graphChanged();
    }

    if (m_undoStack) {
        m_undoStack->push(new NodeMoveCommand(this, node->nodeId(), oldPos, snapped, true));
    }
}

QString EditorScene::nextNodeId() {
    return QStringLiteral("N_%1").arg(m_nodeCounter++);
}

QString EditorScene::nextPortId() {
    return QStringLiteral("P_%1").arg(m_portCounter++);
}

QString EditorScene::nextEdgeId() {
    return QStringLiteral("E_%1").arg(m_edgeCounter++);
}

QString EditorScene::nextGroupId() {
    return QStringLiteral("G_%1").arg(m_groupCounter++);
}

void EditorScene::updateCounterFromId(const QString& id, int* counter) {
    if (!counter) {
        return;
    }
    const QRegularExpression re(QStringLiteral("^[A-Z]_(\\d+)$"));
    const QRegularExpressionMatch match = re.match(id);
    if (!match.hasMatch()) {
        return;
    }
    bool ok = false;
    const int index = match.captured(1).toInt(&ok);
    if (ok) {
        *counter = std::max(*counter, index + 1);
    }
}

NodeItem* EditorScene::findNodeByIdInternal(const QString& nodeId) const {
    if (nodeId.isEmpty()) {
        return nullptr;
    }
    for (QGraphicsItem* item : items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            if (node->nodeId() == nodeId) {
                return node;
            }
        }
    }
    return nullptr;
}

bool EditorScene::applyNodeRenameInternal(const QString& nodeId, const QString& newName, bool emitGraphChangedFlag) {
    NodeItem* target = findNodeByIdInternal(nodeId);
    if (!target || target->displayName() == newName) {
        return false;
    }
    target->setDisplayName(newName);
    if (emitGraphChangedFlag) {
        emit graphChanged();
    }
    if (target->isSelected()) {
        onSelectionChangedInternal();
    }
    return true;
}

bool EditorScene::applyNodePositionInternal(const QString& nodeId, const QPointF& newPos, bool emitGraphChangedFlag) {
    NodeItem* target = findNodeByIdInternal(nodeId);
    if (!target || target->pos() == newPos) {
        return false;
    }
    target->setPos(newPos);
    if (emitGraphChangedFlag) {
        emit graphChanged();
    }
    if (target->isSelected()) {
        onSelectionChangedInternal();
    }
    return true;
}

bool EditorScene::applyNodePropertyInternal(const QString& nodeId,
                                            const QString& key,
                                            const QString& value,
                                            bool emitGraphChangedFlag) {
    NodeItem* target = findNodeByIdInternal(nodeId);
    if (!target || !target->setPropertyValue(key, value)) {
        return false;
    }
    if (emitGraphChangedFlag) {
        emit graphChanged();
    }
    if (target->isSelected()) {
        onSelectionChangedInternal();
    }
    return true;
}

QPointF EditorScene::snapPoint(const QPointF& p) const {
    if (!m_snapToGrid) {
        return p;
    }
    const int g = gridSize();
    return QPointF(std::round(p.x() / g) * g, std::round(p.y() / g) * g);
}

bool EditorScene::canConnect(PortItem* a, PortItem* b) const {
    if (!a || !b || a == b) {
        return false;
    }
    if (a->ownerNode() == b->ownerNode()) {
        return false;
    }

    PortItem* outputPort = nullptr;
    PortItem* inputPort = nullptr;
    if (a->direction() == PortDirection::Output && b->direction() == PortDirection::Input) {
        outputPort = a;
        inputPort = b;
    } else if (a->direction() == PortDirection::Input && b->direction() == PortDirection::Output) {
        outputPort = b;
        inputPort = a;
    } else {
        return false;
    }

    if (hasEdgeBetweenPorts(outputPort, inputPort)) {
        return false;
    }
    if (inputPortHasConnection(inputPort)) {
        return false;
    }
    return true;
}

bool EditorScene::hasEdgeBetweenPorts(PortItem* outputPort, PortItem* inputPort) const {
    if (!outputPort || !inputPort) {
        return false;
    }

    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            if (edge->sourcePort() == outputPort && edge->targetPort() == inputPort) {
                return true;
            }
        }
    }
    return false;
}

bool EditorScene::inputPortHasConnection(PortItem* inputPort) const {
    if (!inputPort) {
        return false;
    }

    for (QGraphicsItem* item : items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            if (edge->targetPort() == inputPort && edge->sourcePort()) {
                return true;
            }
        }
    }
    return false;
}

PortItem* EditorScene::pickPortAt(const QPointF& scenePos) const {
    const QTransform viewTransform = views().isEmpty() ? QTransform() : views().first()->transform();
    const QList<QGraphicsItem*> itemsAtPos = items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, viewTransform);
    for (QGraphicsItem* item : itemsAtPos) {
        if (PortItem* port = dynamic_cast<PortItem*>(item)) {
            return port;
        }
    }
    return nullptr;
}

QVector<NodeItem*> EditorScene::collectSelectedNodes() const {
    QVector<NodeItem*> selectedNodes;
    QSet<QString> seenNodeIds;
    const QList<QGraphicsItem*> selected = selectedItems();
    selectedNodes.reserve(selected.size());

    auto appendNode = [&selectedNodes, &seenNodeIds](NodeItem* node) {
        if (!node || seenNodeIds.contains(node->nodeId())) {
            return;
        }
        selectedNodes.push_back(node);
        seenNodeIds.insert(node->nodeId());
    };

    for (QGraphicsItem* item : selected) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            appendNode(node);
            continue;
        }
        if (QGraphicsItemGroup* group = dynamic_cast<QGraphicsItemGroup*>(item)) {
            const QList<QGraphicsItem*> children = group->childItems();
            for (QGraphicsItem* child : children) {
                appendNode(dynamic_cast<NodeItem*>(child));
            }
        }
    }
    std::sort(selectedNodes.begin(),
              selectedNodes.end(),
              [](const NodeItem* a, const NodeItem* b) { return a->nodeId() < b->nodeId(); });
    return selectedNodes;
}

QVector<NodeItem*> EditorScene::collectLayoutNodes(bool selectedOnly) const {
    QVector<NodeItem*> selectedNodes;
    if (selectedOnly) {
        selectedNodes = collectSelectedNodes();
    }

    QVector<NodeItem*> nodes;
    if (selectedNodes.size() >= 2) {
        nodes = selectedNodes;
    } else {
        const QList<QGraphicsItem*> sceneItems = items();
        nodes.reserve(sceneItems.size());
        for (QGraphicsItem* item : sceneItems) {
            if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
                nodes.push_back(node);
            }
        }
    }

    std::sort(nodes.begin(), nodes.end(), [](const NodeItem* a, const NodeItem* b) { return a->nodeId() < b->nodeId(); });
    return nodes;
}

void EditorScene::clearNodeGroups() {
    const QList<QGraphicsItemGroup*> groups = m_nodeGroups.values();
    for (QGraphicsItemGroup* group : groups) {
        if (!group) {
            continue;
        }
        const QList<QGraphicsItem*> children = group->childItems();
        for (QGraphicsItem* child : children) {
            if (dynamic_cast<NodeItem*>(child)) {
                child->setParentItem(nullptr);
            }
        }
        if (group->scene() == this) {
            removeItem(group);
        }
        delete group;
    }
    m_nodeGroups.clear();
    m_draggingGroup = nullptr;
    m_draggingGroupTracked = false;
}

void EditorScene::rebuildNodeGroups() {
    clearNodeGroups();

    QHash<QString, QVector<NodeItem*>> groupedNodes;
    for (QGraphicsItem* item : items()) {
        NodeItem* node = dynamic_cast<NodeItem*>(item);
        if (!node || node->groupId().isEmpty()) {
            continue;
        }
        groupedNodes[node->groupId()].push_back(node);
    }

    QSet<QString> validGroupIds;
    for (auto it = groupedNodes.begin(); it != groupedNodes.end();) {
        if (it.value().size() < 2) {
            for (NodeItem* node : it.value()) {
                if (node) {
                    node->setGroupId(QString());
                }
            }
            it = groupedNodes.erase(it);
            continue;
        }
        validGroupIds.insert(it.key());
        std::sort(it.value().begin(), it.value().end(), [](const NodeItem* a, const NodeItem* b) {
            return a->nodeId() < b->nodeId();
        });
        ++it;
    }

    for (auto it = m_collapsedGroups.begin(); it != m_collapsedGroups.end();) {
        if (!validGroupIds.contains(*it)) {
            it = m_collapsedGroups.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = groupedNodes.begin(); it != groupedNodes.end(); ++it) {
        const QString& groupId = it.key();
        const QVector<NodeItem*>& nodes = it.value();
        QList<QGraphicsItem*> members;
        members.reserve(nodes.size());
        for (NodeItem* node : nodes) {
            members.push_back(node);
        }

        QGraphicsItemGroup* group = createItemGroup(members);
        group->setHandlesChildEvents(false);
        group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        group->setFlag(QGraphicsItem::ItemIsMovable, true);
        group->setData(0, groupId);

        const QRectF contentBounds = group->childrenBoundingRect();
        const QRectF frameRect = contentBounds.adjusted(-14.0, -28.0, 14.0, 14.0);

        auto* frame = new QGraphicsRectItem(frameRect, group);
        frame->setPen(QPen(QColor(80, 120, 190), 1.2, Qt::DashLine));
        frame->setBrush(QColor(170, 195, 235, 24));
        frame->setAcceptedMouseButtons(Qt::NoButton);
        frame->setFlag(QGraphicsItem::ItemIsSelectable, false);
        frame->setFlag(QGraphicsItem::ItemIsMovable, false);
        frame->setZValue(-1000.0);
        frame->setData(0, QStringLiteral("group_frame"));

        auto* title = new QGraphicsSimpleTextItem(QStringLiteral("Group %1").arg(groupId), group);
        title->setBrush(QColor(64, 94, 146));
        title->setPos(frameRect.left() + 8.0, frameRect.top() + 4.0);
        title->setAcceptedMouseButtons(Qt::NoButton);
        title->setFlag(QGraphicsItem::ItemIsSelectable, false);
        title->setFlag(QGraphicsItem::ItemIsMovable, false);
        title->setZValue(-999.0);
        title->setData(0, QStringLiteral("group_title"));

        m_nodeGroups.insert(groupId, group);
        updateCounterFromId(groupId, &m_groupCounter);
    }

    refreshCollapsedVisibility();
}

QGraphicsItemGroup* EditorScene::owningGroupItem(QGraphicsItem* item) const {
    QGraphicsItem* cursor = item;
    const QList<QGraphicsItemGroup*> groups = m_nodeGroups.values();
    while (cursor) {
        if (QGraphicsItemGroup* group = dynamic_cast<QGraphicsItemGroup*>(cursor)) {
            if (groups.contains(group)) {
                return group;
            }
        }
        cursor = cursor->parentItem();
    }
    return nullptr;
}

QSet<QString> EditorScene::collectSelectedGroupIds() const {
    QSet<QString> groupIds;
    const QList<QGraphicsItem*> selected = selectedItems();
    const QList<QGraphicsItemGroup*> knownGroups = m_nodeGroups.values();
    for (QGraphicsItem* item : selected) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            if (!node->groupId().isEmpty()) {
                groupIds.insert(node->groupId());
            }
            continue;
        }
        if (QGraphicsItemGroup* group = dynamic_cast<QGraphicsItemGroup*>(item)) {
            if (knownGroups.contains(group)) {
                const QString groupId = group->data(0).toString();
                if (!groupId.isEmpty()) {
                    groupIds.insert(groupId);
                }
            }
        }
    }
    return groupIds;
}

void EditorScene::refreshCollapsedVisibility() {
    QSet<QString> collapsedNodeIds;
    for (QGraphicsItem* item : items()) {
        NodeItem* node = dynamic_cast<NodeItem*>(item);
        if (!node) {
            continue;
        }
        const bool collapsed = !node->groupId().isEmpty() && m_collapsedGroups.contains(node->groupId());
        node->setVisible(!collapsed);
        if (collapsed && node->isSelected()) {
            node->setSelected(false);
        }
        if (collapsed) {
            collapsedNodeIds.insert(node->nodeId());
        }
    }

    for (QGraphicsItem* item : items()) {
        EdgeItem* edge = dynamic_cast<EdgeItem*>(item);
        if (!edge || !edge->sourcePort() || !edge->targetPort()) {
            continue;
        }
        const NodeItem* sourceNode = edge->sourcePort()->ownerNode();
        const NodeItem* targetNode = edge->targetPort()->ownerNode();
        if (!sourceNode || !targetNode) {
            continue;
        }
        const bool hidden = collapsedNodeIds.contains(sourceNode->nodeId()) || collapsedNodeIds.contains(targetNode->nodeId());
        edge->setVisible(!hidden);
        if (hidden && edge->isSelected()) {
            edge->setSelected(false);
        }
    }

    for (auto it = m_nodeGroups.constBegin(); it != m_nodeGroups.constEnd(); ++it) {
        const QString& groupId = it.key();
        QGraphicsItemGroup* group = it.value();
        if (!group) {
            continue;
        }
        const bool collapsed = m_collapsedGroups.contains(groupId);
        const QList<QGraphicsItem*> children = group->childItems();
        for (QGraphicsItem* child : children) {
            const QString tag = child->data(0).toString();
            if (tag == QStringLiteral("group_frame")) {
                if (QGraphicsRectItem* frame = dynamic_cast<QGraphicsRectItem*>(child)) {
                    frame->setPen(QPen(collapsed ? QColor(32, 92, 182) : QColor(80, 120, 190),
                                       collapsed ? 1.6 : 1.2,
                                       Qt::DashLine));
                    frame->setBrush(collapsed ? QColor(145, 181, 233, 52) : QColor(170, 195, 235, 24));
                }
            } else if (tag == QStringLiteral("group_title")) {
                if (QGraphicsSimpleTextItem* title = dynamic_cast<QGraphicsSimpleTextItem*>(child)) {
                    title->setText(collapsed ? QStringLiteral("Group %1 (collapsed)").arg(groupId)
                                             : QStringLiteral("Group %1").arg(groupId));
                }
            }
        }
    }
}

bool EditorScene::applyAutoLayout(const QVector<NodeItem*>& nodes) {
    if (nodes.size() < 2) {
        return false;
    }

    const qreal xSpacing = std::max<qreal>(40.0, m_autoLayoutHorizontalSpacing);
    const qreal ySpacing = std::max<qreal>(40.0, m_autoLayoutVerticalSpacing);

    bool changed = false;
    if (m_autoLayoutMode == AutoLayoutMode::Grid) {
        changed = applyGridAutoLayout(nodes, xSpacing, ySpacing);
    } else {
        changed = applyLayeredAutoLayout(nodes, xSpacing, ySpacing);
    }
    if (!changed) {
        return false;
    }

    emit graphChanged();
    if (!selectedItems().isEmpty()) {
        onSelectionChangedInternal();
    }
    return true;
}

bool EditorScene::applyLayeredAutoLayout(const QVector<NodeItem*>& nodes, qreal xSpacing, qreal ySpacing) {
    QRectF oldBounds;
    bool oldBoundsInitialized = false;
    QHash<NodeItem*, int> nodeIndex;
    nodeIndex.reserve(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        NodeItem* node = nodes[i];
        if (!node) {
            continue;
        }
        nodeIndex.insert(node, i);
        const QRectF rect = node->sceneBoundingRect();
        oldBounds = oldBoundsInitialized ? oldBounds.united(rect) : rect;
        oldBoundsInitialized = true;
    }
    if (!oldBoundsInitialized || nodeIndex.size() < 2) {
        return false;
    }

    QVector<QVector<int>> outgoing(nodes.size());
    QVector<int> indegree(nodes.size(), 0);
    QSet<quint64> edgeDedup;

    for (QGraphicsItem* item : items()) {
        EdgeItem* edge = dynamic_cast<EdgeItem*>(item);
        if (!edge || !edge->sourcePort() || !edge->targetPort()) {
            continue;
        }
        NodeItem* fromNode = edge->sourcePort()->ownerNode();
        NodeItem* toNode = edge->targetPort()->ownerNode();
        if (!fromNode || !toNode || fromNode == toNode) {
            continue;
        }
        const int fromIndex = nodeIndex.value(fromNode, -1);
        const int toIndex = nodeIndex.value(toNode, -1);
        if (fromIndex < 0 || toIndex < 0) {
            continue;
        }

        const quint64 key = (static_cast<quint64>(static_cast<quint32>(fromIndex)) << 32) |
                            static_cast<quint32>(toIndex);
        if (edgeDedup.contains(key)) {
            continue;
        }
        edgeDedup.insert(key);

        outgoing[fromIndex].push_back(toIndex);
        ++indegree[toIndex];
    }

    auto compareNodeOrder = [&nodes](int lhs, int rhs) {
        const QPointF a = nodes[lhs]->pos();
        const QPointF b = nodes[rhs]->pos();
        if (!qFuzzyCompare(a.y() + 1.0, b.y() + 1.0)) {
            return a.y() < b.y();
        }
        if (!qFuzzyCompare(a.x() + 1.0, b.x() + 1.0)) {
            return a.x() < b.x();
        }
        return nodes[lhs]->nodeId() < nodes[rhs]->nodeId();
    };

    QVector<int> frontier;
    frontier.reserve(nodes.size());
    for (int i = 0; i < indegree.size(); ++i) {
        if (indegree[i] == 0) {
            frontier.push_back(i);
        }
    }
    std::sort(frontier.begin(), frontier.end(), compareNodeOrder);

    QVector<int> layer(nodes.size(), 0);
    QVector<bool> processed(nodes.size(), false);

    int head = 0;
    int processedCount = 0;
    while (head < frontier.size()) {
        const int current = frontier[head++];
        if (processed[current]) {
            continue;
        }
        processed[current] = true;
        ++processedCount;

        for (int next : outgoing[current]) {
            layer[next] = std::max(layer[next], layer[current] + 1);
            --indegree[next];
            if (indegree[next] == 0) {
                frontier.push_back(next);
            }
        }
    }

    int maxLayer = 0;
    for (int value : layer) {
        maxLayer = std::max(maxLayer, value);
    }

    if (processedCount < nodes.size()) {
        QVector<int> remaining;
        remaining.reserve(nodes.size() - processedCount);
        for (int i = 0; i < processed.size(); ++i) {
            if (!processed[i]) {
                remaining.push_back(i);
            }
        }
        std::sort(remaining.begin(), remaining.end(), compareNodeOrder);
        for (int i = 0; i < remaining.size(); ++i) {
            layer[remaining[i]] = maxLayer + i + 1;
        }
    }

    QMap<int, QVector<int>> layerBuckets;
    for (int i = 0; i < layer.size(); ++i) {
        layerBuckets[layer[i]].push_back(i);
    }

    QVector<QPointF> targetPositions(nodes.size(), QPointF());
    QRectF newBounds;
    bool newBoundsInitialized = false;

    int column = 0;
    for (auto it = layerBuckets.begin(); it != layerBuckets.end(); ++it, ++column) {
        QVector<int>& bucket = it.value();
        std::sort(bucket.begin(), bucket.end(), compareNodeOrder);
        for (int row = 0; row < bucket.size(); ++row) {
            const int nodeIdx = bucket[row];
            const QPointF target(column * xSpacing, row * ySpacing);
            targetPositions[nodeIdx] = target;

            const QRectF nodeRect(target, nodes[nodeIdx]->nodeSize());
            newBounds = newBoundsInitialized ? newBounds.united(nodeRect) : nodeRect;
            newBoundsInitialized = true;
        }
    }

    if (!newBoundsInitialized) {
        return false;
    }

    const QPointF translation = oldBounds.center() - newBounds.center();
    bool changed = false;
    for (int i = 0; i < nodes.size(); ++i) {
        QPointF finalPos = targetPositions[i] + translation;
        if (m_snapToGrid) {
            finalPos = snapPoint(finalPos);
        }
        if (nodes[i]->pos() == finalPos) {
            continue;
        }
        nodes[i]->setPos(finalPos);
        changed = true;
    }
    return changed;
}

bool EditorScene::applyGridAutoLayout(const QVector<NodeItem*>& nodes, qreal xSpacing, qreal ySpacing) {
    QRectF oldBounds;
    bool oldBoundsInitialized = false;
    for (NodeItem* node : nodes) {
        if (!node) {
            continue;
        }
        const QRectF rect = node->sceneBoundingRect();
        oldBounds = oldBoundsInitialized ? oldBounds.united(rect) : rect;
        oldBoundsInitialized = true;
    }
    if (!oldBoundsInitialized) {
        return false;
    }

    QVector<int> order;
    order.reserve(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes[i]) {
            order.push_back(i);
        }
    }
    if (order.size() < 2) {
        return false;
    }

    std::sort(order.begin(), order.end(), [&nodes](int lhs, int rhs) {
        const QPointF a = nodes[lhs]->pos();
        const QPointF b = nodes[rhs]->pos();
        if (!qFuzzyCompare(a.y() + 1.0, b.y() + 1.0)) {
            return a.y() < b.y();
        }
        if (!qFuzzyCompare(a.x() + 1.0, b.x() + 1.0)) {
            return a.x() < b.x();
        }
        return nodes[lhs]->nodeId() < nodes[rhs]->nodeId();
    });

    const int columns = std::max(2, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(order.size())))));

    QVector<QPointF> targetPositions(nodes.size(), QPointF());
    QRectF newBounds;
    bool newBoundsInitialized = false;
    for (int i = 0; i < order.size(); ++i) {
        const int nodeIdx = order[i];
        const int row = i / columns;
        const int column = i % columns;
        const QPointF target(column * xSpacing, row * ySpacing);
        targetPositions[nodeIdx] = target;

        const QRectF nodeRect(target, nodes[nodeIdx]->nodeSize());
        newBounds = newBoundsInitialized ? newBounds.united(nodeRect) : nodeRect;
        newBoundsInitialized = true;
    }

    if (!newBoundsInitialized) {
        return false;
    }

    const QPointF translation = oldBounds.center() - newBounds.center();
    bool changed = false;
    for (int nodeIdx : order) {
        QPointF finalPos = targetPositions[nodeIdx] + translation;
        if (m_snapToGrid) {
            finalPos = snapPoint(finalPos);
        }
        if (nodes[nodeIdx]->pos() == finalPos) {
            continue;
        }
        nodes[nodeIdx]->setPos(finalPos);
        changed = true;
    }
    return changed;
}

void EditorScene::finishConnectionAt(const QPointF& scenePos, PortItem* explicitTarget) {
    if (!m_pendingPort) {
        return;
    }

    PortItem* target = explicitTarget ? explicitTarget : pickPortAt(scenePos);
    if (target && canConnect(m_pendingPort, target)) {
        PortItem* outPort = (m_pendingPort->direction() == PortDirection::Output) ? m_pendingPort : target;
        PortItem* inPort = (m_pendingPort->direction() == PortDirection::Input) ? m_pendingPort : target;
        if (m_undoStack) {
            createEdgeWithUndo(outPort, inPort);
        } else {
            createEdge(outPort, inPort);
        }
    }

    if (m_previewEdge) {
        removeItem(m_previewEdge);
        delete m_previewEdge;
        m_previewEdge = nullptr;
    }
    m_pendingPort = nullptr;
    emit connectionStateChanged(false);
}

NodeItem* EditorScene::buildNodeByType(const QString& typeName) {
    const ComponentCatalog& catalog = ComponentCatalog::instance();
    const ComponentSpec* spec = catalog.find(typeName);
    const ComponentSpec& resolved = spec ? *spec : catalog.fallback();

    const QString id = nextNodeId();
    const QString display = resolved.displayName;

    QVector<PortData> ports;
    const int inCount = std::max(1, resolved.inputCount);
    const int outCount = std::max(1, resolved.outputCount);

    for (int i = 0; i < inCount; ++i) {
        ports.push_back(PortData{nextPortId(), QStringLiteral("in%1").arg(i + 1), QStringLiteral("input")});
    }
    for (int i = 0; i < outCount; ++i) {
        ports.push_back(PortData{nextPortId(), QStringLiteral("out%1").arg(i + 1), QStringLiteral("output")});
    }
    return buildNode(id, resolved.typeName, display, resolved.size, ports, resolved.defaultProperties);
}

NodeItem* EditorScene::buildNode(const QString& nodeId,
                                 const QString& typeName,
                                 const QString& displayName,
                                 const QSizeF& size,
                                 const QVector<PortData>& ports,
                                 const QVector<PropertyData>& properties) {
    NodeItem* node = new NodeItem(nodeId, typeName, displayName, size);
    node->setProperties(properties);
    connect(node, &NodeItem::nodeDragFinished, this, &EditorScene::onNodeDragFinished);
    for (const PortData& port : ports) {
        const PortDirection dir =
            port.direction.compare(QStringLiteral("output"), Qt::CaseInsensitive) == 0 ? PortDirection::Output
                                                                                         : PortDirection::Input;
        PortItem* p = node->addPort(port.id, port.name, dir);
        connect(p, &PortItem::connectionStart, this, &EditorScene::onPortConnectionStart);
        connect(p, &PortItem::connectionRelease, this, &EditorScene::onPortConnectionRelease);
    }
    return node;
}
