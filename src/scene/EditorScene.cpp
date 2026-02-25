#include "EditorScene.h"

#include "../items/EdgeItem.h"
#include "../items/NodeItem.h"
#include "../items/PortItem.h"

#include <cmath>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>

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

    for (PortItem* p : node->inputPorts()) {
        connect(p, &PortItem::connectionStart, this, &EditorScene::onPortConnectionStart);
        connect(p, &PortItem::connectionRelease, this, &EditorScene::onPortConnectionRelease);
    }
    for (PortItem* p : node->outputPorts()) {
        connect(p, &PortItem::connectionStart, this, &EditorScene::onPortConnectionStart);
        connect(p, &PortItem::connectionRelease, this, &EditorScene::onPortConnectionRelease);
    }

    emit graphChanged();
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
    NodeItem* node = new NodeItem(id, typeName, display, QSizeF(120.0, 72.0));

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
        node->addPort(nextPortId(), QStringLiteral("in%1").arg(i + 1), PortDirection::Input);
    }
    for (int i = 0; i < outCount; ++i) {
        node->addPort(nextPortId(), QStringLiteral("out%1").arg(i + 1), PortDirection::Output);
    }

    return node;
}
