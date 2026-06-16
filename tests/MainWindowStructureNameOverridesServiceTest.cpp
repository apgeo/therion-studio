#include "../src/app/MainWindowStructureNameOverridesService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowStructureNameOverridesServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesOverrides();
    void serializesOverrides();
};

void MainWindowStructureNameOverridesServiceTest::parsesOverrides()
{
    const QString json = QStringLiteral("{\"a\":\" Alpha \",\"b\":\"\",\"c\":\"Charlie\",\"d\":123}");
    const QHash<QString, QString> overrides = MainWindowStructureNameOverridesService::parse(json);

    QCOMPARE(overrides.size(), 2);
    QCOMPARE(overrides.value(QStringLiteral("a")), QStringLiteral("Alpha"));
    QCOMPARE(overrides.value(QStringLiteral("c")), QStringLiteral("Charlie"));

    const QHash<QString, QString> emptyFromInvalid =
        MainWindowStructureNameOverridesService::parse(QStringLiteral("not-json"));
    QVERIFY(emptyFromInvalid.isEmpty());
}

void MainWindowStructureNameOverridesServiceTest::serializesOverrides()
{
    QHash<QString, QString> overrides;
    overrides.insert(QStringLiteral("a"), QStringLiteral("Alpha"));
    overrides.insert(QStringLiteral("c"), QStringLiteral("Charlie"));

    const QString json = MainWindowStructureNameOverridesService::serialize(overrides);
    const QHash<QString, QString> roundtrip = MainWindowStructureNameOverridesService::parse(json);
    QCOMPARE(roundtrip, overrides);
}
}

int runMainWindowStructureNameOverridesServiceTest(int argc, char **argv)
{
    MainWindowStructureNameOverridesServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowStructureNameOverridesServiceTest.moc"
