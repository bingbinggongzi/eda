#include "EditorScene.h"

#include "../items/EdgeItem.h"
#include "../items/NodeItem.h"
#include "../items/PortItem.h"

#include <algorithm>
#include <cmath>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QRegularExpression>

EditorScene::EditorScene(QObject* parent)
    : QGraphicsScene(parent) {
    connect(this, &QGraphicsScene::selectionChanged, this, &EditorScene::onSelectionChangedInternal);
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

NodeItem* EditorScene::createNodeFromData(const NodeData& nodeData) {
    NodeItem* node = buildNode(nodeData.id, nodeData.type, nodeData.name, nodeData.size, nodeData.ports);
    if (!node) {
        return nullptr;
    }
    node->setPos(nodeData.position);
    addItem(node);

    updateCounterFromId(nodeData.id, &m_nodeCounter);
    for (const PortData& port : nodeData.ports) {
        updateCounterFromId(port.id, &m_portCounter);
    }
    return node;
}

EdgeItem* EditorScene::createEdge(PortItem* outputPort, PortItem* inputPort) {
    if (!outputPort || !inputPort) {
        return nullptr;
    }

    EdgeItem* edge = new EdgeItem(nextEdgeId(), outputPort);
    edge->setTargetPort(inputPort);
    addItem(edge);
    emit graphChanged();
    return edge;
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
    edge->setTargetPort(inPort);
    addItem(edge);
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

    clear();
    m_nodeCounter = 1;
    m_portCounter = 1;
    m_edgeCounter = 1;
    emit graphChanged();
}

GraphDocument EditorScene::toDocument() const {
    GraphDocument doc;
    doc.schemaVersion = 1;

    for (QGraphicsItem* item : items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            NodeData nodeData;
            nodeData.id = node->nodeId();
            nodeData.type = node->typeName();
            nodeData.name = node->displayName();
            nodeData.position = node->scenePos();
            nodeData.size = node->nodeSize();

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

    return doc;
}

bool EditorScene::fromDocument(const GraphDocument& document) {
    clearGraph();

    for (const NodeData& node : document.nodes) {
        if (!createNodeFromData(node)) {
            return false;
        }
    }
    for (const EdgeData& edge : document.edges) {
        createEdgeFromData(edge);
    }

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

void EditorScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_previewEdge) {
        m_previewEdge->setPreviewEnd(event->scenePos());
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void EditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_pendingPort) {
        finishConnectionAt(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void EditorScene::onPortConnectionStart(PortItem* port) {
    if (!port) {
        return;
    }

    m_pendingPort = port;
    if (m_previewEdge) {
        removeItem(m_previewEdge);
        delete m_previewEdge;
        m_previewEdge = nullptr;
    }

    m_previewEdge = new EdgeItem(QStringLiteral("__preview__"), port);
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

QString EditorScene::nextNodeId() {
    return QStringLiteral("N_%1").arg(m_nodeCounter++);
}

QString EditorScene::nextPortId() {
    return QStringLiteral("P_%1").arg(m_portCounter++);
}

QString EditorScene::nextEdgeId() {
    return QStringLiteral("E_%1").arg(m_edgeCounter++);
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
    return a->direction() != b->direction();
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

void EditorScene::finishConnectionAt(const QPointF& scenePos, PortItem* explicitTarget) {
    if (!m_pendingPort) {
        return;
    }

    PortItem* target = explicitTarget ? explicitTarget : pickPortAt(scenePos);
    if (target && canConnect(m_pendingPort, target)) {
        PortItem* outPort = (m_pendingPort->direction() == PortDirection::Output) ? m_pendingPort : target;
        PortItem* inPort = (m_pendingPort->direction() == PortDirection::Input) ? m_pendingPort : target;
        createEdge(outPort, inPort);
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
    const QString id = nextNodeId();
    const QString display = typeName.isEmpty() ? QStringLiteral("Node") : typeName;

    QVector<PortData> ports;
    const QString lower = typeName.toLower();
    int inCount = 1;
    int outCount = 1;
    if (lower.contains(QStringLiteral("voter"))) {
        inCount = 3;
        outCount = 1;
    } else if (lower.contains(QStringLiteral("sum"))) {
        inCount = 2;
        outCount = 1;
    } else if (lower.contains(QStringLiteral("sft"))) {
        inCount = 2;
        outCount = 1;
    }

    for (int i = 0; i < inCount; ++i) {
        ports.push_back(PortData{nextPortId(), QStringLiteral("in%1").arg(i + 1), QStringLiteral("input")});
    }
    for (int i = 0; i < outCount; ++i) {
        ports.push_back(PortData{nextPortId(), QStringLiteral("out%1").arg(i + 1), QStringLiteral("output")});
    }
    return buildNode(id, typeName, display, QSizeF(120.0, 72.0), ports);
}

NodeItem* EditorScene::buildNode(const QString& nodeId,
                                 const QString& typeName,
                                 const QString& displayName,
                                 const QSizeF& size,
                                 const QVector<PortData>& ports) {
    NodeItem* node = new NodeItem(nodeId, typeName, displayName, size);
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
