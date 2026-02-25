#include "app/GraphView.h"
#include "items/EdgeItem.h"
#include "items/NodeItem.h"
#include "model/GraphSerializer.h"
#include "scene/EditorScene.h"

#include <QDataStream>
#include <QFile>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <QUndoStack>

namespace {
int countNodes(const EditorScene& scene) {
    int count = 0;
    for (QGraphicsItem* item : scene.items()) {
        if (dynamic_cast<NodeItem*>(item)) {
            ++count;
        }
    }
    return count;
}

int countEdges(const EditorScene& scene) {
    int count = 0;
    for (QGraphicsItem* item : scene.items()) {
        if (EdgeItem* edge = dynamic_cast<EdgeItem*>(item)) {
            if (edge->sourcePort() && edge->targetPort()) {
                ++count;
            }
        }
    }
    return count;
}

NodeItem* findNodeById(EditorScene& scene, const QString& nodeId) {
    for (QGraphicsItem* item : scene.items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            if (node->nodeId() == nodeId) {
                return node;
            }
        }
    }
    return nullptr;
}
}  // namespace

class EdaSuite : public QObject {
    Q_OBJECT

private slots:
    void serializerRoundtrip();
    void serializerLegacyMigration();
    void serializerUnsupportedSchema();
    void sceneRoundtrip();
    void undoRedoSmoke();
    void granularCommandMerge();
    void obstacleRoutingToggle();
    void toolboxMimeDropAccepted();
    void stressLargeGraphBuild();
};

void EdaSuite::serializerRoundtrip() {
    GraphDocument src;
    src.schemaVersion = 1;
    src.nodes.push_back(NodeData{QStringLiteral("N_1"),
                                 QStringLiteral("Voter"),
                                 QStringLiteral("VoterA"),
                                 QPointF(120.0, 200.0),
                                 QSizeF(120.0, 72.0),
                                 {PortData{QStringLiteral("P_1"), QStringLiteral("in1"), QStringLiteral("input")},
                                  PortData{QStringLiteral("P_2"), QStringLiteral("out1"), QStringLiteral("output")}},
                                 {PropertyData{QStringLiteral("vote_required"), QStringLiteral("int"), QStringLiteral("2")}}});
    src.nodes.push_back(NodeData{QStringLiteral("N_2"),
                                 QStringLiteral("Sum"),
                                 QStringLiteral("SumA"),
                                 QPointF(300.0, 220.0),
                                 QSizeF(120.0, 72.0),
                                 {PortData{QStringLiteral("P_3"), QStringLiteral("in1"), QStringLiteral("input")},
                                  PortData{QStringLiteral("P_4"), QStringLiteral("out1"), QStringLiteral("output")}},
                                 {PropertyData{QStringLiteral("bias"), QStringLiteral("double"), QStringLiteral("0.0")}}});
    src.edges.push_back(EdgeData{QStringLiteral("E_1"),
                                 QStringLiteral("N_1"),
                                 QStringLiteral("P_2"),
                                 QStringLiteral("N_2"),
                                 QStringLiteral("P_3")});

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString filePath = tmp.filePath(QStringLiteral("graph.json"));

    QString error;
    QVERIFY(GraphSerializer::saveToFile(src, filePath, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    GraphDocument dst;
    QVERIFY(GraphSerializer::loadFromFile(&dst, filePath, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QCOMPARE(dst.schemaVersion, src.schemaVersion);
    QCOMPARE(dst.nodes.size(), src.nodes.size());
    QCOMPARE(dst.edges.size(), src.edges.size());
    QCOMPARE(dst.nodes[0].id, src.nodes[0].id);
    QCOMPARE(dst.nodes[0].properties.size(), src.nodes[0].properties.size());
    QCOMPARE(dst.nodes[0].properties[0].key, src.nodes[0].properties[0].key);
    QCOMPARE(dst.edges[0].fromPortId, src.edges[0].fromPortId);
}

void EdaSuite::serializerLegacyMigration() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString filePath = tmp.filePath(QStringLiteral("legacy.json"));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = R"({
      "schemaVersion": 0,
      "nodes": [
        { "id": "N_A", "type": "tm_Node", "name": "A", "x": 10, "y": 20, "w": 120, "h": 72, "ports": [] },
        { "id": "N_B", "type": "tm_Node", "name": "B", "x": 220, "y": 20, "w": 120, "h": 72, "ports": [] }
      ],
      "edges": [
        { "id": "", "fromNodeId": "N_A", "fromPortId": "", "toNodeId": "N_B", "toPortId": "" }
      ]
    })";
    file.write(payload);
    file.close();

    GraphDocument doc;
    QString error;
    QVERIFY(GraphSerializer::loadFromFile(&doc, filePath, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QCOMPARE(doc.schemaVersion, 1);
    QCOMPARE(doc.nodes.size(), 2);
    QVERIFY(doc.nodes[0].ports.size() >= 2);
    QVERIFY(doc.nodes[1].ports.size() >= 2);
    QCOMPARE(doc.edges.size(), 1);
    QVERIFY(!doc.edges[0].id.isEmpty());
    QVERIFY(!doc.edges[0].fromPortId.isEmpty());
    QVERIFY(!doc.edges[0].toPortId.isEmpty());
}

void EdaSuite::serializerUnsupportedSchema() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString filePath = tmp.filePath(QStringLiteral("future.json"));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = R"({
      "schemaVersion": 99,
      "nodes": [],
      "edges": []
    })";
    file.write(payload);
    file.close();

    GraphDocument doc;
    QString error;
    QVERIFY(!GraphSerializer::loadFromFile(&doc, filePath, &error));
    QVERIFY(!error.isEmpty());
}

