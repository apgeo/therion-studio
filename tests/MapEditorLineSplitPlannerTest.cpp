#include "../src/app/text_editor/map_editor/MapEditorLineSplitPlanner.h"

#include <QString>
#include <QStringList>

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

int runSplitReferencedOpenLineUpdatesAreaReferencesTest()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1\n"
        "  0 0\n"
        "  1 0\n"
        "  2 0\n"
        "endline\n"
        "area water\n"
        "  id area-1\n"
        "  line-1\n"
        "  line-1 # keep comment\n"
        "endarea\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line border -id line-1\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "line border -id line-1-split-1\n"
        "  1 0\n"
        "  2 0\n"
        "endline\n"
        "area water\n"
        "  id area-1\n"
        "  line-1 line-1-split-1\n"
        "  line-1 line-1-split-1 # keep comment\n"
        "endarea\n"
        "endscrap\n");

    const MapEditorLineSplitPlan plan = MapEditorLineSplitPlanner::planSplit(text,
                                                                              3,
                                                                              QStringList{QStringLiteral("0 0"), QStringLiteral("1 0")},
                                                                              QStringList{QStringLiteral("1 0"), QStringLiteral("2 0")});
    if (!expect(plan.resolved && plan.changed, "Split plan for referenced open line should resolve and change text.")) {
        return 1;
    }
    if (!expect(plan.areaReferencesUpdated, "Split plan should rewrite area references for referenced line IDs.")) {
        return 1;
    }
    if (!expect(plan.originalLineId == QStringLiteral("line-1"), "Split plan should expose original line ID.")) {
        return 1;
    }
    if (!expect(plan.splitLineId == QStringLiteral("line-1-split-1"), "Split plan should create deterministic split line ID.")) {
        return 1;
    }
    return expect(plan.updatedText == expected,
                  "Split plan should split line block and update all area-border references while preserving comments.")
        ? 0
        : 1;
}

int runSplitUnreferencedLineKeepsAreasUntouchedTest()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  1 0\n"
        "  2 0\n"
        "endline\n"
        "area water\n"
        "  wall-2\n"
        "endarea\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "line wall -id wall-1-split-1\n"
        "  1 0\n"
        "  2 0\n"
        "endline\n"
        "area water\n"
        "  wall-2\n"
        "endarea\n"
        "endscrap\n");

    const MapEditorLineSplitPlan plan = MapEditorLineSplitPlanner::planSplit(text,
                                                                              3,
                                                                              QStringList{QStringLiteral("0 0"), QStringLiteral("1 0")},
                                                                              QStringList{QStringLiteral("1 0"), QStringLiteral("2 0")});
    if (!expect(plan.resolved && plan.changed, "Split plan for unreferenced line should resolve and change text.")) {
        return 1;
    }
    if (!expect(!plan.areaReferencesUpdated, "Split plan should not report area reference rewrites for unreferenced IDs.")) {
        return 1;
    }
    return expect(plan.updatedText == expected,
                  "Split plan for unreferenced line should keep unrelated area rows untouched.")
        ? 0
        : 1;
}

int runSplitLineWithoutIdTest()
{
    const QString text = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 0\n"
        "  2 0\n"
        "endline\n"
        "endscrap\n");
    const QString expected = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 0\n"
        "endline\n"
        "line wall\n"
        "  1 0\n"
        "  2 0\n"
        "endline\n"
        "endscrap\n");

    const MapEditorLineSplitPlan plan = MapEditorLineSplitPlanner::planSplit(text,
                                                                              3,
                                                                              QStringList{QStringLiteral("0 0"), QStringLiteral("1 0")},
                                                                              QStringList{QStringLiteral("1 0"), QStringLiteral("2 0")});
    if (!expect(plan.resolved && plan.changed, "Split plan for line without ID should resolve and change text.")) {
        return 1;
    }
    if (!expect(plan.originalLineId.isEmpty() && plan.splitLineId.isEmpty(),
                "Split plan should not invent IDs when source line has no -id.")) {
        return 1;
    }
    return expect(plan.updatedText == expected,
                  "Split plan for line without ID should split geometry without adding IDs.")
        ? 0
        : 1;
}

int runSplitCrLfPreservationAndMalformedLineTest()
{
    const QString text = QString::fromLatin1(
        "encoding utf-8\r\n"
        "scrap s1 -projection plan\r\n"
        "line border -id line-1\r\n"
        "  0 0\r\n"
        "  1 0\r\n"
        "  2 0\r\n"
        "endline\r\n"
        "area water\r\n"
        "  line-1\r\n"
        "endarea\r\n"
        "endscrap\r\n");
    const QString expected = QString::fromLatin1(
        "encoding utf-8\r\n"
        "scrap s1 -projection plan\r\n"
        "line border -id line-1\r\n"
        "  0 0\r\n"
        "  1 0\r\n"
        "endline\r\n"
        "line border -id line-1-split-1\r\n"
        "  1 0\r\n"
        "  2 0\r\n"
        "endline\r\n"
        "area water\r\n"
        "  line-1 line-1-split-1\r\n"
        "endarea\r\n"
        "endscrap\r\n");

    const MapEditorLineSplitPlan plan = MapEditorLineSplitPlanner::planSplit(text,
                                                                              3,
                                                                              QStringList{QStringLiteral("0 0"), QStringLiteral("1 0")},
                                                                              QStringList{QStringLiteral("1 0"), QStringLiteral("2 0")});
    if (!expect(plan.resolved && plan.changed, "CRLF split plan should resolve and change text.")) {
        return 1;
    }
    if (!expect(plan.updatedText == expected,
                "Split plan should preserve CRLF line endings while rewriting area references.")) {
        return 1;
    }

    const QString malformedText = QStringLiteral(
        "encoding utf-8\n"
        "scrap s1 -projection plan\n"
        "line wall -id broken\n"
        "  0 0\n"
        "  1 0\n"
        "endscrap\n");
    const MapEditorLineSplitPlan malformedPlan = MapEditorLineSplitPlanner::planSplit(
        malformedText,
        3,
        QStringList{QStringLiteral("0 0"), QStringLiteral("1 0")},
        QStringList{QStringLiteral("1 0"), QStringLiteral("2 0")});
    return expect(!malformedPlan.resolved && !malformedPlan.errorMessage.isEmpty(),
                  "Malformed line block without endline should return a resolved=false plan with an error.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runSplitReferencedOpenLineUpdatesAreaReferencesTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSplitUnreferencedLineKeepsAreasUntouchedTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSplitLineWithoutIdTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSplitCrLfPreservationAndMalformedLineTest(); rc != 0) {
        return rc;
    }
    return 0;
}
