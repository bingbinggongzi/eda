#include "app/GraphView.h"
#include "app/MainWindow.h"
#include "items/EdgeItem.h"
#include "items/NodeItem.h"
#include "model/GraphSerializer.h"
#include "scene/EditorScene.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QMimeData>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QtTest>
#include <QtGlobal>
#include <QUndoStack>

#include <cmath>

namespace {
struct SnapshotOptions {
    int channelTolerance = 8;
    double maxDifferentRatio = 0.01;
};

bool snapshotUpdateModeEnabled() {
    return qEnvironmentVariableIntValue("EDA_UPDATE_SNAPSHOTS") == 1;
}

QString snapshotBaselineDir() {
    return QStringLiteral(EDA_SOURCE_DIR "/tests/baselines");
}

QString snapshotArtifactDir() {
    return QDir(QDir::currentPath()).filePath(QStringLiteral("snapshot_artifacts"));
}

QImage renderWidgetSnapshot(QWidget* widget) {
    if (!widget) {
        return QImage();
    }
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    widget->render(&painter);
    return image.convertToFormat(QImage::Format_RGBA8888);
}

bool compareOrUpdateSnapshot(const QImage& actualInput,
                             const QString& snapshotName,
                             const SnapshotOptions& options,
                             QString* errorOut) {
    const QImage actual = actualInput.convertToFormat(QImage::Format_RGBA8888);
    if (actual.isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Actual snapshot is null: %1").arg(snapshotName);
        }
        return false;
    }

    QDir baselineDir(snapshotBaselineDir());
    if (!baselineDir.exists()) {
        baselineDir.mkpath(QStringLiteral("."));
    }
    const QString baselinePath = baselineDir.filePath(snapshotName + QStringLiteral(".png"));

    if (snapshotUpdateModeEnabled()) {
        if (!actual.save(baselinePath)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to save baseline: %1").arg(baselinePath);
            }
            return false;
        }
        return true;
    }

    if (!QFileInfo::exists(baselinePath)) {
        QDir artifactDir(snapshotArtifactDir());
        artifactDir.mkpath(QStringLiteral("."));
        actual.save(artifactDir.filePath(snapshotName + QStringLiteral(".actual.png")));
        if (errorOut) {
            *errorOut = QStringLiteral("Missing baseline: %1 (run with EDA_UPDATE_SNAPSHOTS=1)").arg(baselinePath);
        }
        return false;
    }

    QImage baseline(baselinePath);
    baseline = baseline.convertToFormat(QImage::Format_RGBA8888);
    if (baseline.size() != actual.size()) {
        QDir artifactDir(snapshotArtifactDir());
        artifactDir.mkpath(QStringLiteral("."));
        baseline.save(artifactDir.filePath(snapshotName + QStringLiteral(".baseline.png")));
        actual.save(artifactDir.filePath(snapshotName + QStringLiteral(".actual.png")));
        if (errorOut) {
            *errorOut = QStringLiteral("Snapshot size mismatch for %1: baseline=%2x%3 actual=%4x%5")
                            .arg(snapshotName)
                            .arg(baseline.width())
                            .arg(baseline.height())
                            .arg(actual.width())
                            .arg(actual.height());
        }
        return false;
    }

    QImage diff(actual.size(), QImage::Format_ARGB32);
    int diffPixels = 0;
    const int total = actual.width() * actual.height();

    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < actual.width(); ++x) {
            const QColor a(actual.pixel(x, y));
            const QColor b(baseline.pixel(x, y));
            const bool different = std::abs(a.red() - b.red()) > options.channelTolerance ||
                                   std::abs(a.green() - b.green()) > options.channelTolerance ||
                                   std::abs(a.blue() - b.blue()) > options.channelTolerance ||
                                   std::abs(a.alpha() - b.alpha()) > options.channelTolerance;
            if (different) {
                ++diffPixels;
                diff.setPixelColor(x, y, QColor(255, 0, 255));
            } else {
                const int gray = (b.red() + b.green() + b.blue()) / 3;
                diff.setPixelColor(x, y, QColor(gray, gray, gray, 255));
            }
        }
    }

    const double ratio = total > 0 ? static_cast<double>(diffPixels) / static_cast<double>(total) : 0.0;
    if (ratio > options.maxDifferentRatio) {
        QDir artifactDir(snapshotArtifactDir());
        artifactDir.mkpath(QStringLiteral("."));
        baseline.save(artifactDir.filePath(snapshotName + QStringLiteral(".baseline.png")));
        actual.save(artifactDir.filePath(snapshotName + QStringLiteral(".actual.png")));
        diff.save(artifactDir.filePath(snapshotName + QStringLiteral(".diff.png")));
        if (errorOut) {
            *errorOut = QStringLiteral("Snapshot diff too large for %1: %.4f (limit %.4f)")
                            .arg(snapshotName)
                            .arg(ratio)
                            .arg(options.maxDifferentRatio);
        }
        return false;
    }

    return true;
}

void saveSmokeArtifact(const QImage& image, const QString& fileName) {
    if (image.isNull()) {
        return;
    }
    QDir artifactDir(snapshotArtifactDir());
    artifactDir.mkpath(QStringLiteral("."));
    image.save(artifactDir.filePath(fileName));
}

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

