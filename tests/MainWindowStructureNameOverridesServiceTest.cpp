#include "../src/app/MainWindowStructureNameOverridesService.h"

#include <QHash>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runParseTest()
{
    const QString json = QStringLiteral("{\"a\":\" Alpha \",\"b\":\"\",\"c\":\"Charlie\",\"d\":123}");
    const QHash<QString, QString> overrides = MainWindowStructureNameOverridesService::parse(json);

    if (!expect(overrides.size() == 2,
                "Parse should keep only non-empty trimmed string values.")) {
        return 1;
    }
    if (!expect(overrides.value(QStringLiteral("a")) == QStringLiteral("Alpha"),
                "Parse should trim override values.")) {
        return 1;
    }
    if (!expect(overrides.value(QStringLiteral("c")) == QStringLiteral("Charlie"),
                "Parse should keep valid string override values.")) {
        return 1;
    }

    const QHash<QString, QString> emptyFromInvalid =
        MainWindowStructureNameOverridesService::parse(QStringLiteral("not-json"));
    if (!expect(emptyFromInvalid.isEmpty(),
                "Parse should return empty overrides for invalid JSON.")) {
        return 1;
    }

    return 0;
}

int runSerializeTest()
{
    QHash<QString, QString> overrides;
    overrides.insert(QStringLiteral("a"), QStringLiteral("Alpha"));
    overrides.insert(QStringLiteral("c"), QStringLiteral("Charlie"));

    const QString json = MainWindowStructureNameOverridesService::serialize(overrides);
    const QHash<QString, QString> roundtrip = MainWindowStructureNameOverridesService::parse(json);
    if (!expect(roundtrip == overrides,
                "Serialize output should round-trip through parse without semantic loss.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runParseTest() != 0) {
        return 1;
    }
    if (runSerializeTest() != 0) {
        return 1;
    }

    return 0;
}
