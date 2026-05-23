#include "../src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h"

#include <QString>

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

int runMovePointAfterLineBlockTest()
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
    if (!expect(plan.resolved, "Moving a point after a line block should resolve.")) {
        return 1;
    }
    if (!expect(plan.changed, "Moving a point after a later line block should report a changed plan.")) {
        return 1;
    }
    if (!expect(plan.sourceStartLine == 3 && plan.sourceEndLine == 3,
                "Point move source span should contain only the point line.")) {
        return 1;
    }
    if (!expect(plan.insertBeforeLineOriginal == 8 && plan.insertBeforeLineAfterRemoval == 7,
                "Point move insertion lines should be tracked before and after source removal.")) {
        return 1;
    }
    return expect(plan.movedText == expected,
                  "Moving a point after a line block should preserve source text and place the point after endline.")
        ? 0
        : 1;
}

int runMoveLineBlockBeforePointTest()
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
    if (!expect(MapEditorObjectMovePlanner::applyMove(&text,
                                                       4,
                                                       3,
                                                       MapEditorObjectMovePosition::BeforeTarget,
                                                       &errorMessage),
                "Moving a line block before a point should apply.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    return expect(text == expected,
                  "Moving a line block before a point should preserve all line block rows.")
        ? 0
        : 1;
}

int runMoveAreaBetweenScrapsTest()
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
    if (!expect(plan.resolved && plan.changed,
                "Moving an area before an object in another scrap should resolve and change text.")) {
        return 1;
    }
    return expect(plan.movedText == expected,
                  "Moving an area before a target in another scrap should place the whole area block inside the target scrap.")
        ? 0
        : 1;
}

int runMovePointIntoScrapTest()
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
    if (!expect(plan.resolved && plan.changed,
                "Moving a point into a target scrap should resolve and change text.")) {
        return 1;
    }
    if (!expect(plan.insertBeforeLineOriginal == 7 && plan.insertBeforeLineAfterRemoval == 6,
                "Moving into a target scrap should insert before endscrap after source removal.")) {
        return 1;
    }
    return expect(plan.movedText == expected,
                  "Moving a point into a target scrap should append the point before the target endscrap.")
        ? 0
        : 1;
}

int runCrLfPreservationTest()
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
    if (!expect(plan.resolved && plan.changed,
                "Moving CRLF point rows should resolve and change text.")) {
        return 1;
    }
    return expect(plan.movedText == expected,
                  "Moving point rows should preserve CRLF line endings.")
        ? 0
        : 1;
}

int runNoOpAndInvalidTargetTest()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "endscrap\n");

    const MapEditorObjectMovePlan noOpPlan = MapEditorObjectMovePlanner::planMove(text,
                                                                                  3,
                                                                                  3,
                                                                                  MapEditorObjectMovePosition::AfterTarget);
    if (!expect(noOpPlan.resolved && !noOpPlan.changed,
                "Moving an object after itself should resolve as a no-op.")) {
        return 1;
    }
    if (!expect(noOpPlan.movedText == text,
                "No-op move should preserve the original text exactly.")) {
        return 1;
    }

    const MapEditorObjectMovePlan invalidPlan = MapEditorObjectMovePlanner::planMove(text,
                                                                                    2,
                                                                                    3,
                                                                                    MapEditorObjectMovePosition::BeforeTarget);
    return expect(!invalidPlan.resolved && !invalidPlan.errorMessage.isEmpty(),
                  "Moving a non-object source line should fail with an error message.")
        ? 0
        : 1;
}

int runAdjacentBoundaryNoOpTest()
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
    if (!expect(beforeNextPlan.resolved && !beforeNextPlan.changed,
                "Moving an object before its immediate next sibling should resolve as a no-op.")) {
        return 1;
    }
    if (!expect(beforeNextPlan.movedText == text,
                "Immediate-next no-op move should preserve the original text exactly.")) {
        return 1;
    }

    const MapEditorObjectMovePlan afterPreviousPlan = MapEditorObjectMovePlanner::planMove(
        text,
        5,
        4,
        MapEditorObjectMovePosition::AfterTarget);
    if (!expect(afterPreviousPlan.resolved && !afterPreviousPlan.changed,
                "Moving an object after its immediate previous sibling should resolve as a no-op.")) {
        return 1;
    }
    return expect(afterPreviousPlan.movedText == text,
                  "Immediate-previous no-op move should preserve the original text exactly.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runMovePointAfterLineBlockTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMoveLineBlockBeforePointTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMoveAreaBetweenScrapsTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMovePointIntoScrapTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runCrLfPreservationTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runNoOpAndInvalidTargetTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAdjacentBoundaryNoOpTest(); rc != 0) {
        return rc;
    }
    return 0;
}