QVector<QPointF> pathPolyline(const QPainterPath& path) {
    QVector<QPointF> points;
    points.reserve(path.elementCount());
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        const QPointF p(e.x, e.y);
        if (!points.isEmpty()) {
            const QPointF prev = points.last();
            if (std::abs(prev.x() - p.x()) < 0.1 && std::abs(prev.y() - p.y()) < 0.1) {
                continue;
            }
        }
        points.push_back(p);
    }
    return points;
}

int pathTurnCount(const QVector<QPointF>& points) {
    if (points.size() < 3) {
        return 0;
    }

    int turns = 0;
    QPointF previousDelta = points[1] - points[0];
    for (int i = 2; i < points.size(); ++i) {
        const QPointF currentDelta = points[i] - points[i - 1];
        const bool previousHorizontal = std::abs(previousDelta.x()) > std::abs(previousDelta.y());
        const bool currentHorizontal = std::abs(currentDelta.x()) > std::abs(currentDelta.y());
        if (previousHorizontal != currentHorizontal) {
            ++turns;
        }
        previousDelta = currentDelta;
    }
    return turns;
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
    void edgeConnectionRules();
    void granularCommandMerge();
    void autoLayoutUndoAndSelection();
    void autoLayoutModesAndSpacing();
    void rotateAndLayerUndo();
    void groupUngroupUndo();
    void groupVisualAndSelectMembers();
    void obstacleRoutingToggle();
    void obstacleRoutingDirectionalBias();
    void parallelEdgeBundleSpread();
    void toolboxMimeDropAccepted();
    void fileLifecycleNewSaveAsClose();
    void fileLifecycleOpenAndDirtyPrompt();
    void uiSnapshotMainWindow();
    void uiSnapshotGraphViewDragPreview();
    void uiActionClickSmokeCapture();
    void layoutSettingsMarkDirty();
    void stressLargeGraphBuild();
};

void EdaSuite::serializerRoundtrip() {
    GraphDocument src;
    src.schemaVersion = 1;
    src.autoLayoutMode = QStringLiteral("grid");
    src.autoLayoutXSpacing = 360.0;
    src.autoLayoutYSpacing = 210.0;
    src.nodes.push_back(NodeData{QStringLiteral("N_1"),
                                 QStringLiteral("Voter"),
                                 QStringLiteral("VoterA"),
                                 QPointF(120.0, 200.0),
                                 QSizeF(120.0, 72.0),
                                 {PortData{QStringLiteral("P_1"), QStringLiteral("in1"), QStringLiteral("input")},
                                  PortData{QStringLiteral("P_2"), QStringLiteral("out1"), QStringLiteral("output")}},
                                 {PropertyData{QStringLiteral("vote_required"), QStringLiteral("int"), QStringLiteral("2")}},
                                 15.0,
                                 3.0,
                                 QStringLiteral("G_1")});
    src.nodes.push_back(NodeData{QStringLiteral("N_2"),
                                 QStringLiteral("Sum"),
                                 QStringLiteral("SumA"),
                                 QPointF(300.0, 220.0),
                                 QSizeF(120.0, 72.0),
                                 {PortData{QStringLiteral("P_3"), QStringLiteral("in1"), QStringLiteral("input")},
                                  PortData{QStringLiteral("P_4"), QStringLiteral("out1"), QStringLiteral("output")}},
                                 {PropertyData{QStringLiteral("bias"), QStringLiteral("double"), QStringLiteral("0.0")}},
                                 -30.0,
                                 1.0,
                                 QStringLiteral("G_1")});
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
    QCOMPARE(dst.autoLayoutMode, src.autoLayoutMode);
    QCOMPARE(dst.autoLayoutXSpacing, src.autoLayoutXSpacing);
    QCOMPARE(dst.autoLayoutYSpacing, src.autoLayoutYSpacing);
    QCOMPARE(dst.nodes.size(), src.nodes.size());
    QCOMPARE(dst.edges.size(), src.edges.size());
    QCOMPARE(dst.nodes[0].id, src.nodes[0].id);
    QCOMPARE(dst.nodes[0].properties.size(), src.nodes[0].properties.size());
    QCOMPARE(dst.nodes[0].properties[0].key, src.nodes[0].properties[0].key);
    QCOMPARE(dst.nodes[0].rotationDegrees, src.nodes[0].rotationDegrees);
    QCOMPARE(dst.nodes[0].z, src.nodes[0].z);
    QCOMPARE(dst.nodes[0].groupId, src.nodes[0].groupId);
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
    QCOMPARE(doc.autoLayoutMode, QStringLiteral("layered"));
    QCOMPARE(doc.autoLayoutXSpacing, 240.0);
    QCOMPARE(doc.autoLayoutYSpacing, 140.0);
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
    scene.setAutoLayoutMode(AutoLayoutMode::Grid);
    scene.setAutoLayoutSpacing(300.0, 170.0);
    NodeItem* n1 = scene.createNode(QStringLiteral("Voter"), QPointF(100.0, 100.0));
    NodeItem* n2 = scene.createNode(QStringLiteral("Sum"), QPointF(350.0, 140.0));
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);
    n1->setRotation(30.0);
    n1->setZValue(5.0);
    n1->setGroupId(QStringLiteral("G_1"));
    n2->setGroupId(QStringLiteral("G_1"));
    QVERIFY(scene.createEdge(n1->firstOutputPort(), n2->firstInputPort()) != nullptr);

    GraphDocument doc = scene.toDocument();
    QCOMPARE(doc.nodes.size(), 2);
    QCOMPARE(doc.edges.size(), 1);

    EditorScene loaded;
    QVERIFY(loaded.fromDocument(doc));
    QCOMPARE(loaded.autoLayoutMode(), AutoLayoutMode::Grid);
    QCOMPARE(loaded.autoLayoutHorizontalSpacing(), 300.0);
    QCOMPARE(loaded.autoLayoutVerticalSpacing(), 170.0);
    QCOMPARE(countNodes(loaded), 2);
    QCOMPARE(countEdges(loaded), 1);
    NodeItem* loadedN1 = findNodeById(loaded, n1->nodeId());
    NodeItem* loadedN2 = findNodeById(loaded, n2->nodeId());
    QVERIFY(loadedN1 != nullptr);
    QVERIFY(loadedN2 != nullptr);
    QCOMPARE(loadedN1->rotation(), 30.0);
    QCOMPARE(loadedN1->zValue(), 5.0);
    QCOMPARE(loadedN1->groupId(), QStringLiteral("G_1"));
    QCOMPARE(loadedN2->groupId(), QStringLiteral("G_1"));
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

