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
    QVector<NodeData> nodes;
    QVector<EdgeData> edges;
};
