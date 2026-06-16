#include "../src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorAreaReferenceResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesAreaBodyReferencesToBorderLines();
    void reportsMultipleAreaReferences();
    void preservesLineNumbersThroughLosslessProjection();
};

void MapEditorAreaReferenceResolverTest::resolvesAreaBodyReferencesToBorderLines()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "endline\n"
        "area water -id a1\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");

    const QSet<int> borderLines = mapEditorBorderLineNumbersForArea(text, 5);
    QCOMPARE(borderLines.size(), 1);
    QVERIFY(borderLines.contains(2));

    const QVector<MapEditorAreaReference> references = mapEditorAreaReferencesForBorderLine(text, 2);
    QCOMPARE(references.size(), 1);
    QCOMPARE(references.first().areaLineNumber, 5);
    QCOMPARE(references.first().areaLabel, QStringLiteral("water (a1)"));
}

void MapEditorAreaReferenceResolverTest::reportsMultipleAreaReferences()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "area sand\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");

    const QVector<MapEditorAreaReference> references = mapEditorAreaReferencesForBorderLine(text, 2);
    QCOMPARE(references.size(), 2);
    QCOMPARE(references.at(0).areaLineNumber, 5);
    QCOMPARE(references.at(1).areaLineNumber, 8);
}

void MapEditorAreaReferenceResolverTest::preservesLineNumbersThroughLosslessProjection()
{
    const QString text = QStringLiteral(
        "# header\r\n"
        "\r\n"
        "scrap s1 -projection plan\r\n"
        "line border -id line-1 -close on\r\n"
        "  0 0\r\n"
        "endline\r\n"
        "  # area comment\r\n"
        "area water -id a1\r\n"
        "  line-1\r\n"
        "endarea\r\n"
        "endscrap\r\n");

    const QSet<int> borderLines = mapEditorBorderLineNumbersForArea(text, 8);
    QCOMPARE(borderLines.size(), 1);
    QVERIFY(borderLines.contains(4));

    const QVector<MapEditorAreaReference> references = mapEditorAreaReferencesForBorderLine(text, 4);
    QCOMPARE(references.size(), 1);
    QCOMPARE(references.first().areaLineNumber, 8);
}
}

int runMapEditorAreaReferenceResolverTest(int argc, char **argv)
{
    MapEditorAreaReferenceResolverTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorAreaReferenceResolverTest.moc"
