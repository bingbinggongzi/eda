#pragma once

#include "GraphDocument.h"

#include <QString>

class GraphSerializer {
public:
    static bool saveToFile(const GraphDocument& document, const QString& filePath, QString* errorMessage = nullptr);
    static bool loadFromFile(GraphDocument* document, const QString& filePath, QString* errorMessage = nullptr);
};