void EdaSuite::edgeConnectionRules() {
    EditorScene scene;

    NodeItem* n1 = scene.createNode(QStringLiteral("Voter"), QPointF(100.0, 100.0));
    NodeItem* n2 = scene.createNode(QStringLiteral("SET"), QPointF(320.0, 100.0));
    NodeItem* n3 = scene.createNode(QStringLiteral("Sum"), QPointF(560.0, 100.0));
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);
    QVERIFY(n3 != nullptr);

    PortItem* out1 = n1->firstOutputPort();
    PortItem* out2 = n2->firstOutputPort();
    PortItem* in3 = n3->firstInputPort();
    QVERIFY(out1 != nullptr);
    QVERIFY(out2 != nullptr);
    QVERIFY(in3 != nullptr);

    QVERIFY(scene.createEdge(out1, in3) != nullptr);
    QCOMPARE(countEdges(scene), 1);

    // Reject duplicate source-target connection.
    QVERIFY(scene.createEdge(out1, in3) == nullptr);
    QCOMPARE(countEdges(scene), 1);

    // Reject a second incoming edge to the same input port.
    QVERIFY(scene.createEdge(out2, in3) == nullptr);
    QCOMPARE(countEdges(scene), 1);

    // Reject invalid direction order.
    QVERIFY(scene.createEdge(in3, out1) == nullptr);

    // Reject same-node self-connection.
    QVERIFY(scene.createEdge(n1->firstOutputPort(), n1->firstInputPort()) == nullptr);

    EditorScene undoScene;
    QUndoStack undoStack;
    undoScene.setUndoStack(&undoStack);

    NodeItem* u1 = undoScene.createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(0.0, 0.0));
    NodeItem* u2 = undoScene.createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(240.0, 0.0));
    QVERIFY(u1 != nullptr);
    QVERIFY(u2 != nullptr);
    QCOMPARE(undoStack.count(), 2);

    QVERIFY(undoScene.createEdgeWithUndo(u1->firstOutputPort(), u2->firstInputPort()) != nullptr);
    QCOMPARE(undoStack.count(), 3);
    QVERIFY(undoScene.createEdgeWithUndo(u1->firstOutputPort(), u2->firstInputPort()) == nullptr);
    QCOMPARE(undoStack.count(), 3);
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

void EdaSuite::autoLayoutUndoAndSelection() {
    {
        EditorScene scene;
        scene.setSnapToGrid(false);
        QUndoStack undoStack;
        scene.setUndoStack(&undoStack);

        NodeItem* n1 = scene.createNode(QStringLiteral("tm_Node"), QPointF(580.0, 360.0));
        NodeItem* n2 = scene.createNode(QStringLiteral("tm_Node"), QPointF(180.0, 120.0));
        NodeItem* n3 = scene.createNode(QStringLiteral("tm_Node"), QPointF(360.0, 480.0));
        QVERIFY(n1 != nullptr);
        QVERIFY(n2 != nullptr);
        QVERIFY(n3 != nullptr);

        QVERIFY(scene.createEdge(n2->firstOutputPort(), n1->firstInputPort()) != nullptr);
        QVERIFY(scene.createEdge(n1->firstOutputPort(), n3->firstInputPort()) != nullptr);

        const QPointF p1Before = n1->pos();
        const QPointF p2Before = n2->pos();
        const QPointF p3Before = n3->pos();

        QVERIFY(scene.autoLayoutWithUndo(true));
        QCOMPARE(undoStack.count(), 1);
        QVERIFY(n2->pos().x() < n1->pos().x());
        QVERIFY(n1->pos().x() < n3->pos().x());
        QVERIFY(n1->pos() != p1Before || n2->pos() != p2Before || n3->pos() != p3Before);
    }

    {
        EditorScene scene;
        scene.setSnapToGrid(false);
        QUndoStack undoStack;
        scene.setUndoStack(&undoStack);

        NodeItem* a = scene.createNode(QStringLiteral("tm_Node"), QPointF(620.0, 320.0));
        NodeItem* b = scene.createNode(QStringLiteral("tm_Node"), QPointF(260.0, 420.0));
        NodeItem* c = scene.createNode(QStringLiteral("tm_Node"), QPointF(120.0, 80.0));
        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);
        QVERIFY(c != nullptr);

        scene.createEdge(a->firstOutputPort(), b->firstInputPort());

        const QPointF cBefore = c->pos();
        a->setSelected(true);
        b->setSelected(true);

        QVERIFY(scene.autoLayoutWithUndo(true));
        QCOMPARE(c->pos(), cBefore);
        QCOMPARE(undoStack.count(), 1);
    }
}