void EdaSuite::sceneRoundtrip() {
    EditorScene scene;
    NodeItem* n1 = scene.createNode(QStringLiteral("Voter"), QPointF(100.0, 100.0));
    NodeItem* n2 = scene.createNode(QStringLiteral("Sum"), QPointF(350.0, 140.0));
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);
    QVERIFY(scene.createEdge(n1->firstOutputPort(), n2->firstInputPort()) != nullptr);

    GraphDocument doc = scene.toDocument();
    QCOMPARE(doc.nodes.size(), 2);
    QCOMPARE(doc.edges.size(), 1);

    EditorScene loaded;
    QVERIFY(loaded.fromDocument(doc));
    QCOMPARE(countNodes(loaded), 2);
    QCOMPARE(countEdges(loaded), 1);
}

void EdaSuite::undoRedoSmoke() {
    EditorScene scene;
    QUndoStack undoStack;
    scene.setUndoStack(&undoStack);

    NodeItem* n1 = scene.createNodeWithUndo(QStringLiteral("Voter"), QPointF(100.0, 100.0));
    QVERIFY(n1 != nullptr);
    const QString n1Id = n1->nodeId();
    QCOMPARE(countNodes(scene), 1);
    QCOMPARE(undoStack.count(), 1);

    undoStack.undo();
    QCOMPARE(countNodes(scene), 0);

    undoStack.redo();
    QCOMPARE(countNodes(scene), 1);

    NodeItem* n2 = scene.createNodeWithUndo(QStringLiteral("Sum"), QPointF(320.0, 120.0));
    QVERIFY(n2 != nullptr);
    const QString n2Id = n2->nodeId();
    n1 = findNodeById(scene, n1Id);
    n2 = findNodeById(scene, n2Id);
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);
    QVERIFY(scene.createEdgeWithUndo(n1->firstOutputPort(), n2->firstInputPort()) != nullptr);
    QCOMPARE(countEdges(scene), 1);

    undoStack.undo();
    QCOMPARE(countEdges(scene), 0);
}

void EdaSuite::granularCommandMerge() {
    EditorScene scene;
    QUndoStack undoStack;
    scene.setUndoStack(&undoStack);

    NodeItem* node = scene.createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(0.0, 0.0));
    QVERIFY(node != nullptr);
    const QString nodeId = node->nodeId();
    QCOMPARE(undoStack.count(), 1);

    QVERIFY(scene.moveNodeWithUndo(nodeId, QPointF(100.0, 100.0)));
    QCOMPARE(undoStack.count(), 2);
    QVERIFY(scene.moveNodeWithUndo(nodeId, QPointF(140.0, 100.0)));
    QCOMPARE(undoStack.count(), 2);

    QVERIFY(scene.renameNodeWithUndo(nodeId, QStringLiteral("Node_A")));
    QCOMPARE(undoStack.count(), 3);
    QVERIFY(scene.renameNodeWithUndo(nodeId, QStringLiteral("Node_B")));
    QCOMPARE(undoStack.count(), 3);

    QVERIFY(scene.setNodePropertyWithUndo(nodeId, QStringLiteral("pressure"), QStringLiteral("2.0")));
    QCOMPARE(undoStack.count(), 4);
    QVERIFY(scene.setNodePropertyWithUndo(nodeId, QStringLiteral("pressure"), QStringLiteral("3.0")));
    QCOMPARE(undoStack.count(), 4);

    undoStack.undo();
    node = findNodeById(scene, nodeId);
    QVERIFY(node != nullptr);
    QCOMPARE(node->propertyValue(QStringLiteral("pressure")), QStringLiteral("1.0"));

    undoStack.undo();
    node = findNodeById(scene, nodeId);
    QVERIFY(node != nullptr);
    QCOMPARE(node->displayName(), QStringLiteral("tm_Node"));

    undoStack.undo();
    node = findNodeById(scene, nodeId);
    QVERIFY(node != nullptr);
    QCOMPARE(node->pos(), QPointF(0.0, 0.0));
}

