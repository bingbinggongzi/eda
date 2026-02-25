#pragma once

#include <QGraphicsScene>
#include <QPointF>
#include <QString>

class EdgeItem;
class NodeItem;
class PortItem;

class EditorScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit EditorScene(QObject* parent = nullptr);

    NodeItem* createNode(const QString& typeName, const QPointF& scenePos);
    EdgeItem* createEdge(PortItem* outputPort, PortItem* inputPort);

    void setSnapToGrid(bool enabled);
    bool snapToGrid() const;
    int gridSize() const;

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
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private slots:
    void onPortConnectionStart(PortItem* port);
    void onPortConnectionRelease(PortItem* port);
    void onSelectionChangedInternal();

private:
    QString nextNodeId();
    QString nextPortId();
    QString nextEdgeId();
    QPointF snapPoint(const QPointF& p) const;
    bool canConnect(PortItem* a, PortItem* b) const;
    PortItem* pickPortAt(const QPointF& scenePos) const;
    void finishConnectionAt(const QPointF& scenePos, PortItem* explicitTarget = nullptr);
    NodeItem* buildNodeByType(const QString& typeName);

    int m_nodeCounter = 1;
    int m_portCounter = 1;
    int m_edgeCounter = 1;
    bool m_snapToGrid = true;

    PortItem* m_pendingPort = nullptr;
    EdgeItem* m_previewEdge = nullptr;
};
