#pragma once

#include "GraphDocument.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

struct ComponentSpec {
    QString typeName;
    QString displayName;
    QString category;
    QSizeF size;
    int inputCount = 1;
    int outputCount = 1;
    QVector<PropertyData> defaultProperties;
};

class ComponentCatalog {
public:
    static const ComponentCatalog& instance();

    const ComponentSpec* find(const QString& typeName) const;
    const ComponentSpec& fallback() const;
    QStringList categories() const;
    QStringList typesInCategory(const QString& category) const;

private:
    ComponentCatalog();
    void addSpec(const ComponentSpec& spec);

    QHash<QString, ComponentSpec> m_specs;
    QHash<QString, QStringList> m_categories;
    ComponentSpec m_fallback;
};
