#include "../src/core/TherionDocumentEditor.h"

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

int runRewritePreservesOtherContentTest()
{
    QString contents = QStringLiteral("survey original\r\n# keep this comment\r\nmap old-map\r\n");
    QString errorMessage;

    const auto expectRewriteFailure = [&](QString documentContents, int lineNumber, const QString &category, const QString &name) {
        const QString before = documentContents;
        errorMessage.clear();
        if (TherionDocumentEditor::rewriteStructureEntryName(&documentContents, lineNumber, category, name, &errorMessage)) {
            return false;
        }

        if (documentContents != before) {
            return false;
        }

        return !errorMessage.isEmpty();
    };

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteStructureEntryName(nullptr, 1, QStringLiteral("Maps"), QStringLiteral("should-not-apply"), &errorMessage), "The rewrite helper should reject null document contents.")) {
        return 1;
    }

    if (!expect(!errorMessage.isEmpty(), "The rewrite helper should report an error for null document contents.")) {
        return 1;
    }

    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Surveys"), QStringLiteral("new-survey"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey new-survey\r\n# keep this comment\r\nmap old-map\r\n"), "The rewrite helper changed unrelated content or line endings.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 3, QStringLiteral("Maps"), QStringLiteral("map name with spaces"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey new-survey\r\n# keep this comment\r\nmap \"map name with spaces\"\r\n"), "The rewrite helper did not quote a spaced name correctly.")) {
        return 1;
    }

    contents = QStringLiteral("station 'old station' # keep this comment\npoint station station-01\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Stations"), QStringLiteral("new station\"name"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("station 'new station\"name' # keep this comment\npoint station station-01\n"), "The rewrite helper did not preserve comments or quote escaping.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 2, QStringLiteral("Stations"), QStringLiteral("new-station-02"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("station 'new station\"name' # keep this comment\npoint station new-station-02\n"), "The rewrite helper did not update the point-station entry correctly.")) {
        return 1;
    }

    contents = QStringLiteral(
        "survey survey-old\n"
        "map map-old\n"
        "scrap scrap-old\n"
        "line line-old\n"
        "area area-old\n"
        "point point-old\n"
        "station station-old\n"
        "point station point-station-old\n");

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Surveys"), QStringLiteral("survey-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 2, QStringLiteral("Maps"), QStringLiteral("map-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 3, QStringLiteral("Scraps"), QStringLiteral("scrap-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 4, QStringLiteral("Lines"), QStringLiteral("line-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 5, QStringLiteral("Areas"), QStringLiteral("area-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 6, QStringLiteral("Points"), QStringLiteral("point-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 7, QStringLiteral("Stations"), QStringLiteral("station-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 8, QStringLiteral("Stations"), QStringLiteral("point-station-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral(
                           "survey survey-new\n"
                           "map map-new\n"
                           "scrap scrap-new\n"
                           "line line-new\n"
                           "area area-new\n"
                           "point point-new\n"
                           "station station-new\n"
                           "point station point-station-new\n"),
                "The rewrite helper did not update every writable directive variant correctly.")) {
        return 1;
    }

    QString negativeContents = QStringLiteral("survey original\n# keep this comment\nmap old-map\n");
    if (!expect(expectRewriteFailure(negativeContents, 2, QStringLiteral("Maps"), QStringLiteral("should-not-apply")), "The rewrite helper should reject comment-only lines.")) {
        return 1;
    }

    QString percentCommentContents = QStringLiteral("survey original\n% keep this comment\nmap old-map\n");
    if (!expect(expectRewriteFailure(percentCommentContents, 2, QStringLiteral("Maps"), QStringLiteral("should-not-apply")), "The rewrite helper should reject percent-comment-only lines.")) {
        return 1;
    }

    QString blankLineContents = QStringLiteral("survey original\n\nmap old-map\n");
    if (!expect(expectRewriteFailure(blankLineContents, 2, QStringLiteral("Maps"), QStringLiteral("should-not-apply")), "The rewrite helper should reject blank lines.")) {
        return 1;
    }

    if (!expect(expectRewriteFailure(negativeContents, 9, QStringLiteral("Maps"), QStringLiteral("should-not-apply")), "The rewrite helper should reject out-of-range line numbers.")) {
        return 1;
    }

    if (!expect(expectRewriteFailure(negativeContents, 1, QStringLiteral("Buildings"), QStringLiteral("should-not-apply")), "The rewrite helper should reject unsupported categories.")) {
        return 1;
    }

    if (!expect(expectRewriteFailure(negativeContents, 1, QStringLiteral("Maps"), QStringLiteral("   ")), "The rewrite helper should reject empty names.")) {
        return 1;
    }

    QString malformedContents = QStringLiteral("survey\nmap map-old\n");
    if (!expect(expectRewriteFailure(malformedContents, 1, QStringLiteral("Surveys"), QStringLiteral("should-not-apply")), "The rewrite helper should reject directives without a writable name token.")) {
        return 1;
    }

    contents = QStringLiteral("map map-old\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Maps"), QStringLiteral("path\\with#hash"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("map \"path\\\\with#hash\"\n"), "The rewrite helper did not escape backslashes and hash characters correctly.")) {
        return 1;
    }

    contents = QStringLiteral("\tmap\t\"old map\"\t# tabbed comment\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Maps"), QStringLiteral("new map"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("\tmap\t\"new map\"\t# tabbed comment\n"), "The rewrite helper did not preserve tabs or double-quoted names correctly.")) {
        return 1;
    }

    contents = QStringLiteral("map map-old extra tokens # trailing comment\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Maps"), QStringLiteral("map-new"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("map map-new extra tokens # trailing comment\n"), "The rewrite helper did not preserve trailing token noise.")) {
        return 1;
    }

    contents = QStringLiteral("map \"unterminated map\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Maps"), QStringLiteral("fixed map"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("map \"fixed map\"\n"), "The rewrite helper did not normalize an unterminated quoted name.")) {
        return 1;
    }

    contents = QStringLiteral("\tpoint\tstation\t'old station'\t# station note\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Stations"), QStringLiteral("new station"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("\tpoint\tstation\t'new station'\t# station note\n"), "The rewrite helper did not preserve the station token layout.")) {
        return 1;
    }

    contents = QStringLiteral("\tmap\t\"old map\"\t# double quote note\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Maps"), QStringLiteral("new map \"quote\""), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("\tmap\t\"new map \\\"quote\\\"\"\t# double quote note\n"), "The rewrite helper did not escape double quotes in a double-quoted name.")) {
        return 1;
    }

    contents = QStringLiteral("station 'old station' # single quote note\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 1, QStringLiteral("Stations"), QStringLiteral("new station's name"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("station 'new station\\'s name' # single quote note\n"), contents.toUtf8().constData())) {
        return 1;
    }

    return 0;
}

