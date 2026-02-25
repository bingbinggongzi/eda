#include "EdgeItem.h"

#include "NodeItem.h"
#include "PortItem.h"

#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace {
constexpr qreal kAnchorOffset = 24.0;
constexpr qreal kGridStep = 20.0;
constexpr qreal kObstaclePadding = 14.0;
constexpr int kMaxVisitedCells = 25000;

struct Cell {
    int x = 0;
    int y = 0;

    bool operator==(const Cell& other) const {
        return x == other.x && y == other.y;
    }
};

bool almostEqual(qreal a, qreal b) {
    return std::abs(a - b) < 0.1;
}

bool samePoint(const QPointF& a, const QPointF& b) {
    return almostEqual(a.x(), b.x()) && almostEqual(a.y(), b.y());
}

void appendLineTo(QPainterPath* path, const QPointF& point) {
    if (!path || samePoint(path->currentPosition(), point)) {
        return;
    }
    path->lineTo(point);
}

quint64 cellKey(const Cell& c) {
    return (static_cast<quint64>(static_cast<quint32>(c.x)) << 32) | static_cast<quint32>(c.y);
}

Cell cellFromKey(quint64 key) {
    return Cell{static_cast<int>(static_cast<qint32>(key >> 32)), static_cast<int>(static_cast<qint32>(key & 0xffffffffu))};
}

Cell pointToCell(const QPointF& p) {
    return Cell{static_cast<int>(std::round(p.x() / kGridStep)), static_cast<int>(std::round(p.y() / kGridStep))};
}

QPointF cellToPoint(const Cell& c) {
    return QPointF(c.x * kGridStep, c.y * kGridStep);
}

int manhattan(const Cell& a, const Cell& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

bool areCollinear(const QPointF& a, const QPointF& b, const QPointF& c) {
    return (almostEqual(a.x(), b.x()) && almostEqual(b.x(), c.x())) || (almostEqual(a.y(), b.y()) && almostEqual(b.y(), c.y()));
}

QVector<QRectF> collectObstacleRects(QGraphicsScene* scene, const NodeItem* sourceNode, const NodeItem* targetNode) {
    QVector<QRectF> obstacles;
    if (!scene) {
        return obstacles;
    }

    const QList<QGraphicsItem*> allItems = scene->items();
    for (QGraphicsItem* item : allItems) {
        const NodeItem* node = dynamic_cast<const NodeItem*>(item);
        if (!node || node == sourceNode || node == targetNode) {
            continue;
        }
        obstacles.push_back(node->sceneBoundingRect().adjusted(-kObstaclePadding, -kObstaclePadding, kObstaclePadding, kObstaclePadding));
    }
    return obstacles;
}

QRectF computeSearchBounds(const QPointF& startAnchor, const QPointF& endAnchor, const QVector<QRectF>& obstacles) {
    QRectF bounds = QRectF(startAnchor, endAnchor).normalized();
    if (bounds.width() < 1.0) {
        bounds.setWidth(1.0);
    }
    if (bounds.height() < 1.0) {
        bounds.setHeight(1.0);
    }
    for (const QRectF& obstacle : obstacles) {
        bounds = bounds.united(obstacle);
    }
    bounds.adjust(-220.0, -220.0, 220.0, 220.0);
    return bounds;
}

QVector<QPointF> findObstacleRoute(const QPointF& startAnchor,
                                   const QPointF& endAnchor,
                                   QGraphicsScene* scene,
                                   const NodeItem* sourceNode,
                                   const NodeItem* targetNode) {
    const QVector<QRectF> obstacles = collectObstacleRects(scene, sourceNode, targetNode);
    if (obstacles.isEmpty()) {
        return {};
    }

    const Cell startCell = pointToCell(startAnchor);
    const Cell goalCell = pointToCell(endAnchor);
    const quint64 startKey = cellKey(startCell);
    const quint64 goalKey = cellKey(goalCell);

    const QRectF bounds = computeSearchBounds(startAnchor, endAnchor, obstacles);
    const int minCellX = static_cast<int>(std::floor(bounds.left() / kGridStep)) - 1;
    const int maxCellX = static_cast<int>(std::ceil(bounds.right() / kGridStep)) + 1;
    const int minCellY = static_cast<int>(std::floor(bounds.top() / kGridStep)) - 1;
    const int maxCellY = static_cast<int>(std::ceil(bounds.bottom() / kGridStep)) + 1;

    auto inBounds = [&](const Cell& c) {
        return c.x >= minCellX && c.x <= maxCellX && c.y >= minCellY && c.y <= maxCellY;
    };

    auto isBlocked = [&](const Cell& c) {
        const quint64 key = cellKey(c);
        if (key == startKey || key == goalKey) {
            return false;
        }
        const QPointF center = cellToPoint(c);
        for (const QRectF& obstacle : obstacles) {
            if (obstacle.contains(center)) {
                return true;
            }
        }
        return false;
    };

    struct OpenEntry {
        int f = 0;
        int g = 0;
        Cell cell;
    };

    auto compare = [](const OpenEntry& a, const OpenEntry& b) {
        if (a.f != b.f) {
            return a.f > b.f;
        }
        return a.g > b.g;
    };

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, decltype(compare)> open(compare);
    QHash<quint64, int> gScore;
    QHash<quint64, quint64> cameFrom;
    QSet<quint64> closed;

    open.push(OpenEntry{manhattan(startCell, goalCell), 0, startCell});
    gScore.insert(startKey, 0);

    int visited = 0;
    while (!open.empty() && visited < kMaxVisitedCells) {
        const OpenEntry current = open.top();
        open.pop();

        const quint64 currentKey = cellKey(current.cell);
        if (closed.contains(currentKey)) {
            continue;
        }
        if (currentKey == goalKey) {
            QVector<Cell> cells;
            cells.push_back(goalCell);
            quint64 cursor = goalKey;
            while (cursor != startKey) {
                if (!cameFrom.contains(cursor)) {
                    return {};
                }
                cursor = cameFrom.value(cursor);
                cells.push_front(cellFromKey(cursor));
            }

            QVector<QPointF> route;
            route.reserve(cells.size());
            for (const Cell& cell : cells) {
                const QPointF pt = cellToPoint(cell);
                if (route.size() >= 2 && areCollinear(route[route.size() - 2], route.last(), pt)) {
                    route.back() = pt;
                } else {
                    route.push_back(pt);
                }
            }
            return route;
        }

        closed.insert(currentKey);
        ++visited;

        static const Cell kDirections[] = {Cell{1, 0}, Cell{-1, 0}, Cell{0, 1}, Cell{0, -1}};
        for (const Cell& step : kDirections) {
            const Cell next{current.cell.x + step.x, current.cell.y + step.y};
            if (!inBounds(next) || isBlocked(next)) {
                continue;
            }

            const quint64 nextKey = cellKey(next);
            if (closed.contains(nextKey)) {
                continue;
            }

            const int tentativeG = current.g + 1;
            const int known = gScore.value(nextKey, std::numeric_limits<int>::max());
            if (tentativeG >= known) {
                continue;
            }

            cameFrom.insert(nextKey, currentKey);
            gScore.insert(nextKey, tentativeG);
            open.push(OpenEntry{tentativeG + manhattan(next, goalCell), tentativeG, next});
        }
    }

    return {};
}

QPainterPath buildManhattanPath(const QPointF& start, const QPointF& end, const QPointF& startAnchor, const QPointF& endAnchor) {
    QPainterPath path(start);
    appendLineTo(&path, startAnchor);

    const qreal midX = (startAnchor.x() + endAnchor.x()) * 0.5;
    appendLineTo(&path, QPointF(midX, startAnchor.y()));
    appendLineTo(&path, QPointF(midX, endAnchor.y()));

    appendLineTo(&path, endAnchor);
    appendLineTo(&path, end);
    return path;
}

QPainterPath buildObstaclePath(const QPointF& start,
                              const QPointF& end,
                              const QPointF& startAnchor,
                              const QPointF& endAnchor,
                              QGraphicsScene* scene,
                              const NodeItem* sourceNode,
                              const NodeItem* targetNode) {
    QPainterPath path(start);
    appendLineTo(&path, startAnchor);

    const QVector<QPointF> route = findObstacleRoute(startAnchor, endAnchor, scene, sourceNode, targetNode);
    if (route.isEmpty()) {
        const qreal midX = (startAnchor.x() + endAnchor.x()) * 0.5;
        appendLineTo(&path, QPointF(midX, startAnchor.y()));
        appendLineTo(&path, QPointF(midX, endAnchor.y()));
    } else {
        const QPointF routedStart = route.first();
        const QPointF routedEnd = route.last();

        appendLineTo(&path, QPointF(routedStart.x(), startAnchor.y()));
        appendLineTo(&path, routedStart);
        for (int i = 1; i < route.size() - 1; ++i) {
            appendLineTo(&path, route[i]);
        }
        appendLineTo(&path, routedEnd);
        appendLineTo(&path, QPointF(routedEnd.x(), endAnchor.y()));
    }

    appendLineTo(&path, endAnchor);
    appendLineTo(&path, end);
    return path;
}

qreal startAnchorOffset(const PortItem* sourcePort) {
    if (!sourcePort) {
        return kAnchorOffset;
    }
    return sourcePort->direction() == PortDirection::Output ? kAnchorOffset : -kAnchorOffset;
}

qreal endAnchorOffset(const PortItem* sourcePort, const PortItem* targetPort, const QPointF& start, const QPointF& end) {
    if (targetPort) {
        return targetPort->direction() == PortDirection::Input ? -kAnchorOffset : kAnchorOffset;
    }
    const qreal sourceOffset = startAnchorOffset(sourcePort);
    return (end.x() >= start.x()) ? -sourceOffset : sourceOffset;
}
}  // namespace

