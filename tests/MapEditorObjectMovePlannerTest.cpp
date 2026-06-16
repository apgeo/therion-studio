#include "../src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorObjectMovePlannerTest : public QObject
{
    Q_OBJECT

private slots:
    void movesPointAfterLineBlock();
    void movesLineBlockBeforePoint();
    void movesAreaBetweenScraps();
    void movesPointIntoScrap();
    void preservesCrLfLineEndings();
    void rejectsNoOpAndInvalidTargets();
    void treatsAdjacentBoundaryMovesAsNoOps();
};

void MapEditorObjectMovePlannerTest::movesPointAfterLineBlock()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "point 1 2 station -name P1\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 1 2 station -name P1\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "endscrap\n");

    const MapEditorObjectMovePlan plan = MapEditorObjectMovePlanner::planMove(text,
                                                                              3,
                                                                              4,
                                                                              MapEditorObjectMovePosition::AfterTarget);
    QVERIFY2(plan.resolved, "Moving a point after a line block should resolve.");
    QVERIFY2(plan.changed, "Moving a point after a later line block should report a changed plan.");
    QCOMPARE(plan.sourceStartLine, 3);
    QCOMPARE(plan.sourceEndLine, 3);
    QCOMPARE(plan.insertBeforeLineOriginal, 8);
    QCOMPARE(plan.insertBeforeLineAfterRemoval, 7);
    QCOMPARE(plan.movedText, expected);
}

void MapEditorObjectMovePlannerTest::movesLineBlockBeforePoint()
{
    QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "point 1 2 station -name P1\n"
        "line wall\n"
        "  # keep this comment with the line block\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall\n"
        "  # keep this comment with the line block\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 1 2 station -name P1\n"
        "endscrap\n");

    QString errorMessage;
    QVERIFY2(MapEditorObjectMovePlanner::applyMove(&text,
                                                   4,
                                                   3,
                                                   MapEditorObjectMovePosition::BeforeTarget,
                                                   &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(text, expected);
}

void MapEditorObjectMovePlannerTest::movesAreaBetweenScraps()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "point 5 6 label -text target\n"
        "endscrap\n");

    const MapEditorObjectMovePlan plan = MapEditorObjectMovePlanner::planMove(text,
                                                                              3,
                                                                              8,
                                                                              MapEditorObjectMovePosition::BeforeTarget);
    QVERIFY2(plan.resolved && plan.changed,
             "Moving an area before an object in another scrap should resolve and change text.");
    QCOMPARE(plan.movedText, expected);
}

void MapEditorObjectMovePlannerTest::movesPointIntoScrap()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "point 1 2 station -name P1\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "point 1 2 station -name P1\n"
        "endscrap\n");

    const MapEditorObjectMovePlan plan = MapEditorObjectMovePlanner::planMove(text,
                                                                              3,
                                                                              5,
                                                                              MapEditorObjectMovePosition::IntoTargetScrap);
    QVERIFY2(plan.resolved && plan.changed,
             "Moving a point into a target scrap should resolve and change text.");
    QCOMPARE(plan.insertBeforeLineOriginal, 7);
    QCOMPARE(plan.insertBeforeLineAfterRemoval, 6);
    QCOMPARE(plan.movedText, expected);
}

void MapEditorObjectMovePlannerTest::preservesCrLfLineEndings()
{
    const QString text = QString::fromLatin1(
        "encoding utf-8\r\n"
        "scrap s1 -projection plan\r\n"
        "point 1 2 station -name P1\r\n"
        "point 3 4 station -name P2\r\n"
        "endscrap\r\n");
    const QString expected = QString::fromLatin1(
        "encoding utf-8\r\n"
        "scrap s1 -projection plan\r\n"
        "point 3 4 station -name P2\r\n"
        "point 1 2 station -name P1\r\n"
        "endscrap\r\n");

    const MapEditorObjectMovePlan plan = MapEditorObjectMovePlanner::planMove(text,
                                                                              3,
                                                                              4,
                                                                              MapEditorObjectMovePosition::AfterTarget);
    QVERIFY2(plan.resolved && plan.changed,
             "Moving CRLF point rows should resolve and change text.");
    QCOMPARE(plan.movedText, expected);
}

void MapEditorObjectMovePlannerTest::rejectsNoOpAndInvalidTargets()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "endscrap\n");

    const MapEditorObjectMovePlan noOpPlan = MapEditorObjectMovePlanner::planMove(
        text,
        3,
        3,
        MapEditorObjectMovePosition::AfterTarget);
    QVERIFY2(noOpPlan.resolved && !noOpPlan.changed,
             "Moving an object after itself should resolve as a no-op.");
    QCOMPARE(noOpPlan.movedText, text);

    const MapEditorObjectMovePlan invalidPlan = MapEditorObjectMovePlanner::planMove(
        text,
        2,
        3,
        MapEditorObjectMovePosition::BeforeTarget);
    QVERIFY2(!invalidPlan.resolved && !invalidPlan.errorMessage.isEmpty(),
             "Moving a non-object source line should fail with an error message.");
}

void MapEditorObjectMovePlannerTest::treatsAdjacentBoundaryMovesAsNoOps()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "endscrap\n");

    const MapEditorObjectMovePlan beforeNextPlan = MapEditorObjectMovePlanner::planMove(
        text,
        3,
        4,
        MapEditorObjectMovePosition::BeforeTarget);
    QVERIFY2(beforeNextPlan.resolved && !beforeNextPlan.changed,
             "Moving an object before its immediate next sibling should resolve as a no-op.");
    QCOMPARE(beforeNextPlan.movedText, text);

    const MapEditorObjectMovePlan afterPreviousPlan = MapEditorObjectMovePlanner::planMove(
        text,
        5,
        4,
        MapEditorObjectMovePosition::AfterTarget);
    QVERIFY2(afterPreviousPlan.resolved && !afterPreviousPlan.changed,
             "Moving an object after its immediate previous sibling should resolve as a no-op.");
    QCOMPARE(afterPreviousPlan.movedText, text);
}
}

int runMapEditorObjectMovePlannerTest(int argc, char **argv)
{
    MapEditorObjectMovePlannerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorObjectMovePlannerTest.moc"