void EdaSuite::autoLayoutModesAndSpacing() {
    {
        EditorScene scene;
        scene.setSnapToGrid(false);
        QUndoStack undoStack;
        scene.setUndoStack(&undoStack);

        scene.setAutoLayoutMode(AutoLayoutMode::Grid);
        scene.setAutoLayoutSpacing(300.0, 180.0);

        NodeItem* n1 = scene.createNode(QStringLiteral("tm_Node"), QPointF(120.0, 420.0));
        NodeItem* n2 = scene.createNode(QStringLiteral("tm_Node"), QPointF(640.0, 140.0));
        NodeItem* n3 = scene.createNode(QStringLiteral("tm_Node"), QPointF(320.0, 260.0));
        NodeItem* n4 = scene.createNode(QStringLiteral("tm_Node"), QPointF(520.0, 560.0));
        QVERIFY(n1 != nullptr);
        QVERIFY(n2 != nullptr);
        QVERIFY(n3 != nullptr);
        QVERIFY(n4 != nullptr);
        const QString n1Id = n1->nodeId();
        const QString n2Id = n2->nodeId();
        const QString n3Id = n3->nodeId();
        const QString n4Id = n4->nodeId();
        const QPointF p1Before = n1->pos();
        const QPointF p2Before = n2->pos();
        const QPointF p3Before = n3->pos();
        const QPointF p4Before = n4->pos();

        QVERIFY(scene.autoLayoutWithUndo(false));
        QCOMPARE(undoStack.count(), 1);

        const qreal minX = std::min(std::min(n1->pos().x(), n2->pos().x()), std::min(n3->pos().x(), n4->pos().x()));
        const qreal minY = std::min(std::min(n1->pos().y(), n2->pos().y()), std::min(n3->pos().y(), n4->pos().y()));

        QVector<QPointF> normalized{
            QPointF(n1->pos().x() - minX, n1->pos().y() - minY),
            QPointF(n2->pos().x() - minX, n2->pos().y() - minY),
            QPointF(n3->pos().x() - minX, n3->pos().y() - minY),
            QPointF(n4->pos().x() - minX, n4->pos().y() - minY),
        };

        QSet<QString> expected{
            QStringLiteral("0,0"),
            QStringLiteral("300,0"),
            QStringLiteral("0,180"),
            QStringLiteral("300,180"),
        };
        for (const QPointF& p : normalized) {
            expected.remove(QStringLiteral("%1,%2").arg(qRound(p.x())).arg(qRound(p.y())));
        }
        QVERIFY(expected.isEmpty());

        undoStack.undo();
        n1 = findNodeById(scene, n1Id);
        n2 = findNodeById(scene, n2Id);
        n3 = findNodeById(scene, n3Id);
        n4 = findNodeById(scene, n4Id);
        QVERIFY(n1 != nullptr);
        QVERIFY(n2 != nullptr);
        QVERIFY(n3 != nullptr);
        QVERIFY(n4 != nullptr);
        QCOMPARE(n1->pos(), p1Before);
        QCOMPARE(n2->pos(), p2Before);
        QCOMPARE(n3->pos(), p3Before);
        QCOMPARE(n4->pos(), p4Before);
    }

    {
        EditorScene scene;
        scene.setSnapToGrid(false);

        scene.setAutoLayoutMode(AutoLayoutMode::Layered);
        scene.setAutoLayoutSpacing(320.0, 160.0);

        NodeItem* a = scene.createNode(QStringLiteral("tm_Node"), QPointF(600.0, 300.0));
        NodeItem* b = scene.createNode(QStringLiteral("tm_Node"), QPointF(260.0, 120.0));
        NodeItem* c = scene.createNode(QStringLiteral("tm_Node"), QPointF(200.0, 520.0));
        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);
        QVERIFY(c != nullptr);
        QVERIFY(scene.createEdge(b->firstOutputPort(), a->firstInputPort()) != nullptr);
        QVERIFY(scene.createEdge(a->firstOutputPort(), c->firstInputPort()) != nullptr);

        QVERIFY(scene.autoLayoutWithUndo(false));
        const qreal dx1 = a->pos().x() - b->pos().x();
        const qreal dx2 = c->pos().x() - a->pos().x();
        QVERIFY(std::abs(dx1 - 320.0) < 0.6);
        QVERIFY(std::abs(dx2 - 320.0) < 0.6);
    }
}