void EdaSuite::obstacleRoutingToggle() {
    EditorScene scene;
    scene.setSnapToGrid(false);

    NodeItem* left = scene.createNode(QStringLiteral("tm_Node"), QPointF(100.0, 140.0));
    NodeItem* blocker = scene.createNode(QStringLiteral("tm_Node"), QPointF(320.0, 130.0));
    NodeItem* right = scene.createNode(QStringLiteral("tm_Node"), QPointF(520.0, 140.0));
    QVERIFY(left != nullptr);
    QVERIFY(blocker != nullptr);
    QVERIFY(right != nullptr);

    EdgeItem* edge = scene.createEdge(left->firstOutputPort(), right->firstInputPort());
    QVERIFY(edge != nullptr);

    scene.setEdgeRoutingMode(EdgeRoutingMode::Manhattan);
    QCOMPARE(edge->routingMode(), EdgeRoutingMode::Manhattan);
    const QRectF manhattanBounds = edge->path().boundingRect();
    QVERIFY(manhattanBounds.height() < 1.0);

    scene.setEdgeRoutingMode(EdgeRoutingMode::ObstacleAvoiding);
    QCOMPARE(edge->routingMode(), EdgeRoutingMode::ObstacleAvoiding);
    const QRectF avoidBounds = edge->path().boundingRect();
    QVERIFY(avoidBounds.height() > manhattanBounds.height() + 10.0);
}

void EdaSuite::toolboxMimeDropAccepted() {
    class TestGraphView final : public GraphView {
    public:
        using GraphView::dragEnterEvent;
        using GraphView::dropEvent;
    };

    EditorScene scene;
    scene.setSceneRect(0, 0, 1200, 800);

    TestGraphView view;
    view.resize(640, 480);
    view.setScene(&scene);

    QMimeData mime;
    QByteArray encoded;
    {
        QDataStream stream(&encoded, QIODevice::WriteOnly);
        QMap<int, QVariant> roleData;
        roleData.insert(Qt::UserRole, QStringLiteral("Voter"));
        roleData.insert(Qt::DisplayRole, QStringLiteral("Voter"));
        stream << 0 << 0 << roleData;
    }
    mime.setData(QStringLiteral("application/x-qabstractitemmodeldatalist"), encoded);

    QSignalSpy droppedSpy(&view, &GraphView::paletteItemDropped);

    QDragEnterEvent enterEvent(QPoint(120, 80), Qt::CopyAction, &mime, Qt::NoButton, Qt::NoModifier);
    view.dragEnterEvent(&enterEvent);
    QVERIFY(enterEvent.isAccepted());

    QDropEvent dropEvent(QPointF(120.0, 80.0), Qt::CopyAction, &mime, Qt::NoButton, Qt::NoModifier);
    view.dropEvent(&dropEvent);
    QVERIFY(dropEvent.isAccepted());

    QCOMPARE(droppedSpy.count(), 1);
    const QList<QVariant> args = droppedSpy.takeFirst();
    QCOMPARE(args[0].toString(), QStringLiteral("Voter"));
    QVERIFY(args[1].canConvert<QPointF>());
}

void EdaSuite::stressLargeGraphBuild() {
    EditorScene scene;
    QVector<NodeItem*> created;
    created.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        const int col = i % 40;
        const int row = i / 40;
        NodeItem* node = scene.createNode(QStringLiteral("tm_Node"), QPointF(80.0 + col * 160.0, 80.0 + row * 110.0));
        QVERIFY(node != nullptr);
        created.push_back(node);
    }

    for (int i = 1; i < created.size(); ++i) {
        QVERIFY(scene.createEdge(created[i - 1]->firstOutputPort(), created[i]->firstInputPort()) != nullptr);
    }

    QCOMPARE(countNodes(scene), 1000);
    QCOMPARE(countEdges(scene), 999);
}

QTEST_MAIN(EdaSuite)
#include "test_suite.moc"
