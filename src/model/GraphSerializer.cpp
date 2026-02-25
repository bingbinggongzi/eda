#include "GraphSerializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QJsonObject toJson(const PortData& port) {
    QJsonObject o;
    o[QStringLiteral("id")] = port.id;
    o[QStringLiteral("name")] = port.name;
    o[QStringLiteral("direction")] = port.direction;
    return o;
}

QJsonObject toJson(const NodeData& node) {
    QJsonObject o;
    o[QStringLiteral("id")] = node.id;
    o[QStringLiteral("type")] = node.type;
    o[QStringLiteral("name")] = node.name;
    o[QStringLiteral("x")] = node.position.x();
    o[QStringLiteral("y")] = node.position.y();
    o[QStringLiteral("w")] = node.size.width();
    o[QStringLiteral("h")] = node.size.height();

    QJsonArray ports;
    for (const PortData& p : node.ports) {
        ports.append(toJson(p));
    }
    o[QStringLiteral("ports")] = ports;
    return o;
}

QJsonObject toJson(const EdgeData& edge) {
    QJsonObject o;
    o[QStringLiteral("id")] = edge.id;
    o[QStringLiteral("fromNodeId")] = edge.fromNodeId;
    o[QStringLiteral("fromPortId")] = edge.fromPortId;
    o[QStringLiteral("toNodeId")] = edge.toNodeId;
    o[QStringLiteral("toPortId")] = edge.toPortId;
    return o;
}

bool fromJson(const QJsonObject& o, PortData* out) {
    if (!out) {
        return false;
    }
    out->id = o.value(QStringLiteral("id")).toString();
    out->name = o.value(QStringLiteral("name")).toString();
    out->direction = o.value(QStringLiteral("direction")).toString();
    return !out->id.isEmpty();
}

bool fromJson(const QJsonObject& o, NodeData* out) {
    if (!out) {
        return false;
    }

    out->id = o.value(QStringLiteral("id")).toString();
    out->type = o.value(QStringLiteral("type")).toString();
    out->name = o.value(QStringLiteral("name")).toString();
    out->position = QPointF(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble());
    out->size = QSizeF(o.value(QStringLiteral("w")).toDouble(120.0), o.value(QStringLiteral("h")).toDouble(72.0));
    out->ports.clear();

    const QJsonArray ports = o.value(QStringLiteral("ports")).toArray();
    for (const QJsonValue& value : ports) {
        PortData port;
        if (fromJson(value.toObject(), &port)) {
            out->ports.append(port);
        }
    }
    return !out->id.isEmpty();
}

bool fromJson(const QJsonObject& o, EdgeData* out) {
    if (!out) {
        return false;
    }
    out->id = o.value(QStringLiteral("id")).toString();
    out->fromNodeId = o.value(QStringLiteral("fromNodeId")).toString();
    out->fromPortId = o.value(QStringLiteral("fromPortId")).toString();
    out->toNodeId = o.value(QStringLiteral("toNodeId")).toString();
    out->toPortId = o.value(QStringLiteral("toPortId")).toString();
    return !out->id.isEmpty() && !out->fromPortId.isEmpty() && !out->toPortId.isEmpty();
}
}  // namespace

bool GraphSerializer::saveToFile(const GraphDocument& document, const QString& filePath, QString* errorMessage) {
    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = document.schemaVersion;

    QJsonArray nodes;
    for (const NodeData& n : document.nodes) {
        nodes.append(toJson(n));
    }
    root[QStringLiteral("nodes")] = nodes;

    QJsonArray edges;
    for (const EdgeData& e : document.edges) {
        edges.append(toJson(e));
    }
    root[QStringLiteral("edges")] = edges;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot open file for write: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Write failed: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool GraphSerializer::loadFromFile(GraphDocument* document, const QString& filePath, QString* errorMessage) {
    if (!document) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Document pointer is null");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot open file for read: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray payload = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = json.object();
    document->schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);
    document->nodes.clear();
    document->edges.clear();

    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue& value : nodes) {
        NodeData node;
        if (fromJson(value.toObject(), &node)) {
            document->nodes.append(node);
        }
    }

    const QJsonArray edges = root.value(QStringLiteral("edges")).toArray();
    for (const QJsonValue& value : edges) {
        EdgeData edge;
        if (fromJson(value.toObject(), &edge)) {
            document->edges.append(edge);
        }
    }

    return true;
}