void EdaSuite::rotateAndLayerUndo() {
    EditorScene scene;
    scene.setSnapToGrid(false);
    QUndoStack undoStack;
    scene.setUndoStack(&undoStack);

    NodeItem* n1 = scene.createNode(QStringLiteral("tm_Node"), QPointF(120.0, 120.0));
    NodeItem* n2 = scene.createNode(QStringLiteral("tm_Node"), QPointF(420.0, 120.0));
    NodeItem* n3 = scene.createNode(QStringLiteral("tm_Node"), QPointF(720.0, 120.0));
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);
    QVERIFY(n3 != nullptr);
    QCOMPARE(countNodes(scene), 3);

    const qreal r1Before = n1->rotation();
    const qreal r2Before = n2->rotation();
    n1->setSelected(true);
    n2->setSelected(true);
    QVERIFY(scene.rotateSelectionWithUndo(90.0));
    QCOMPARE(undoStack.count(), 1);
    QCOMPARE(n1->rotation(), r1Before + 90.0);
    QCOMPARE(n2->rotation(), r2Before + 90.0);

    scene.clearSelection();
    n2->setSelected(true);
    QVERIFY(scene.bringSelectionToFrontWithUndo());
    QCOMPARE(undoStack.count(), 2);
    QVERIFY(n2->zValue() > n1->zValue());
    QVERIFY(n2->zValue() > n3->zValue());

    scene.clearSelection();
    n2->setSelected(true);
    QVERIFY(scene.sendSelectionToBackWithUndo());
    QCOMPARE(undoStack.count(), 3);
    QVERIFY(n2->zValue() < n1->zValue());
    QVERIFY(n2->zValue() < n3->zValue());

    const qreal z1 = n1->zValue();
    scene.clearSelection();
    n1->setSelected(true);
    QVERIFY(scene.bringSelectionForwardWithUndo());
    QCOMPARE(undoStack.count(), 4);
    QCOMPARE(n1->zValue(), z1 + 1.0);

    QVERIFY(scene.sendSelectionBackwardWithUndo());
    QCOMPARE(undoStack.count(), 5);
    QCOMPARE(n1->zValue(), z1);
}

void EdaSuite::groupUngroupUndo() {
    EditorScene scene;
    scene.setSnapToGrid(false);
    QUndoStack undoStack;
    scene.setUndoStack(&undoStack);

    NodeItem* a = scene.createNode(QStringLiteral("tm_Node"), QPointF(140.0, 120.0));
    NodeItem* b = scene.createNode(QStringLiteral("tm_Node"), QPointF(340.0, 120.0));
    NodeItem* c = scene.createNode(QStringLiteral("tm_Node"), QPointF(560.0, 120.0));
    QVERIFY(a != nullptr);
    QVERIFY(b != nullptr);
    QVERIFY(c != nullptr);
    const QString aId = a->nodeId();
    const QString bId = b->nodeId();

    a->setSelected(true);
    b->setSelected(true);
    QVERIFY(scene.groupSelectionWithUndo());
    QCOMPARE(undoStack.count(), 1);
    QVERIFY(!a->groupId().isEmpty());
    QCOMPARE(a->groupId(), b->groupId());
    QVERIFY(c->groupId().isEmpty());

    QGraphicsItemGroup* selectedGroup = nullptr;
    for (QGraphicsItem* item : scene.selectedItems()) {
        if (auto* group = dynamic_cast<QGraphicsItemGroup*>(item)) {
            selectedGroup = group;
            break;
        }
    }
    QVERIFY(selectedGroup != nullptr);

    const qreal aRotationBefore = a->rotation();
    const qreal bRotationBefore = b->rotation();
    QVERIFY(scene.rotateSelectionWithUndo(90.0));
    QCOMPARE(undoStack.count(), 2);
    QCOMPARE(a->rotation(), aRotationBefore + 90.0);
    QCOMPARE(b->rotation(), bRotationBefore + 90.0);

    const GraphDocument groupedDoc = scene.toDocument();
    EditorScene loaded;
    QVERIFY(loaded.fromDocument(groupedDoc));
    NodeItem* loadedA = findNodeById(loaded, a->nodeId());
    NodeItem* loadedB = findNodeById(loaded, b->nodeId());
    QVERIFY(loadedA != nullptr);
    QVERIFY(loadedB != nullptr);
    QVERIFY(!loadedA->groupId().isEmpty());
    QCOMPARE(loadedA->groupId(), loadedB->groupId());

    QVERIFY(scene.ungroupSelectionWithUndo());
    QCOMPARE(undoStack.count(), 3);
    QVERIFY(a->groupId().isEmpty());
    QVERIFY(b->groupId().isEmpty());

    undoStack.undo();
    a = findNodeById(scene, aId);
    b = findNodeById(scene, bId);
    QVERIFY(a != nullptr);
    QVERIFY(b != nullptr);
    QVERIFY(!a->groupId().isEmpty());
    QCOMPARE(a->groupId(), b->groupId());

    undoStack.undo();
    a = findNodeById(scene, aId);
    b = findNodeById(scene, bId);
    QVERIFY(a != nullptr);
    QVERIFY(b != nullptr);
    QCOMPARE(a->rotation(), aRotationBefore);
    QCOMPARE(b->rotation(), bRotationBefore);
}

void EdaSuite::groupVisualAndSelectMembers() {
    EditorScene scene;
    scene.setSnapToGrid(false);

    NodeItem* n1 = scene.createNode(QStringLiteral("tm_Node"), QPointF(120.0, 120.0));
    NodeItem* n2 = scene.createNode(QStringLiteral("tm_Node"), QPointF(360.0, 120.0));
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);

    n1->setSelected(true);
    n2->setSelected(true);
    QVERIFY(scene.groupSelectionWithUndo());

    QGraphicsItemGroup* selectedGroup = nullptr;
    for (QGraphicsItem* item : scene.selectedItems()) {
        if (auto* group = dynamic_cast<QGraphicsItemGroup*>(item)) {
            selectedGroup = group;
            break;
        }
    }
    QVERIFY(selectedGroup != nullptr);

    bool hasFrame = false;
    bool hasTitle = false;
    for (QGraphicsItem* child : selectedGroup->childItems()) {
        if (dynamic_cast<QGraphicsRectItem*>(child)) {
            hasFrame = true;
        }
        if (dynamic_cast<QGraphicsSimpleTextItem*>(child)) {
            hasTitle = true;
        }
    }
    QVERIFY(hasFrame);
    QVERIFY(hasTitle);
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

void EdaSuite::obstacleRoutingDirectionalBias() {
    EditorScene scene;
    scene.setSnapToGrid(false);

    NodeItem* source = scene.createNode(QStringLiteral("tm_Node"), QPointF(560.0, 220.0));
    NodeItem* target = scene.createNode(QStringLiteral("tm_Node"), QPointF(160.0, 260.0));
    // Keep one unrelated obstacle so obstacle routing path-search is always used.
    NodeItem* unrelatedObstacle = scene.createNode(QStringLiteral("tm_Node"), QPointF(960.0, 80.0));
    QVERIFY(source != nullptr);
    QVERIFY(target != nullptr);
    QVERIFY(unrelatedObstacle != nullptr);

    EdgeItem* edge = scene.createEdge(source->firstOutputPort(), target->firstInputPort());
    QVERIFY(edge != nullptr);
    scene.setEdgeRoutingMode(EdgeRoutingMode::ObstacleAvoiding);

    const QVector<QPointF> points = pathPolyline(edge->path());
    QVERIFY(points.size() >= 5);
    // `start -> startAnchor` is guaranteed by construction. The segment after that should not backtrack.
    const QPointF startAnchor = points[1];
    const QPointF firstAfterAnchor = points[2];
    const QString directionMessage =
        QStringLiteral("startAnchor=(%1,%2) firstAfterAnchor=(%3,%4) points=%5")
            .arg(startAnchor.x(), 0, 'f', 1)
            .arg(startAnchor.y(), 0, 'f', 1)
            .arg(firstAfterAnchor.x(), 0, 'f', 1)
            .arg(firstAfterAnchor.y(), 0, 'f', 1)
            .arg(points.size());
    const QByteArray directionMessageUtf8 = directionMessage.toUtf8();
    QVERIFY2(firstAfterAnchor.x() >= startAnchor.x() - 10.0, directionMessageUtf8.constData());

    const int turns = pathTurnCount(points);
    QVERIFY(turns <= 6);
}

void EdaSuite::parallelEdgeBundleSpread() {
    EditorScene scene;
    scene.setSnapToGrid(false);
    scene.setEdgeRoutingMode(EdgeRoutingMode::Manhattan);

    NodeItem* source = scene.createNode(QStringLiteral("tm_Node"), QPointF(120.0, 220.0));
    NodeItem* target = scene.createNode(QStringLiteral("Voter"), QPointF(620.0, 180.0));
    QVERIFY(source != nullptr);
    QVERIFY(target != nullptr);
    QVERIFY(source->firstOutputPort() != nullptr);
    QVERIFY(target->inputPorts().size() >= 3);

    QVector<EdgeItem*> edges;
    for (int i = 0; i < 3; ++i) {
        EdgeItem* edge = scene.createEdge(source->firstOutputPort(), target->inputPorts()[i]);
        QVERIFY(edge != nullptr);
        edges.push_back(edge);
    }

    QVector<qreal> trunkXs;
    trunkXs.reserve(edges.size());
    QSet<int> uniqueTrunkXs;
    for (EdgeItem* edge : edges) {
        const QVector<QPointF> points = pathPolyline(edge->path());
        QVERIFY(points.size() >= 4);
        const qreal trunkX = points[2].x();
        trunkXs.push_back(trunkX);
        uniqueTrunkXs.insert(qRound(trunkX));
    }

    QCOMPARE(uniqueTrunkXs.size(), 3);
    std::sort(trunkXs.begin(), trunkXs.end());
    QVERIFY(std::abs(trunkXs[1] - trunkXs[0]) >= 10.0);
    QVERIFY(std::abs(trunkXs[2] - trunkXs[1]) >= 10.0);
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

void EdaSuite::fileLifecycleNewSaveAsClose() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString savePath = tmp.filePath(QStringLiteral("save_as_graph.json"));

    MainWindow window;
    QCOMPARE(window.documentCount(), 1);

    const int newIndex = window.newDocument(QStringLiteral("DocForSaveAs"));
    QVERIFY(newIndex >= 0);
    QCOMPARE(window.activeDocumentIndex(), newIndex);
    QCOMPARE(window.documentCount(), 2);

    EditorScene* scene = window.activeScene();
    QVERIFY(scene != nullptr);
    QVERIFY(scene->createNodeWithUndo(QStringLiteral("Voter"), QPointF(120.0, 120.0)) != nullptr);
    QCoreApplication::processEvents();
    QVERIFY(window.isDocumentDirty(newIndex));

    window.setSaveFileDialogProvider([savePath](const QString&) { return savePath; });
    QVERIFY(window.saveCurrentDocument(true));
    QVERIFY(QFileInfo::exists(savePath));
    QVERIFY(!window.isDocumentDirty(newIndex));
    QCOMPARE(window.documentFilePath(newIndex), savePath);

    scene = window.activeScene();
    QVERIFY(scene != nullptr);
    QVERIFY(scene->createNodeWithUndo(QStringLiteral("Sum"), QPointF(220.0, 120.0)) != nullptr);
    QCoreApplication::processEvents();
    QVERIFY(window.isDocumentDirty(newIndex));

    window.setUnsavedPromptProvider([](const QString&) { return QMessageBox::Save; });
    window.setSaveFileDialogProvider([savePath](const QString&) { return savePath; });
    QVERIFY(window.closeDocument(newIndex));
    QCOMPARE(window.documentCount(), 1);
}

void EdaSuite::fileLifecycleOpenAndDirtyPrompt() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString openPath = tmp.filePath(QStringLiteral("open_graph.json"));

    GraphDocument doc;
    doc.schemaVersion = 1;
    doc.nodes.push_back(NodeData{QStringLiteral("N_1"),
                                 QStringLiteral("tm_Node"),
                                 QStringLiteral("OpenNode"),
                                 QPointF(10.0, 20.0),
                                 QSizeF(120.0, 72.0),
                                 {PortData{QStringLiteral("P_1"), QStringLiteral("in1"), QStringLiteral("input")},
                                  PortData{QStringLiteral("P_2"), QStringLiteral("out1"), QStringLiteral("output")}},
                                 {}});
    QString error;
    QVERIFY(GraphSerializer::saveToFile(doc, openPath, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    MainWindow window;
    const int beforeCount = window.documentCount();

    window.setOpenFileDialogProvider([openPath]() { return openPath; });
    QVERIFY(window.openDocumentByDialog());

    QCOMPARE(window.documentCount(), beforeCount + 1);
    const int openedIndex = window.activeDocumentIndex();
    QVERIFY(openedIndex >= 0);
    QCOMPARE(window.documentFilePath(openedIndex), openPath);
    QVERIFY(!window.isDocumentDirty(openedIndex));

    EditorScene* scene = window.activeScene();
    QVERIFY(scene != nullptr);
    QVERIFY(scene->createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(300.0, 200.0)) != nullptr);
    QCoreApplication::processEvents();
    QVERIFY(window.isDocumentDirty(openedIndex));

    window.setUnsavedPromptProvider([](const QString&) { return QMessageBox::Cancel; });
    QVERIFY(!window.closeDocument(openedIndex));
    QCOMPARE(window.documentCount(), beforeCount + 1);

    window.setUnsavedPromptProvider([](const QString&) { return QMessageBox::Discard; });
    QVERIFY(window.closeDocument(openedIndex));
    QCOMPARE(window.documentCount(), beforeCount);
}

void EdaSuite::uiSnapshotMainWindow() {
    MainWindow window;
    window.resize(1400, 860);
    window.show();
    window.statusBar()->clearMessage();
    QCoreApplication::processEvents();

    const QImage actual = renderWidgetSnapshot(&window);
    QString error;
    SnapshotOptions options;
    options.channelTolerance = 10;
    options.maxDifferentRatio = 0.02;
    QVERIFY2(compareOrUpdateSnapshot(actual, QStringLiteral("main_window_default"), options, &error), qPrintable(error));
}

void EdaSuite::uiSnapshotGraphViewDragPreview() {
    class TestGraphView final : public GraphView {
    public:
        using GraphView::dragEnterEvent;
        using GraphView::dragMoveEvent;
    };

    EditorScene scene;
    scene.setSceneRect(0, 0, 1400, 900);

    TestGraphView view;
    view.resize(1000, 640);
    view.setScene(&scene);
    view.show();
    QCoreApplication::processEvents();

    QMimeData mime;
    mime.setText(QStringLiteral("Voter"));

    QDragEnterEvent enterEvent(QPoint(180, 120), Qt::CopyAction, &mime, Qt::NoButton, Qt::NoModifier);
    view.dragEnterEvent(&enterEvent);
    QVERIFY(enterEvent.isAccepted());

    QDragMoveEvent moveEvent(QPoint(260, 180), Qt::CopyAction, &mime, Qt::NoButton, Qt::NoModifier);
    view.dragMoveEvent(&moveEvent);
    QVERIFY(moveEvent.isAccepted());

    QCoreApplication::processEvents();
    const QImage actual = renderWidgetSnapshot(&view);
    QString error;
    SnapshotOptions options;
    options.channelTolerance = 8;
    options.maxDifferentRatio = 0.015;
    QVERIFY2(compareOrUpdateSnapshot(actual, QStringLiteral("graph_view_drag_preview"), options, &error), qPrintable(error));
}

void EdaSuite::uiActionClickSmokeCapture() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString savePath = tmp.filePath(QStringLiteral("toolbar_save.json"));
    const QString openPath = tmp.filePath(QStringLiteral("toolbar_open.json"));

    MainWindow window;
    window.resize(1300, 820);
    window.show();
    QCoreApplication::processEvents();

    window.setSaveFileDialogProvider([savePath](const QString&) { return savePath; });
    window.setOpenFileDialogProvider([openPath]() { return openPath; });
    window.setUnsavedPromptProvider([](const QString&) { return QMessageBox::Discard; });

    auto findAction = [&window](const QString& text) -> QAction* {
        const QList<QAction*> actions = window.findChildren<QAction*>();
        for (QAction* action : actions) {
            if (action && action->text() == text) {
                return action;
            }
        }
        return nullptr;
    };

    QAction* newAction = findAction(QStringLiteral("New"));
    QAction* saveAction = findAction(QStringLiteral("Save"));
    QAction* clearAction = findAction(QStringLiteral("Clear Graph"));
    QAction* openAction = findAction(QStringLiteral("Open"));
    QAction* closeAction = findAction(QStringLiteral("Close Tab"));
    QAction* autoLayoutAction = findAction(QStringLiteral("Auto Layout"));

    QVERIFY(newAction != nullptr);
    QVERIFY(saveAction != nullptr);
    QVERIFY(clearAction != nullptr);
    QVERIFY(openAction != nullptr);
    QVERIFY(closeAction != nullptr);
    QVERIFY(autoLayoutAction != nullptr);

    saveSmokeArtifact(renderWidgetSnapshot(&window), QStringLiteral("ui_smoke_00_start.png"));

    const int beforeCount = window.documentCount();
    newAction->trigger();
    QCoreApplication::processEvents();
    QCOMPARE(window.documentCount(), beforeCount + 1);
    saveSmokeArtifact(renderWidgetSnapshot(&window), QStringLiteral("ui_smoke_01_after_new.png"));

    EditorScene* scene = window.activeScene();
    QVERIFY(scene != nullptr);
    NodeItem* n1 = scene->createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(640.0, 320.0));
    NodeItem* n2 = scene->createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(220.0, 120.0));
    NodeItem* n3 = scene->createNodeWithUndo(QStringLiteral("tm_Node"), QPointF(380.0, 500.0));
    QVERIFY(n1 != nullptr);
    QVERIFY(n2 != nullptr);
    QVERIFY(n3 != nullptr);
    QVERIFY(scene->createEdgeWithUndo(n2->firstOutputPort(), n1->firstInputPort()) != nullptr);
    QVERIFY(scene->createEdgeWithUndo(n1->firstOutputPort(), n3->firstInputPort()) != nullptr);
    QCoreApplication::processEvents();
    QVERIFY(window.isDocumentDirty(window.activeDocumentIndex()));

    const QPointF p1Before = n1->pos();
    const QPointF p2Before = n2->pos();
    const QPointF p3Before = n3->pos();
    autoLayoutAction->trigger();
    QCoreApplication::processEvents();
    QVERIFY(n2->pos().x() < n1->pos().x());
    QVERIFY(n1->pos().x() < n3->pos().x());
    QVERIFY(n1->pos() != p1Before || n2->pos() != p2Before || n3->pos() != p3Before);
    const QString autoLayoutArtifactName = QStringLiteral("ui_smoke_015_after_auto_layout.png");
    saveSmokeArtifact(renderWidgetSnapshot(&window), autoLayoutArtifactName);
    const QString autoLayoutArtifactPath = QDir(snapshotArtifactDir()).filePath(autoLayoutArtifactName);
    QVERIFY2(QFileInfo::exists(autoLayoutArtifactPath), qPrintable(autoLayoutArtifactPath));

    saveAction->trigger();
    QCoreApplication::processEvents();
    QVERIFY(QFileInfo::exists(savePath));
    QVERIFY(!window.isDocumentDirty(window.activeDocumentIndex()));
    saveSmokeArtifact(renderWidgetSnapshot(&window), QStringLiteral("ui_smoke_02_after_save.png"));

    clearAction->trigger();
    QCoreApplication::processEvents();
    QVERIFY(window.isDocumentDirty(window.activeDocumentIndex()));
    saveSmokeArtifact(renderWidgetSnapshot(&window), QStringLiteral("ui_smoke_03_after_clear.png"));

    // Prepare open target by saving current graph to dedicated path.
    QString error;
    QVERIFY(GraphSerializer::saveToFile(scene->toDocument(), openPath, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    openAction->trigger();
    QCoreApplication::processEvents();
    QCOMPARE(window.documentFilePath(window.activeDocumentIndex()), openPath);
    saveSmokeArtifact(renderWidgetSnapshot(&window), QStringLiteral("ui_smoke_04_after_open.png"));

    const int countBeforeClose = window.documentCount();
    closeAction->trigger();
    QCoreApplication::processEvents();
    QCOMPARE(window.documentCount(), countBeforeClose - 1);
    saveSmokeArtifact(renderWidgetSnapshot(&window), QStringLiteral("ui_smoke_05_after_close.png"));
}

void EdaSuite::layoutSettingsMarkDirty() {
    MainWindow window;
    const int index = window.activeDocumentIndex();
    QVERIFY(index >= 0);
    EditorScene* scene = window.activeScene();
    QVERIFY(scene != nullptr);

    const bool initialDirty = window.isDocumentDirty(index);
    if (initialDirty) {
        QSKIP("Initial document unexpectedly dirty; skipping dirty-state assertion.");
    }

    scene->setAutoLayoutMode(AutoLayoutMode::Grid);
    QCoreApplication::processEvents();
    QVERIFY(window.isDocumentDirty(index));
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
