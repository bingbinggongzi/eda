#pragma once

#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

struct PortData {
    QString id;
    QString name;
    QString direction;  // "input" or "output"
};

struct PropertyData {
    QString key;
    QString type;   // "string" | "int" | "double" | "bool"
    QString value;
};

struct LayerData {
    QString id;
    QString name;
    bool visible = true;
    bool locked = false;
};

struct NodeData {
    QString id;
    QString type;
    QString name;
    QPointF position;
    QSizeF size;
    QVector<PortData> ports;
    QVector<PropertyData> properties;
    qreal rotationDegrees = 0.0;
    qreal z = 1.0;
    QString groupId;
    QString layerId;
};

struct EdgeData {
    QString id;
    QString fromNodeId;
    QString fromPortId;
    QString toNodeId;
    QString toPortId;
};

struct GraphDocument {
    int schemaVersion = 1;
    QString autoLayoutMode = QStringLiteral("layered");
    qreal autoLayoutXSpacing = 240.0;
    qreal autoLayoutYSpacing = 140.0;
    QVector<LayerData> layers;
    QString activeLayerId;
    QString edgeRoutingProfile = QStringLiteral("balanced");
    QString edgeBundlePolicy = QStringLiteral("centered");
    QString edgeBundleScope = QStringLiteral("global");
    qreal edgeBundleSpacing = 18.0;
    QVector<QString> collapsedGroupIds;
    QVector<NodeData> nodes;
    QVector<EdgeData> edges;
};
