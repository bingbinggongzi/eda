#include "EdgeItem.h"

#include "NodeItem.h"
#include "PortItem.h"

#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>
#include <QSet>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace {
constexpr qreal kAnchorOffset = 24.0;
constexpr qreal kGridStep = 20.0;
constexpr qreal kObstaclePadding = 14.0;
constexpr int kMaxVisitedCells = 80000;
constexpr int kStepCost = 10;
constexpr int kTurnPenalty = 7;
constexpr int kReversePenalty = 12;
constexpr int kStartDirectionPenalty = 20;
constexpr int kGoalDirectionPenalty = 18;
constexpr int kDirectionPreferenceDepth = 2;

struct Cell {
    int x = 0;
    int y = 0;

    bool operator==(const Cell& other) const {
        return x == other.x && y == other.y;
    }
};

enum class RouteDir : quint8 {
    None = 0,
    Right,
    Left,
    Down,
    Up
};

struct StateKey {
    Cell cell;
    RouteDir dir = RouteDir::None;

    bool operator==(const StateKey& other) const {
        return cell == other.cell && dir == other.dir;
    }
};

size_t qHash(const StateKey& key, size_t seed = 0) noexcept {
    const quint64 packed = (static_cast<quint64>(static_cast<quint32>(key.cell.x)) << 32) |
                           static_cast<quint32>(key.cell.y);
    return ::qHash(packed ^ (static_cast<quint64>(key.dir) << 1), seed);
}

bool almostEqual(qreal a, qreal b) {
    return std::abs(a - b) < 0.1;
}

bool samePoint(const QPointF& a, const QPointF& b) {
    return almostEqual(a.x(), b.x()) && almostEqual(a.y(), b.y());
}

struct BundleMetrics {
    qreal offset = 0.0;
    int siblingCount = 0;
};

BundleMetrics computeBundleMetrics(const EdgeItem* edge) {
    BundleMetrics metrics;
    if (!edge || !edge->scene() || !edge->sourcePort() || !edge->targetPort()) {
        return metrics;
    }

    const NodeItem* sourceNode = edge->sourcePort()->ownerNode();
    const NodeItem* targetNode = edge->targetPort()->ownerNode();
    if (!sourceNode || !targetNode) {
        return metrics;
    }

    QVector<const EdgeItem*> siblings;
    const QList<QGraphicsItem*> sceneItems = edge->scene()->items();
    siblings.reserve(sceneItems.size());

    for (QGraphicsItem* item : sceneItems) {
        const EdgeItem* other = dynamic_cast<const EdgeItem*>(item);
        if (!other || !other->sourcePort() || !other->targetPort()) {
            continue;
        }
        const NodeItem* otherSource = other->sourcePort()->ownerNode();
        const NodeItem* otherTarget = other->targetPort()->ownerNode();
        if (!otherSource || !otherTarget) {
            continue;
        }
        if (otherSource->nodeId() == sourceNode->nodeId() && otherTarget->nodeId() == targetNode->nodeId()) {
            siblings.push_back(other);
        }
    }

    metrics.siblingCount = siblings.size();
    if (siblings.size() < 2) {
        return metrics;
    }

    const QPointF from = sourceNode->sceneBoundingRect().center();
    const QPointF to = targetNode->sceneBoundingRect().center();
    const bool horizontalDominant = std::abs(to.x() - from.x()) >= std::abs(to.y() - from.y());

    if (edge->routingProfile() == EdgeRoutingProfile::Dense) {
        std::sort(siblings.begin(), siblings.end(), [horizontalDominant, from, to](const EdgeItem* a, const EdgeItem* b) {
            const QPointF aSource = a->sourcePort()->scenePos();
            const QPointF aTarget = a->targetPort()->scenePos();
            const QPointF bSource = b->sourcePort()->scenePos();
            const QPointF bTarget = b->targetPort()->scenePos();

            if (horizontalDominant) {
                const bool leftToRight = from.x() <= to.x();
                const qreal aKey = leftToRight ? aTarget.y() : -aTarget.y();
                const qreal bKey = leftToRight ? bTarget.y() : -bTarget.y();
                if (!qFuzzyCompare(aKey + 1.0, bKey + 1.0)) {
                    return aKey < bKey;
                }
                if (!qFuzzyCompare(aSource.y() + 1.0, bSource.y() + 1.0)) {
                    return aSource.y() < bSource.y();
                }
            } else {
                const bool topToBottom = from.y() <= to.y();
                const qreal aKey = topToBottom ? aTarget.x() : -aTarget.x();
                const qreal bKey = topToBottom ? bTarget.x() : -bTarget.x();
                if (!qFuzzyCompare(aKey + 1.0, bKey + 1.0)) {
                    return aKey < bKey;
                }
                if (!qFuzzyCompare(aSource.x() + 1.0, bSource.x() + 1.0)) {
                    return aSource.x() < bSource.x();
                }
            }
            return a->edgeId() < b->edgeId();
        });
    } else {
        std::sort(
            siblings.begin(), siblings.end(), [](const EdgeItem* a, const EdgeItem* b) { return a->edgeId() < b->edgeId(); });
    }

    int index = 0;
    for (int i = 0; i < siblings.size(); ++i) {
        if (siblings[i] == edge) {
            index = i;
            break;
        }
    }

    qreal spacing = std::max<qreal>(0.0, edge->bundleSpacing());
    if (edge->routingProfile() == EdgeRoutingProfile::Dense) {
        const int extraSiblings = std::max(0, static_cast<int>(siblings.size()) - 2);
        spacing += std::min<qreal>(26.0, static_cast<qreal>(extraSiblings) * 4.0);
    }
    const qreal centeredIndex = static_cast<qreal>(index) - (static_cast<qreal>(siblings.size() - 1) * 0.5);
    metrics.offset = centeredIndex * spacing;
    return metrics;
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

RouteDir routeDirFromDelta(const Cell& delta) {
    if (delta.x > 0) {
        return RouteDir::Right;
    }
    if (delta.x < 0) {
        return RouteDir::Left;
    }
    if (delta.y > 0) {
        return RouteDir::Down;
    }
    if (delta.y < 0) {
        return RouteDir::Up;
    }
    return RouteDir::None;
}

bool isOpposite(RouteDir a, RouteDir b) {
    return (a == RouteDir::Left && b == RouteDir::Right) || (a == RouteDir::Right && b == RouteDir::Left) ||
           (a == RouteDir::Up && b == RouteDir::Down) || (a == RouteDir::Down && b == RouteDir::Up);
}

int weightedHeuristic(const Cell& from, const Cell& goal, RouteDir currentDir, RouteDir preferredGoalDir) {
    int heuristic = manhattan(from, goal) * kStepCost;
    if (currentDir != RouteDir::None && preferredGoalDir != RouteDir::None && currentDir != preferredGoalDir) {
        heuristic += 2;
    }
    return heuristic;
}

bool areCollinear(const QPointF& a, const QPointF& b, const QPointF& c) {
    return (almostEqual(a.x(), b.x()) && almostEqual(b.x(), c.x())) || (almostEqual(a.y(), b.y()) && almostEqual(b.y(), c.y()));
}

bool isSameDirectionCollinear(const QPointF& a, const QPointF& b, const QPointF& c) {
    if (!areCollinear(a, b, c)) {
        return false;
    }
    if (almostEqual(a.x(), b.x()) && almostEqual(b.x(), c.x())) {
        const qreal first = b.y() - a.y();
        const qreal second = c.y() - b.y();
        return first * second >= 0.0;
    }

    const qreal first = b.x() - a.x();
    const qreal second = c.x() - b.x();
    return first * second >= 0.0;
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
                                   const NodeItem* targetNode,
                                   RouteDir preferredStartDir,
                                   RouteDir preferredGoalDir) {
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
        StateKey state;
    };

    auto compare = [](const OpenEntry& a, const OpenEntry& b) {
        if (a.f != b.f) {
            return a.f > b.f;
        }
        return a.g > b.g;
    };

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, decltype(compare)> open(compare);
    QHash<StateKey, int> gScore;
    QHash<StateKey, StateKey> cameFrom;
    QSet<StateKey> closed;

    const StateKey startState{startCell, RouteDir::None};
    open.push(OpenEntry{weightedHeuristic(startCell, goalCell, RouteDir::None, preferredGoalDir), 0, startState});
    gScore.insert(startState, 0);

    int visited = 0;
    while (!open.empty() && visited < kMaxVisitedCells) {
        const OpenEntry current = open.top();
        open.pop();

        if (closed.contains(current.state)) {
            continue;
        }
        if (current.state.cell == goalCell) {
            QVector<Cell> cells;
            StateKey cursor = current.state;
            cells.push_front(cursor.cell);
            while (!(cursor == startState)) {
                if (!cameFrom.contains(cursor)) {
                    return {};
                }
                cursor = cameFrom.value(cursor);
                cells.push_front(cursor.cell);
            }

            QVector<QPointF> route;
            route.reserve(cells.size());
            for (const Cell& cell : cells) {
                const QPointF pt = cellToPoint(cell);
                if (route.size() >= 2 && isSameDirectionCollinear(route[route.size() - 2], route.last(), pt)) {
                    route.back() = pt;
                } else {
                    route.push_back(pt);
                }
            }
            return route;
        }

        closed.insert(current.state);
        ++visited;

        static const Cell kDirections[] = {Cell{1, 0}, Cell{-1, 0}, Cell{0, 1}, Cell{0, -1}};
        for (const Cell& step : kDirections) {
            const RouteDir stepDir = routeDirFromDelta(step);
            const Cell next{current.state.cell.x + step.x, current.state.cell.y + step.y};
            if (!inBounds(next) || isBlocked(next)) {
                continue;
            }

            const StateKey nextState{next, stepDir};
            if (closed.contains(nextState)) {
                continue;
            }

            int stepCost = kStepCost;
            if (current.state.dir != RouteDir::None && current.state.dir != stepDir) {
                stepCost += kTurnPenalty;
                if (isOpposite(current.state.dir, stepDir)) {
                    stepCost += kReversePenalty;
                }
            }

            const int startDistance = manhattan(startCell, current.state.cell);
            if (preferredStartDir != RouteDir::None && startDistance < kDirectionPreferenceDepth &&
                isOpposite(stepDir, preferredStartDir)) {
                continue;
            }
            if (preferredStartDir != RouteDir::None && startDistance < kDirectionPreferenceDepth && stepDir != preferredStartDir) {
                stepCost += kStartDirectionPenalty;
            }

            const int goalDistance = manhattan(next, goalCell);
            if (preferredGoalDir != RouteDir::None && goalDistance <= kDirectionPreferenceDepth && stepDir != preferredGoalDir) {
                stepCost += kGoalDirectionPenalty;
            }

            const int tentativeG = current.g + stepCost;
            const int known = gScore.value(nextState, std::numeric_limits<int>::max());
            if (tentativeG >= known) {
                continue;
            }

            cameFrom.insert(nextState, current.state);
            gScore.insert(nextState, tentativeG);
            open.push(OpenEntry{tentativeG + weightedHeuristic(next, goalCell, stepDir, preferredGoalDir), tentativeG, nextState});
        }
    }

    return {};
}

