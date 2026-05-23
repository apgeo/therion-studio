#include "../src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.h"

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

int runReferencedLineDeleteRejectedTest()
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
    if (!expect(!plan.resolved, "Deleting an area-referenced line should be rejected.")) {
        return 1;
    }
    return expect(!plan.errorMessage.isEmpty(),
                  "Rejected line deletion should explain why the object was not removed.")
        ? 0
        : 1;
}

int runAreaDeleteRemovesPrivateBorderLineTest()
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
        "endscrap\n");

    const MapEditorObjectDeletePlan plan = planMapEditorObjectDelete(text, 7);
    if (!expect(plan.resolved && plan.changed,
                "Deleting an area with a private border line should resolve and change the source.")) {
        return 1;
    }
    if (!expect(plan.updatedText == expected,
                "Deleting an area should remove its area block and private referenced border line block.")) {
        return 1;
    }
    if (!expect(plan.focusLineAfterDelete == 3,
                "Deleting an area should focus the first removed line when its private border line was above it.")) {
        return 1;
    }
    return expect(plan.removedLineNumbers.size() == 7,
                  "Area delete should report all removed source lines for hidden-row cleanup.")
        ? 0
        : 1;
}

int runAreaDeleteKeepsSharedBorderLineTest()
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
    if (!expect(plan.resolved && plan.changed,
                "Deleting one of two areas sharing a border line should resolve and change the source.")) {
        return 1;
    }
    return expect(plan.updatedText == expected,
                  "Deleting one area should preserve a border line still referenced by another area.")
        ? 0
        : 1;
}

int runStandaloneLineDeleteTest()
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
    if (!expect(plan.resolved && plan.changed,
                "Deleting a standalone line should resolve and change the source.")) {
        return 1;
    }
    return expect(plan.updatedText == expected,
                  "Deleting a standalone line should remove the full line/endline block.")
        ? 0
        : 1;
}

int runCrLfPreservationTest()
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
    if (!expect(plan.resolved && plan.changed,
                "Deleting a CRLF line block should resolve and change the source.")) {
        return 1;
    }
    return expect(plan.updatedText == expected,
                  "Object deletion should preserve CRLF line endings in the remaining text.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runReferencedLineDeleteRejectedTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaDeleteRemovesPrivateBorderLineTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaDeleteKeepsSharedBorderLineTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runStandaloneLineDeleteTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runCrLfPreservationTest(); rc != 0) {
        return rc;
    }
    return 0;
}