int runAppendScrapBlockTest()
{
    QString errorMessage;

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::appendScrapBlock(nullptr, QStringLiteral("new-scrap"), nullptr, &errorMessage), "appendScrapBlock should reject null contents.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "appendScrapBlock should provide an error for null contents.")) {
        return 1;
    }

    QString contents;
    int lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendScrapBlock(&contents, QString(), &lineNumber, &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap new-scrap\nendscrap\n"), "appendScrapBlock should create the default block in an empty document.")) {
        return 1;
    }
    if (!expect(lineNumber == 1, "appendScrapBlock should report line 1 for an empty-document insertion.")) {
        return 1;
    }

    contents = QStringLiteral("survey demo\r\nscrap new-scrap\r\nendscrap\r\n");
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendScrapBlock(&contents, QStringLiteral("new-scrap"), &lineNumber, &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("survey demo\r\nscrap new-scrap\r\nendscrap\r\n\r\nscrap new-scrap-2\r\nendscrap\r\n"),
                "appendScrapBlock should preserve CRLF and generate a unique scrap name.")) {
        return 1;
    }
    if (!expect(lineNumber == 5, "appendScrapBlock should report the inserted scrap start line in CRLF content.")) {
        return 1;
    }

    contents = QStringLiteral("survey cave\n");
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendScrapBlock(&contents, QStringLiteral("  My New Scrap 2026  "), &lineNumber, &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("survey cave\n\nscrap my-new-scrap-2026\nendscrap\n"),
                "appendScrapBlock should sanitize preferred names into stable identifiers.")) {
        return 1;
    }
    if (!expect(lineNumber == 3, "appendScrapBlock should report insertion line after a separated block append.")) {
        return 1;
    }

    return 0;
}

