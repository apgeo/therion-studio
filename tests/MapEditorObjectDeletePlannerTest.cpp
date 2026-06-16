#include "../src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorObjectDeletePlannerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsReferencedLineDelete();
    void keepsReferencedBorderLineWhenDeletingArea();
    void keepsSharedBorderLineWhenDeletingArea();
    void deletesStandaloneLineBlock();
    void preservesCrLfLineEndings();
};

void MapEditorObjectDeletePlannerTest::rejectsReferencedLineDelete()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");

    const MapEditorObjectDeletePlan plan = planMapEditorObjectDelete(text, 3);
    QVERIFY2(!plan.resolved, "Deleting an area-referenced line should be rejected.");
    QVERIFY2(!plan.errorMessage.isEmpty(),
             "Rejected line deletion should explain why the object was not removed.");
}

void MapEditorObjectDeletePlannerTest::keepsReferencedBorderLineWhenDeletingArea()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "endscrap\n");

    const MapEditorObjectDeletePlan plan = planMapEditorObjectDelete(text, 7);
    QVERIFY2(plan.resolved && plan.changed,
             "Deleting an area with a referenced border line should resolve and change the source.");
    QCOMPARE(plan.updatedText, expected);
    QCOMPARE(plan.focusLineAfterDelete, 7);
    QCOMPARE(plan.removedLineNumbers.size(), 3);
}

void MapEditorObjectDeletePlannerTest::keepsSharedBorderLineWhenDeletingArea()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "area sand\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "area sand\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");

    const MapEditorObjectDeletePlan plan = planMapEditorObjectDelete(text, 7);
    QVERIFY2(plan.resolved && plan.changed,
             "Deleting one of two areas sharing a border line should resolve and change the source.");
    QCOMPARE(plan.updatedText, expected);
}

void MapEditorObjectDeletePlannerTest::deletesStandaloneLineBlock()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "point 2 3 station -name P1\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "point 2 3 station -name P1\n"
        "endscrap\n");

    const MapEditorObjectDeletePlan plan = planMapEditorObjectDelete(text, 3);
    QVERIFY2(plan.resolved && plan.changed,
             "Deleting a standalone line should resolve and change the source.");
    QCOMPARE(plan.updatedText, expected);
}

void MapEditorObjectDeletePlannerTest::preservesCrLfLineEndings()
{
    const QString text = QString::fromLatin1(
        "encoding utf-8\r\n"
        "scrap s1 -projection plan\r\n"
        "point 2 3 station -name P1\r\n"
        "line wall\r\n"
        "  0 0\r\n"
        "endline\r\n"
        "endscrap\r\n");
    const QString expected = QString::fromLatin1(
        "encoding utf-8\r\n"
        "scrap s1 -projection plan\r\n"
        "point 2 3 station -name P1\r\n"
        "endscrap\r\n");

    const MapEditorObjectDeletePlan plan = planMapEditorObjectDelete(text, 4);
    QVERIFY2(plan.resolved && plan.changed,
             "Deleting a CRLF line block should resolve and change the source.");
    QCOMPARE(plan.updatedText, expected);
}
}

int runMapEditorObjectDeletePlannerTest(int argc, char **argv)
{
    MapEditorObjectDeletePlannerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorObjectDeletePlannerTest.moc"
