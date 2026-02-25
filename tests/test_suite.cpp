#include "../src/items/EdgeItem.h"
#include "../src/items/NodeItem.h"
#include "../src/model/GraphSerializer.h"
#include "../src/scene/EditorScene.h"

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
    void sceneRoundtrip();
    void undoRedoSmoke();
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
                                  PortData{QStringLiteral("P_2"), QStringLiteral("out1"), QStringLiteral("output")}}});
    src.nodes.push_back(NodeData{QStringLiteral("N_2"),
                                 QStringLiteral("Sum"),
                                 QStringLiteral("SumA"),
                                 QPointF(300.0, 220.0),
                                 QSizeF(120.0, 72.0),
                                 {PortData{QStringLiteral("P_3"), QStringLiteral("in1"), QStringLiteral("input")},
                                  PortData{QStringLiteral("P_4"), QStringLiteral("out1"), QStringLiteral("output")}}});
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
    QCOMPARE(dst.edges[0].fromPortId, src.edges[0].fromPortId);
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

QTEST_MAIN(EdaSuite)
#include "test_suite.moc"