int runAppendDraftGeometryTest()
{
    QString errorMessage;

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::appendDraftGeometry(nullptr, QStringLiteral("point"), {QPointF(10.0, 20.0)}, nullptr, &errorMessage),
                "appendDraftGeometry should reject null contents.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "appendDraftGeometry should provide an error for null contents.")) {
        return 1;
    }

    QString contents = QStringLiteral("scrap main\nendscrap\n");
    int lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendDraftGeometry(&contents, QStringLiteral("point"), {QPointF(123.4, 567.8)}, &lineNumber, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap main\n  point 123.4 567.8 station -name draft-point\nendscrap\n"),
                "appendDraftGeometry should insert point geometry before endscrap.")) {
        return 1;
    }
    if (!expect(lineNumber == 2, "appendDraftGeometry should report the inserted point line number.")) {
        return 1;
    }

    contents = QStringLiteral("survey demo\r\n");
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendDraftGeometry(&contents,
                                                           QStringLiteral("line"),
                                                           {QPointF(10.0, 20.0), QPointF(30.0, 40.0)},
                                                           &lineNumber,
                                                           &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("survey demo\r\n\r\nscrap map-draft\r\n  line wall\r\n    10.0 20.0 30.0 40.0\r\n  endline\r\nendscrap\r\n"),
                "appendDraftGeometry should create fallback scrap context and preserve CRLF.")) {
        return 1;
    }
    if (!expect(lineNumber == 4, "appendDraftGeometry should report line number for inserted line geometry.")) {
        return 1;
    }

    contents = QStringLiteral("scrap a\nendscrap\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::appendDraftGeometry(&contents, QStringLiteral("area"), {QPointF(1.0, 2.0), QPointF(3.0, 4.0)}, nullptr, &errorMessage),
                "appendDraftGeometry should reject area geometry with too few vertices.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "appendDraftGeometry should report a validation error for insufficient vertices.")) {
        return 1;
    }

    return 0;
}

int runRewritePointCoordinatesTest()
{
    QString errorMessage;

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewritePointCoordinates(nullptr, 1, QPointF(1.0, 2.0), &errorMessage),
                "rewritePointCoordinates should reject null contents.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewritePointCoordinates should report null-content error.")) {
        return 1;
    }

    QString contents = QStringLiteral("line wall\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(100.1, 200.2), &errorMessage),
                "rewritePointCoordinates should reject non-point directives.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewritePointCoordinates should report a non-point directive error.")) {
        return 1;
    }

    contents = QStringLiteral("point station 10 20 station -name a1 # keep\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(345.6, 789.1), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point station 345.6 789.1 station -name a1 # keep\n"),
                "rewritePointCoordinates should rewrite first numeric pair on point line and preserve trailing tokens/comment.")) {
        return 1;
    }

    contents = QStringLiteral("station st-a 1 2\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(-10.0, 55.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("station st-a -10.0 55.5\n"),
                "rewritePointCoordinates should rewrite station directive coordinates.")) {
        return 1;
    }

    contents = QStringLiteral("point station \"10\" \"20\" 30 40 station -name a2\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(1.5, -2.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point station \"10\" \"20\" 1.5 -2.5 station -name a2\n"),
                "rewritePointCoordinates should ignore quoted numeric tokens and rewrite the first unquoted coordinate pair.")) {
        return 1;
    }

    contents = QStringLiteral("point station 1e2 -2.5E-1 station -name a3\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(12.3, -45.6), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point station 12.3 -45.6 station -name a3\n"),
                "rewritePointCoordinates should treat scientific-notation tokens as writable numeric coordinates.")) {
        return 1;
    }

    return 0;
}