EdgeItem::EdgeItem(const QString& edgeId, PortItem* sourcePort, QGraphicsItem* parent)
    : QGraphicsPathItem(parent),
      m_edgeId(edgeId),
      m_sourcePort(sourcePort),
      m_previewEnd(sourcePort ? sourcePort->scenePos() : QPointF()) {
    setZValue(-1.0);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    if (m_sourcePort) {
        m_sourcePort->addEdge(this);
    }
    updatePath();
}

EdgeItem::~EdgeItem() {
    if (m_sourcePort) {
        m_sourcePort->removeEdge(this);
    }
    if (m_targetPort) {
        m_targetPort->removeEdge(this);
    }
}

const QString& EdgeItem::edgeId() const {
    return m_edgeId;
}

PortItem* EdgeItem::sourcePort() const {
    return m_sourcePort;
}

PortItem* EdgeItem::targetPort() const {
    return m_targetPort;
}

EdgeRoutingMode EdgeItem::routingMode() const {
    return m_routingMode;
}

void EdgeItem::setTargetPort(PortItem* port) {
    if (m_targetPort == port) {
        return;
    }
    if (m_targetPort) {
        m_targetPort->removeEdge(this);
    }
    m_targetPort = port;
    if (m_targetPort) {
        m_targetPort->addEdge(this);
    }
    updatePath();
}

void EdgeItem::setPreviewEnd(const QPointF& scenePos) {
    m_previewEnd = scenePos;
    updatePath();
}

void EdgeItem::setRoutingMode(EdgeRoutingMode mode) {
    if (m_routingMode == mode) {
        return;
    }
    m_routingMode = mode;
    updatePath();
}

void EdgeItem::updatePath() {
    if (!m_sourcePort) {
        return;
    }

    const QPointF start = m_sourcePort->scenePos();
    const QPointF end = m_targetPort ? m_targetPort->scenePos() : m_previewEnd;
    const NodeItem* sourceNode = m_sourcePort->ownerNode();
    const NodeItem* targetNode = m_targetPort ? m_targetPort->ownerNode() : nullptr;

    const QPointF startAnchor(start.x() + startAnchorOffset(m_sourcePort), start.y());
    const QPointF endAnchor(end.x() + endAnchorOffset(m_sourcePort, m_targetPort, start, end), end.y());

    QPainterPath path;
    if (m_routingMode == EdgeRoutingMode::ObstacleAvoiding) {
        path = buildObstaclePath(start, end, startAnchor, endAnchor, scene(), sourceNode, targetNode);
    } else {
        path = buildManhattanPath(start, end, startAnchor, endAnchor);
    }
    setPath(path);

    QPen pen(QColor(83, 83, 83), isSelected() ? 2.0 : 1.4);
    if (!m_targetPort) {
        pen.setStyle(Qt::DashLine);
        pen.setColor(QColor(64, 145, 255));
    }
    setPen(pen);
}
