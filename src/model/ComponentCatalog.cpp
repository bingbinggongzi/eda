#include "ComponentCatalog.h"

#include <algorithm>

const ComponentCatalog& ComponentCatalog::instance() {
    static ComponentCatalog catalog;
    return catalog;
}

const ComponentSpec* ComponentCatalog::find(const QString& typeName) const {
    const auto it = m_specs.find(typeName);
    if (it == m_specs.end()) {
        return nullptr;
    }
    return &it.value();
}

const ComponentSpec& ComponentCatalog::fallback() const {
    return m_fallback;
}

QStringList ComponentCatalog::categories() const {
    QStringList keys = m_categories.keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

QStringList ComponentCatalog::typesInCategory(const QString& category) const {
    return m_categories.value(category);
}

ComponentCatalog::ComponentCatalog() {
    m_fallback = ComponentSpec{QStringLiteral("tm_Node"),
                               QStringLiteral("tm_Node"),
                               QStringLiteral("General"),
                               QSizeF(120.0, 72.0),
                               1,
                               1,
                               {PropertyData{QStringLiteral("enabled"), QStringLiteral("bool"), QStringLiteral("true")},
                                PropertyData{QStringLiteral("gain"), QStringLiteral("double"), QStringLiteral("1.0")}}};

    addSpec(ComponentSpec{QStringLiteral("Voter"),
                          QStringLiteral("Voter"),
                          QStringLiteral("Control"),
                          QSizeF(120.0, 80.0),
                          3,
                          1,
                          {PropertyData{QStringLiteral("vote_required"), QStringLiteral("int"), QStringLiteral("2")},
                           PropertyData{QStringLiteral("strategy"), QStringLiteral("string"), QStringLiteral("majority")}}});
    addSpec(ComponentSpec{QStringLiteral("SFT"),
                          QStringLiteral("SFT"),
                          QStringLiteral("Control"),
                          QSizeF(120.0, 72.0),
                          2,
                          1,
                          {PropertyData{QStringLiteral("gain"), QStringLiteral("double"), QStringLiteral("1.0")},
                           PropertyData{QStringLiteral("enabled"), QStringLiteral("bool"), QStringLiteral("true")}}});
    addSpec(ComponentSpec{QStringLiteral("Sum"),
                          QStringLiteral("Sum"),
                          QStringLiteral("Control"),
                          QSizeF(120.0, 72.0),
                          2,
                          1,
                          {PropertyData{QStringLiteral("bias"), QStringLiteral("double"), QStringLiteral("0.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Node"),
                          QStringLiteral("tm_Node"),
                          QStringLiteral("Actuator"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("enabled"), QStringLiteral("bool"), QStringLiteral("true")},
                           PropertyData{QStringLiteral("pressure"), QStringLiteral("double"), QStringLiteral("1.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_CheckVlv"),
                          QStringLiteral("tm_CheckVlv"),
                          QStringLiteral("Actuator"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("cv"), QStringLiteral("double"), QStringLiteral("1.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_AirWater"),
                          QStringLiteral("tm_AirWater"),
                          QStringLiteral("Actuator"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("ratio"), QStringLiteral("double"), QStringLiteral("0.5")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Bound"),
                          QStringLiteral("tm_Bound"),
                          QStringLiteral("Actuator"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("limit"), QStringLiteral("double"), QStringLiteral("100.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_TubeHte"),
                          QStringLiteral("tm_TubeHte"),
                          QStringLiteral("Sensor"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("temperature"), QStringLiteral("double"), QStringLiteral("25.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_HeatExChS"),
                          QStringLiteral("tm_HeatExChS"),
                          QStringLiteral("Sensor"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("efficiency"), QStringLiteral("double"), QStringLiteral("0.85")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_HeatExCh"),
                          QStringLiteral("tm_HeatExCh"),
                          QStringLiteral("Sensor"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("efficiency"), QStringLiteral("double"), QStringLiteral("0.80")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Vessel"),
                          QStringLiteral("tm_Vessel"),
                          QStringLiteral("Sensor"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("level"), QStringLiteral("double"), QStringLiteral("0.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Bearing"),
                          QStringLiteral("tm_Bearing"),
                          QStringLiteral("Control"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("friction"), QStringLiteral("double"), QStringLiteral("0.2")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Load"),
                          QStringLiteral("tm_Load"),
                          QStringLiteral("Electric"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("power"), QStringLiteral("double"), QStringLiteral("100.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_HeatSide"),
                          QStringLiteral("tm_HeatSide"),
                          QStringLiteral("Electric"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("heat_flux"), QStringLiteral("double"), QStringLiteral("1.0")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Valve"),
                          QStringLiteral("tm_Valve"),
                          QStringLiteral("Electric"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("open"), QStringLiteral("bool"), QStringLiteral("false")}}});
    addSpec(ComponentSpec{QStringLiteral("tm_Pump"),
                          QStringLiteral("tm_Pump"),
                          QStringLiteral("Electric"),
                          QSizeF(120.0, 72.0),
                          1,
                          1,
                          {PropertyData{QStringLiteral("speed"), QStringLiteral("double"), QStringLiteral("1200.0")}}});
}

void ComponentCatalog::addSpec(const ComponentSpec& spec) {
    m_specs.insert(spec.typeName, spec);
    m_categories[spec.category].append(spec.typeName);
}