int runRewriteLineAreaVertexTest()
{
    QString errorMessage;

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(nullptr, 1, QStringLiteral("line"), 0, QPointF(1.0, 2.0), &errorMessage),
                "rewriteLineAreaVertex should reject null contents.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineAreaVertex should report null-content error.")) {
        return 1;
    }

    QString contents = QStringLiteral("point station 1 2 station -name a1\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(100.0, 200.0), &errorMessage),
                "rewriteLineAreaVertex should reject non-line and non-area directives.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineAreaVertex should report non-writable-line error.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  10 20 30 40 # keep\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(-5.5, 77.7), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  10 20 -5.5 77.7 # keep\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should rewrite the selected line vertex and preserve trailing comments.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  1e2 -2E1 3.5e+1 4.0\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(55.0, -66.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  1e2 -2E1 55.0 -66.0\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should treat scientific-notation tokens as writable numeric vertices.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  10 20 30 40 -subtype 100 200\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(70.0, 80.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  10 20 70.0 80.0 -subtype 100 200\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should stop vertex selection at trailing metadata tokens on the same line.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -id line-1 10 20 -subtype 100 200\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(-3.0, 9.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -id line-1 -3.0 9.0 -subtype 100 200\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should allow metadata/options before coordinates and still ignore trailing metadata payload.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -id line-1 -subtype temp 100 200 10 20\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(1.0, 2.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -id line-1 -subtype temp 100 200 1.0 2.0\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should skip numeric option payloads before geometry coordinates on the block start line.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  10 20 30 40\n"
                              "  -subtype temporary 100 200\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 2, QPointF(1.0, 2.0), &errorMessage),
                "rewriteLineAreaVertex should ignore option-led continuation lines even when they contain numeric payload tokens.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(),
                "rewriteLineAreaVertex should report out-of-range when only option-led continuation numeric payload exists after real coordinates.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(77.0, -88.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  10 20 77.0 -88.0\n"
                                           "  -subtype temporary 100 200\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should keep option-led continuation payload unchanged while rewriting real geometry vertices.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  10 20 30 40\n"
                              "  smooth off 50 60\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 2, QPointF(99.0, 88.0), &errorMessage),
                "rewriteLineAreaVertex should ignore non-coordinate continuation lines even when they contain numeric payload tokens.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(),
                "rewriteLineAreaVertex should report out-of-range when continuation metadata lines are excluded from writable geometry indexing.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(77.0, 66.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  10 20 77.0 66.0\n"
                                           "  smooth off 50 60\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should rewrite only real geometry vertices and keep non-coordinate continuation metadata unchanged.")) {
        return 1;
    }

    contents = QStringLiteral("area water\n"
                              "  1 2 3 4\n"
                              "  5 6 # keep area note\n"
                              "endarea\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("area"), 2, QPointF(42.0, -9.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("area water\n"
                                           "  1 2 3 4\n"
                                           "  42.0 -9.5 # keep area note\n"
                                           "endarea\n"),
                "rewriteLineAreaVertex should support multi-line area vertex rewrites.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n  10 20\nendline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 2, QPointF(1.0, 2.0), &errorMessage),
                "rewriteLineAreaVertex should reject out-of-range vertex indices.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineAreaVertex should report out-of-range vertex errors.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  \"10\" \"20\" 30 40\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(8.0, 9.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  \"10\" \"20\" 8.0 9.0\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should ignore quoted numeric tokens when selecting writable vertices.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\r\n"
                              "  10 20\r\n"
                              "  30 40\r\n"
                              "endline\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(7.0, 8.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\r\n"
                                           "  10 20\r\n"
                                           "  7.0 8.0\r\n"
                                           "endline\r\n"),
                "rewriteLineAreaVertex should preserve CRLF line endings.")) {
        return 1;
    }

    contents = QStringLiteral("line wall 10 20 30 40 -id line-1\n"
                              "  50 60\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 2, QPointF(-11.0, 12.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall 10 20 30 40 -id line-1\n"
                                           "  -11.0 12.5\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should support rewriting vertices that continue on following lines.")) {
        return 1;
    }

    contents = QStringLiteral("area water\n"
                              "  1 2 3\n"
                              "endarea\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("area"), 1, QPointF(9.0, 9.0), &errorMessage),
                "rewriteLineAreaVertex should reject incomplete odd coordinate tuples.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineAreaVertex should report an error when requested vertex pair is not available.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  1 2\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(5.0, 6.0), &errorMessage),
                "rewriteLineAreaVertex should reject line blocks missing endline.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineAreaVertex should report missing block terminator errors.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    const int rewriteResult = runRewritePreservesOtherContentTest();
    if (rewriteResult != 0) {
        return rewriteResult;
    }

    const int appendScrapResult = runAppendScrapBlockTest();
    if (appendScrapResult != 0) {
        return appendScrapResult;
    }

    const int appendDraftResult = runAppendDraftGeometryTest();
    if (appendDraftResult != 0) {
        return appendDraftResult;
    }

    const int rewritePointResult = runRewritePointCoordinatesTest();
    if (rewritePointResult != 0) {
        return rewritePointResult;
    }

    return runRewriteLineAreaVertexTest();
}
