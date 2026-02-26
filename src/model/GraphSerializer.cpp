#include "GraphSerializer.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

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
    o[QStringLiteral("rotation")] = node.rotationDegrees;
    o[QStringLiteral("z")] = node.z;
    if (!node.groupId.isEmpty()) {
        o[QStringLiteral("groupId")] = node.groupId;
    }

    QJsonArray ports;
    for (const PortData& p : node.ports) {
        ports.append(toJson(p));
    }
    o[QStringLiteral("ports")] = ports;

    QJsonArray properties;
    for (const PropertyData& prop : node.properties) {
        QJsonObject p;
        p[QStringLiteral("key")] = prop.key;
        p[QStringLiteral("type")] = prop.type;
        p[QStringLiteral("value")] = prop.value;
        properties.append(p);
    }
    o[QStringLiteral("properties")] = properties;
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
    out->rotationDegrees = o.value(QStringLiteral("rotation")).toDouble(0.0);
    out->z = o.value(QStringLiteral("z")).toDouble(1.0);
    out->groupId = o.value(QStringLiteral("groupId")).toString();
    out->ports.clear();
    out->properties.clear();

    const QJsonArray ports = o.value(QStringLiteral("ports")).toArray();
    for (const QJsonValue& value : ports) {
        PortData port;
        if (fromJson(value.toObject(), &port)) {
            out->ports.append(port);
        }
    }

    const QJsonArray properties = o.value(QStringLiteral("properties")).toArray();
    for (const QJsonValue& value : properties) {
        const QJsonObject p = value.toObject();
        const QString key = p.value(QStringLiteral("key")).toString();
        if (key.isEmpty()) {
            continue;
        }
        const QString type = p.value(QStringLiteral("type")).toString(QStringLiteral("string"));
        const QString val = p.value(QStringLiteral("value")).toString();
        out->properties.push_back(PropertyData{key, type, val});
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
    return !out->fromNodeId.isEmpty() && !out->toNodeId.isEmpty();
}

bool migrateToCurrent(GraphDocument* document, QString* errorMessage) {
    if (!document) {
        return false;
    }
    if (document->schemaVersion == 0) {
        // Legacy migration path:
        // - ensure every node has at least one input and one output port
        // - backfill edge ids and missing port ids
        QHash<QString, QString> nodeFirstIn;
        QHash<QString, QString> nodeFirstOut;
        int generatedPortCounter = 1;

        for (NodeData& node : document->nodes) {
            bool hasIn = false;
            bool hasOut = false;
            for (const PortData& port : node.ports) {
                if (port.direction == QStringLiteral("input")) {
                    hasIn = true;
                    if (!nodeFirstIn.contains(node.id)) {
                        nodeFirstIn.insert(node.id, port.id);
                    }
                } else if (port.direction == QStringLiteral("output")) {
                    hasOut = true;
                    if (!nodeFirstOut.contains(node.id)) {
                        nodeFirstOut.insert(node.id, port.id);
                    }
                }
            }

            if (!hasIn) {
                const QString pid = QStringLiteral("P_MIG_%1").arg(generatedPortCounter++);
                node.ports.push_back(PortData{pid, QStringLiteral("in1"), QStringLiteral("input")});
                nodeFirstIn.insert(node.id, pid);
            }
            if (!hasOut) {
                const QString pid = QStringLiteral("P_MIG_%1").arg(generatedPortCounter++);
                node.ports.push_back(PortData{pid, QStringLiteral("out1"), QStringLiteral("output")});
                nodeFirstOut.insert(node.id, pid);
            }
        }

        int generatedEdgeCounter = 1;
        for (EdgeData& edge : document->edges) {
            if (edge.id.isEmpty()) {
                edge.id = QStringLiteral("E_MIG_%1").arg(generatedEdgeCounter++);
            }
            if (edge.fromPortId.isEmpty()) {
                edge.fromPortId = nodeFirstOut.value(edge.fromNodeId);
            }
            if (edge.toPortId.isEmpty()) {
                edge.toPortId = nodeFirstIn.value(edge.toNodeId);
            }
            if (edge.fromPortId.isEmpty() || edge.toPortId.isEmpty()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Migration failed: cannot map edge ports for edge %1").arg(edge.id);
                }
                return false;
            }
        }

        document->schemaVersion = 1;
        return true;
    }

    if (document->schemaVersion == 1) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Unsupported schemaVersion: %1").arg(document->schemaVersion);
    }
    return false;
}
}  // namespace

bool GraphSerializer::saveToFile(const GraphDocument& document, const QString& filePath, QString* errorMessage) {
    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = document.schemaVersion;
    root[QStringLiteral("autoLayoutMode")] = document.autoLayoutMode;
    root[QStringLiteral("autoLayoutXSpacing")] = document.autoLayoutXSpacing;
    root[QStringLiteral("autoLayoutYSpacing")] = document.autoLayoutYSpacing;
    root[QStringLiteral("edgeBundlePolicy")] = document.edgeBundlePolicy;
    root[QStringLiteral("edgeBundleSpacing")] = document.edgeBundleSpacing;
    QJsonArray collapsedGroups;
    for (const QString& groupId : document.collapsedGroupIds) {
        if (!groupId.isEmpty()) {
            collapsedGroups.append(groupId);
        }
    }
    root[QStringLiteral("collapsedGroups")] = collapsedGroups;

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
    if (root.contains(QStringLiteral("schemaVersion"))) {
        document->schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);
    } else {
        document->schemaVersion = 1;
    }
    document->autoLayoutMode = root.value(QStringLiteral("autoLayoutMode")).toString(QStringLiteral("layered"));
    if (document->autoLayoutMode.compare(QStringLiteral("grid"), Qt::CaseInsensitive) == 0) {
        document->autoLayoutMode = QStringLiteral("grid");
    } else {
        document->autoLayoutMode = QStringLiteral("layered");
    }
    document->autoLayoutXSpacing = std::max(40.0, root.value(QStringLiteral("autoLayoutXSpacing")).toDouble(240.0));
    document->autoLayoutYSpacing = std::max(40.0, root.value(QStringLiteral("autoLayoutYSpacing")).toDouble(140.0));
    document->edgeBundlePolicy = root.value(QStringLiteral("edgeBundlePolicy")).toString(QStringLiteral("centered"));
    if (document->edgeBundlePolicy.compare(QStringLiteral("directional"), Qt::CaseInsensitive) == 0) {
        document->edgeBundlePolicy = QStringLiteral("directional");
    } else {
        document->edgeBundlePolicy = QStringLiteral("centered");
    }
    document->edgeBundleSpacing = std::max(0.0, root.value(QStringLiteral("edgeBundleSpacing")).toDouble(18.0));
    document->collapsedGroupIds.clear();
    const QJsonArray collapsedGroups = root.value(QStringLiteral("collapsedGroups")).toArray();
    for (const QJsonValue& value : collapsedGroups) {
        const QString groupId = value.toString();
        if (!groupId.isEmpty()) {
            document->collapsedGroupIds.push_back(groupId);
        }
    }
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

    return migrateToCurrent(document, errorMessage);
}