QPainterPath buildManhattanPath(const QPointF& start,
                                const QPointF& end,
                                const QPointF& startAnchor,
                                const QPointF& endAnchor,
                                qreal bundleOffset,
                                EdgeBundlePolicy bundlePolicy) {
    QPainterPath path(start);
    appendLineTo(&path, startAnchor);

    const qreal dx = std::abs(endAnchor.x() - startAnchor.x());
    const qreal dy = std::abs(endAnchor.y() - startAnchor.y());
    const bool verticalDominant = dy > dx;

    if (bundlePolicy == EdgeBundlePolicy::Directional) {
        if (verticalDominant) {
            const qreal midY = (startAnchor.y() + endAnchor.y()) * 0.5;
            const qreal shiftedX = startAnchor.x() + bundleOffset;
            appendLineTo(&path, QPointF(shiftedX, startAnchor.y()));
            appendLineTo(&path, QPointF(shiftedX, midY));
            appendLineTo(&path, QPointF(endAnchor.x() + bundleOffset, midY));
            appendLineTo(&path, QPointF(endAnchor.x() + bundleOffset, endAnchor.y()));
        } else {
            const qreal midX = (startAnchor.x() + endAnchor.x()) * 0.5;
            const qreal shiftedY = startAnchor.y() + bundleOffset;
            appendLineTo(&path, QPointF(startAnchor.x(), shiftedY));
            appendLineTo(&path, QPointF(midX, shiftedY));
            appendLineTo(&path, QPointF(midX, endAnchor.y() + bundleOffset));
            appendLineTo(&path, QPointF(endAnchor.x(), endAnchor.y() + bundleOffset));
        }
    } else {
        const qreal midX = ((startAnchor.x() + endAnchor.x()) * 0.5) + bundleOffset;
        appendLineTo(&path, QPointF(midX, startAnchor.y()));
        appendLineTo(&path, QPointF(midX, endAnchor.y()));
    }

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
                              const NodeItem* targetNode,
                              qreal bundleOffset,
                              EdgeBundlePolicy bundlePolicy) {
    auto preferredExitDirection = [](qreal offset) {
        if (std::abs(offset) < 0.1) {
            return RouteDir::None;
        }
        return offset > 0.0 ? RouteDir::Right : RouteDir::Left;
    };
    auto preferredArrivalDirection = [](qreal offset) {
        if (std::abs(offset) < 0.1) {
            return RouteDir::None;
        }
        return offset > 0.0 ? RouteDir::Left : RouteDir::Right;
    };

    const qreal dx = std::abs(endAnchor.x() - startAnchor.x());
    const qreal dy = std::abs(endAnchor.y() - startAnchor.y());
    const bool verticalDominant = dy > dx;
    const bool directional = (bundlePolicy == EdgeBundlePolicy::Directional);
    const QPointF routedStartAnchor = directional
        ? (verticalDominant ? QPointF(startAnchor.x() + bundleOffset, startAnchor.y())
                            : QPointF(startAnchor.x(), startAnchor.y() + bundleOffset))
        : QPointF(startAnchor.x() + bundleOffset, startAnchor.y());
    const QPointF routedEndAnchor = directional
        ? (verticalDominant ? QPointF(endAnchor.x() + bundleOffset, endAnchor.y())
                            : QPointF(endAnchor.x(), endAnchor.y() + bundleOffset))
        : QPointF(endAnchor.x() + bundleOffset, endAnchor.y());

    QPainterPath path(start);
    appendLineTo(&path, startAnchor);
    appendLineTo(&path, routedStartAnchor);

    const QVector<QPointF> route = findObstacleRoute(routedStartAnchor,
                                                     routedEndAnchor,
                                                     scene,
                                                     sourceNode,
                                                     targetNode,
                                                     preferredExitDirection(startAnchor.x() - start.x()),
                                                     preferredArrivalDirection(endAnchor.x() - end.x()));
    if (route.isEmpty()) {
        const qreal midX = ((startAnchor.x() + endAnchor.x()) * 0.5) + bundleOffset;
        if (directional && verticalDominant) {
            const qreal midY = (startAnchor.y() + endAnchor.y()) * 0.5;
            appendLineTo(&path, QPointF(startAnchor.x() + bundleOffset, midY));
            appendLineTo(&path, QPointF(endAnchor.x() + bundleOffset, midY));
        } else {
            appendLineTo(&path, QPointF(midX, startAnchor.y()));
            appendLineTo(&path, QPointF(midX, endAnchor.y()));
        }
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

    appendLineTo(&path, routedEndAnchor);
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

EdgeRoutingProfile EdgeItem::routingProfile() const {
    return m_routingProfile;
}

EdgeBundlePolicy EdgeItem::bundlePolicy() const {
    return m_bundlePolicy;
}

qreal EdgeItem::bundleSpacing() const {
    return m_bundleSpacing;
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

void EdgeItem::setRoutingProfile(EdgeRoutingProfile profile) {
    if (m_routingProfile == profile) {
        return;
    }
    m_routingProfile = profile;
    updatePath();
}

void EdgeItem::setBundlePolicy(EdgeBundlePolicy policy) {
    if (m_bundlePolicy == policy) {
        return;
    }
    m_bundlePolicy = policy;
    updatePath();
}

void EdgeItem::setBundleSpacing(qreal spacing) {
    const qreal clamped = std::max<qreal>(0.0, spacing);
    if (qFuzzyCompare(m_bundleSpacing + 1.0, clamped + 1.0)) {
        return;
    }
    m_bundleSpacing = clamped;
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
    const BundleMetrics bundleMetrics = m_targetPort ? computeBundleMetrics(this) : BundleMetrics{};
    EdgeBundlePolicy effectivePolicy = m_bundlePolicy;
    if (m_routingProfile == EdgeRoutingProfile::Dense && m_bundlePolicy == EdgeBundlePolicy::Centered &&
        bundleMetrics.siblingCount >= 3) {
        effectivePolicy = EdgeBundlePolicy::Directional;
    }

    QPainterPath path;
    if (m_routingMode == EdgeRoutingMode::ObstacleAvoiding) {
        path = buildObstaclePath(
            start, end, startAnchor, endAnchor, scene(), sourceNode, targetNode, bundleMetrics.offset, effectivePolicy);
    } else {
        path = buildManhattanPath(start, end, startAnchor, endAnchor, bundleMetrics.offset, effectivePolicy);
    }
    setPath(path);

    QPen pen(QColor(83, 83, 83), isSelected() ? 2.0 : 1.4);
    if (!m_targetPort) {
        pen.setStyle(Qt::DashLine);
        pen.setColor(QColor(64, 145, 255));
    }
    setPen(pen);
}
