#include "../src/core/TherionDocumentEditor.h"

#include <functional>
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

    contents = QStringLiteral("survey old-survey\r\nmap old-map\nscrap old-scrap\rendscrap");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteStructureEntryName(&contents, 2, QStringLiteral("Maps"), QStringLiteral("new-map"), &errorMessage), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey old-survey\r\nmap new-map\nscrap old-scrap\rendscrap"),
                "The rewrite helper should preserve mixed physical line endings when replacing one token.")) {
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
    if (!expect(contents == QStringLiteral("scrap scrap-1\nendscrap\n"), "appendScrapBlock should create the default block in an empty document.")) {
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
    if (!expect(contents == QStringLiteral("survey demo\r\nscrap new-scrap\r\nendscrap\r\n\r\nscrap scrap-1\r\nendscrap\r\n"),
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

    contents.clear();
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendScrapBlock(&contents,
                                                        QStringLiteral("scaled"),
                                                        &lineNumber,
                                                        &errorMessage,
                                                        QStringLiteral("-projection plan -scale [0 0 1600 0 0.0 0.0 40.64 0.0 m]")),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap scaled -projection plan -scale [0 0 1600 0 0.0 0.0 40.64 0.0 m]\nendscrap\n"),
                "appendScrapBlock should preserve caller-provided scrap options for map-editor compatible writes.")) {
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
                                                           {QPointF(10.0, 20.0), QPointF(30.0, 40.0), QPointF(50.0, 60.0), QPointF(70.0, 80.0)},
                                                           &lineNumber,
                                                           &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("survey demo\r\n\r\nscrap scrap-1\r\n  line wall\r\n    10.0 20.0\r\n    30.0 40.0\r\n    50.0 60.0\r\n    70.0 80.0\r\n  endline\r\nendscrap\r\n"),
                "appendDraftGeometry should serialize line drafts as anchor rows and preserve CRLF.")) {
        return 1;
    }
    if (!expect(lineNumber == 4, "appendDraftGeometry should report line number for inserted line geometry.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s\nendscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendDraftLineGeometry(&contents,
                                                               {QStringLiteral("1 2"),
                                                                QStringLiteral("3 4"),
                                                                QStringLiteral("5 6")},
                                                               &lineNumber,
                                                               &errorMessage,
                                                               QStringLiteral("-close on")),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s\n"
                                           "  line wall -close on\n"
                                           "    1 2\n"
                                           "    3 4\n"
                                           "    5 6\n"
                                           "  endline\n"
                                           "endscrap\n"),
                "appendDraftLineGeometry should include line options in the generated line header.")) {
        return 1;
    }
    if (!expect(lineNumber == 2, "appendDraftLineGeometry should report the inserted line header line number.")) {
        return 1;
    }

    contents = QStringLiteral("scrap custom\nendscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions lineObjectOptions;
    lineObjectOptions.type = QStringLiteral("wall");
    lineObjectOptions.subtype = QStringLiteral("blocks");
    lineObjectOptions.identifier = QStringLiteral("custom-line");
    if (!expect(TherionDocumentEditor::appendDraftLineGeometry(&contents,
                                                               {QStringLiteral("10 20"),
                                                                QStringLiteral("30 40")},
                                                               &lineNumber,
                                                               &errorMessage,
                                                               QString(),
                                                               lineObjectOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap custom\n"
                                           "  line wall -subtype blocks -id custom-line\n"
                                           "    10 20\n"
                                           "    30 40\n"
                                           "  endline\n"
                                           "endscrap\n"),
                "appendDraftLineGeometry should apply caller-provided type, subtype, and ID options.")) {
        return 1;
    }

    contents = QStringLiteral("scrap custom\nendscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions pointObjectOptions;
    pointObjectOptions.type = QStringLiteral("label");
    pointObjectOptions.subtype = QStringLiteral("remark");
    pointObjectOptions.identifier = QStringLiteral("p-1");
    pointObjectOptions.text = QStringLiteral("Entrance label");
    pointObjectOptions.textEnabled = true;
    if (!expect(TherionDocumentEditor::appendDraftGeometry(&contents,
                                                           QStringLiteral("point"),
                                                           {QPointF(12.0, 34.0)},
                                                           &lineNumber,
                                                           &errorMessage,
                                                           pointObjectOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap custom\n"
                                           "  point 12.0 34.0 label -subtype remark -id p-1 -text \"Entrance label\"\n"
                                           "endscrap\n"),
                "appendDraftGeometry should apply caller-provided point type, subtype, ID, and text options.")) {
        return 1;
    }

    contents = QStringLiteral("scrap custom\nendscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions pointValueOptions;
    pointValueOptions.type = QStringLiteral("height");
    pointValueOptions.value = QStringLiteral("[40? ft]");
    pointValueOptions.valueEnabled = true;
    if (!expect(TherionDocumentEditor::appendDraftGeometry(&contents,
                                                           QStringLiteral("point"),
                                                           {QPointF(56.0, 78.0)},
                                                           &lineNumber,
                                                           &errorMessage,
                                                           pointValueOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap custom\n"
                                           "  point 56.0 78.0 height -value [40? ft]\n"
                                           "endscrap\n"),
                "appendDraftGeometry should apply caller-provided point value options.")) {
        return 1;
    }

    contents = QStringLiteral("scrap first\n"
                              "endscrap\n"
                              "scrap second\n"
                              "endscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions targetedPointOptions;
    targetedPointOptions.targetScrapIdentifier = QStringLiteral("first");
    if (!expect(TherionDocumentEditor::appendDraftGeometry(&contents,
                                                           QStringLiteral("point"),
                                                           {QPointF(9.0, 10.0)},
                                                           &lineNumber,
                                                           &errorMessage,
                                                           targetedPointOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap first\n"
                                           "  point 9.0 10.0 station -name draft-point\n"
                                           "endscrap\n"
                                           "scrap second\n"
                                           "endscrap\n"),
                "appendDraftGeometry should insert point geometry into the requested target scrap.")) {
        return 1;
    }
    if (!expect(lineNumber == 2, "appendDraftGeometry should report the targeted point line number.")) {
        return 1;
    }

    contents = QStringLiteral("scrap first\n"
                              "endscrap\n"
                              "scrap second\n"
                              "endscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions missingTargetOptions;
    missingTargetOptions.targetScrapIdentifier = QStringLiteral("missing");
    if (!expect(!TherionDocumentEditor::appendDraftGeometry(&contents,
                                                            QStringLiteral("point"),
                                                            {QPointF(9.0, 10.0)},
                                                            &lineNumber,
                                                            &errorMessage,
                                                            missingTargetOptions),
                "appendDraftGeometry should reject an explicit target scrap that no longer exists.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "appendDraftGeometry should report a missing target scrap error.")) {
        return 1;
    }

    contents = QStringLiteral("scrap first\n"
                              "endscrap\n"
                              "scrap second\n"
                              "endscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions targetedLineOptions;
    targetedLineOptions.targetScrapIdentifier = QStringLiteral("second");
    if (!expect(TherionDocumentEditor::appendDraftLineGeometry(&contents,
                                                               {QStringLiteral("1 2"), QStringLiteral("3 4")},
                                                               &lineNumber,
                                                               &errorMessage,
                                                               QString(),
                                                               targetedLineOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap first\n"
                                           "endscrap\n"
                                           "scrap second\n"
                                           "  line wall\n"
                                           "    1 2\n"
                                           "    3 4\n"
                                           "  endline\n"
                                           "endscrap\n"),
                "appendDraftLineGeometry should insert line geometry into the requested target scrap.")) {
        return 1;
    }
    if (!expect(lineNumber == 4, "appendDraftLineGeometry should report the targeted line header line number.")) {
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

    contents = QStringLiteral("scrap a\nendscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendDraftGeometry(&contents,
                                                           QStringLiteral("area"),
                                                           {QPointF(1.0, 2.0), QPointF(3.0, 4.0), QPointF(5.0, 6.0), QPointF(7.0, 8.0)},
                                                           &lineNumber,
                                                           &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap a\n"
                                           "  line border -id line-1 -close on\n"
                                           "    1.0 2.0\n"
                                           "    3.0 4.0\n"
                                           "    5.0 6.0\n"
                                           "    7.0 8.0\n"
                                           "  endline\n"
                                           "  area water\n"
                                           "    line-1\n"
                                           "  endarea\n"
                                           "endscrap\n"),
                "appendDraftGeometry should serialize area drafts as closed border lines referenced from area blocks.")) {
        return 1;
    }
    if (!expect(lineNumber == 8, "appendDraftGeometry should report the inserted area line number.")) {
        return 1;
    }

    contents = QStringLiteral("scrap a\nendscrap\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::appendDraftAreaGeometry(&contents,
                                                                {QStringLiteral("1 2"), QStringLiteral("3 4")},
                                                                nullptr,
                                                                &errorMessage),
                "appendDraftAreaGeometry should reject area geometry with too few coordinate rows.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(),
                "appendDraftAreaGeometry should report a validation error for insufficient coordinate rows.")) {
        return 1;
    }

    contents = QStringLiteral("scrap a\n"
                              "  line wall -id line-1\n"
                              "  endline\n"
                              "endscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::appendDraftAreaGeometry(&contents,
                                                               {QStringLiteral("1 2"),
                                                                QStringLiteral("3 4 5 6"),
                                                                QStringLiteral("7 8")},
                                                               &lineNumber,
                                                               &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap a\n"
                                           "  line wall -id line-1\n"
                                           "  endline\n"
                                           "  line border -id line-2 -close on\n"
                                           "    1 2\n"
                                           "    3 4 5 6\n"
                                           "    7 8\n"
                                           "  endline\n"
                                           "  area water\n"
                                           "    line-2\n"
                                           "  endarea\n"
                                           "endscrap\n"),
                "appendDraftAreaGeometry should preserve coordinate rows and write area references to a unique border line id.")) {
        return 1;
    }
    if (!expect(lineNumber == 9, "appendDraftAreaGeometry should report the inserted area line number.")) {
        return 1;
    }

    contents = QStringLiteral("scrap first\n"
                              "endscrap\n"
                              "scrap second\n"
                              "  line wall -id line-1\n"
                              "  endline\n"
                              "endscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions targetedAreaOptions;
    targetedAreaOptions.targetScrapIdentifier = QStringLiteral("second");
    if (!expect(TherionDocumentEditor::appendDraftAreaGeometry(&contents,
                                                               {QStringLiteral("1 2"),
                                                                QStringLiteral("3 4"),
                                                                QStringLiteral("5 6")},
                                                               &lineNumber,
                                                               &errorMessage,
                                                               targetedAreaOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap first\n"
                                           "endscrap\n"
                                           "scrap second\n"
                                           "  line wall -id line-1\n"
                                           "  endline\n"
                                           "  line border -id line-2 -close on\n"
                                           "    1 2\n"
                                           "    3 4\n"
                                           "    5 6\n"
                                           "  endline\n"
                                           "  area water\n"
                                           "    line-2\n"
                                           "  endarea\n"
                                           "endscrap\n"),
                "appendDraftAreaGeometry should insert area geometry into the requested target scrap and use IDs from that scrap.")) {
        return 1;
    }
    if (!expect(lineNumber == 11, "appendDraftAreaGeometry should report the targeted area line number.")) {
        return 1;
    }

    contents = QStringLiteral("scrap a\nendscrap\n");
    lineNumber = 0;
    errorMessage.clear();
    TherionDraftObjectOptions areaObjectOptions;
    areaObjectOptions.type = QStringLiteral("pebbles");
    areaObjectOptions.subtype = QStringLiteral("dry");
    areaObjectOptions.identifier = QStringLiteral("area-1");
    if (!expect(TherionDocumentEditor::appendDraftAreaGeometry(&contents,
                                                               {QStringLiteral("1 2"),
                                                                QStringLiteral("3 4"),
                                                                QStringLiteral("5 6")},
                                                               &lineNumber,
                                                               &errorMessage,
                                                               areaObjectOptions),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap a\n"
                                           "  line border -id line-1 -close on\n"
                                           "    1 2\n"
                                           "    3 4\n"
                                           "    5 6\n"
                                           "  endline\n"
                                           "  area pebbles -subtype dry -id area-1\n"
                                           "    line-1\n"
                                           "  endarea\n"
                                           "endscrap\n"),
                "appendDraftAreaGeometry should apply caller-provided area type, subtype, and ID options.")) {
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
    if (!expect(contents == QStringLiteral("station st-a -10 55.5\n"),
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

    contents = QStringLiteral("point station 10 20 station -name a4 % keep\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(-7.0, 8.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point station -7 8.5 station -name a4 % keep\r\n"),
                "rewritePointCoordinates should preserve CRLF and percent-comments while rewriting coordinates.")) {
        return 1;
    }

    contents = QStringLiteral("survey s\r\npoint station 10 20 station -name mixed\nendsurvey\r");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 2, QPointF(30.25, -40.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("survey s\r\npoint station 30.3 -40.5 station -name mixed\nendsurvey\r"),
                "rewritePointCoordinates should preserve mixed physical line endings when replacing coordinate tokens.")) {
        return 1;
    }

    contents = QStringLiteral("point station 397.50 -969.50 station -name a5\n");
    const QString originalPrecisionPointContents = contents;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(402.25, -965.75), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 1, QPointF(397.5, -969.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == originalPrecisionPointContents,
                "rewritePointCoordinates should preserve coordinate precision style so rewrite-back restores original text.")) {
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
                                           "  10 20 70 80 -subtype 100 200\n"
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
    if (!expect(contents == QStringLiteral("line wall -id line-1 -3 9 -subtype 100 200\n"
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
    if (!expect(contents == QStringLiteral("line wall -id line-1 -subtype temp 1 2 10 20\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should keep start-line numeric token ordering and rewrite the first writable vertex pair.")) {
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
                                           "  10 20 77 -88\n"
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
                                           "  10 20 77 66\n"
                                           "  smooth off 50 60\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should rewrite only real geometry vertices and keep non-coordinate continuation metadata unchanged.")) {
        return 1;
    }

    contents = QStringLiteral("area water\n"
                              "  1 2 3 4\n"
                              "  smooth off 9 9\n"
                              "  -subtype temporary 11 12\n"
                              "  5 6 # anchor\n"
                              "endarea\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("area"), 2, QPointF(77.0, 88.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("area water\n"
                                           "  1 2 3 4\n"
                                           "  smooth off 9 9\n"
                                           "  -subtype temporary 11 12\n"
                                           "  77 88 # anchor\n"
                                           "endarea\n"),
                "rewriteLineAreaVertex should ignore interleaved option/comment metadata lines and rewrite only area-coordinate continuation lines.")) {
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
                                           "  42 -9.5 # keep area note\n"
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
                                           "  \"10\" \"20\" 8 9\n"
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
                                           "  7 8\r\n"
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
                                           "  -11 12.5\n"
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

    contents = QStringLiteral("line wall -id line-2 1 2 3 4 % keep line note\n"
                              "  -subtype \"temp 1\" 900 800\n"
                              "  50 60\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 2, QPointF(-15.0, 16.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -id line-2 1 2 3 4 % keep line note\n"
                                           "  -subtype \"temp 1\" 900 800\n"
                                           "  -15 16\n"
                                           "endline\n"),
                "rewriteLineAreaVertex should ignore option-led continuation payload, keep percent-comments, and rewrite the correct vertex.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\r\n"
                              "  10 20\r\n"
                              "  \"30\" \"40\"\r\n"
                              "  50 60 % keep last vertex\r\n"
                              "endline\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 1, QPointF(99.0, -42.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\r\n"
                                           "  10 20\r\n"
                                           "  \"30\" \"40\"\r\n"
                                           "  99 -42 % keep last vertex\r\n"
                                           "endline\r\n"),
                "rewriteLineAreaVertex should preserve CRLF, ignore quoted numeric noise, and keep percent-comments.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  397.50 -969.50 378.50 -908.00\n"
                              "endline\n");
    const QString originalPrecisionLineContents = contents;
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(400.25, -960.75), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 1, QStringLiteral("line"), 0, QPointF(397.5, -969.5), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == originalPrecisionLineContents,
                "rewriteLineAreaVertex should preserve coordinate precision style so rewrite-back restores original text.")) {
        return 1;
    }

    return 0;
}

int runRewriteLineOptionToggleTest()
{
    QString errorMessage;

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineOptionToggle(nullptr, 1, QStringLiteral("-close"), true, &errorMessage),
                "rewriteLineOptionToggle should reject null contents.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineOptionToggle should report null-content error.")) {
        return 1;
    }

    QString contents = QStringLiteral("point station 1 2 station -name a1\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineOptionToggle(&contents, 1, QStringLiteral("-close"), true, &errorMessage),
                "rewriteLineOptionToggle should reject non-line directives.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineOptionToggle should report non-line directive errors.")) {
        return 1;
    }

    contents = QStringLiteral("line wall # keep\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineOptionToggle(&contents, 1, QStringLiteral("-close"), true, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -close on # keep\n"),
                "rewriteLineOptionToggle should append missing toggle options before comments.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -close\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineOptionToggle(&contents, 1, QStringLiteral("-close"), false, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -close off\n"),
                "rewriteLineOptionToggle should expand bare options with explicit off value when disabling.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -close off -reverse on\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineOptionToggle(&contents, 1, QStringLiteral("-reverse"), false, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -close off -reverse off\r\n"),
                "rewriteLineOptionToggle should rewrite existing toggle values and preserve CRLF.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -close on -reverse off\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineOptionToggle(&contents, 1, QStringLiteral("reverse"), true, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -close on -reverse on\n"),
                "rewriteLineOptionToggle should accept normalized option names without leading dashes.")) {
        return 1;
    }

    return 0;
}

int runRewriteLineCoordinateRowsTest()
{
    QString errorMessage;

    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineCoordinateRows(nullptr, 1, {QStringLiteral("1 2")}, &errorMessage),
                "rewriteLineCoordinateRows should reject null contents.")) {
        return 1;
    }
    if (!expect(!errorMessage.isEmpty(), "rewriteLineCoordinateRows should report null-content error.")) {
        return 1;
    }

    QString contents = QStringLiteral("point station 1 2 station -name a1\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineCoordinateRows(&contents, 1, {QStringLiteral("1 2")}, &errorMessage),
                "rewriteLineCoordinateRows should reject non-line directives.")) {
        return 1;
    }

    contents = QStringLiteral("line wall 1 2 3 4\nendline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteLineCoordinateRows(&contents, 1, {QStringLiteral("5 6")}, &errorMessage),
                "rewriteLineCoordinateRows should reject start-line inline coordinate rewrites.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n  1 2\n  smooth off\n  3 4\nendline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineCoordinateRows(&contents,
                                                                  1,
                                                                  {QStringLiteral("5 6"),
                                                                   QStringLiteral("smooth off"),
                                                                   QStringLiteral("7 8")},
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  5 6\n"
                                           "  smooth off\n"
                                           "  7 8\n"
                                           "endline\n"),
                "rewriteLineCoordinateRows should allow smooth-off continuation lines and preserve them in rewritten output.")) {
        return 1;
    }

    contents = QStringLiteral("line slope\n"
                              "  0 0\n"
                              "  l-size 40.0\n"
                              "  100 0\n"
                              "  orientation 90\n"
                              "  100 100\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineCoordinateRows(&contents,
                                                                  1,
                                                                  {QStringLiteral("0.0 0.0"),
                                                                   QStringLiteral("l-size 40.0"),
                                                                   QStringLiteral("33.3 0.0 66.7 0.0 100.0 0.0"),
                                                                   QStringLiteral("orientation 90.000"),
                                                                   QStringLiteral("100.0 33.3 100.0 66.7 100.0 100.0")},
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  0.0 0.0\n"
                                           "  l-size 40.0\n"
                                           "  33.3 0.0 66.7 0.0 100.0 0.0\n"
                                           "  orientation 90.000\n"
                                           "  100.0 33.3 100.0 66.7 100.0 100.0\n"
                                           "endline\n"),
                "rewriteLineCoordinateRows should allow supported standalone line-point option rows during Bezier rewrites.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\n"
                              "  1 2\n"
                              "  altitude .\n"
                              "  adjust horizontal\n"
                              "  3 4\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineCoordinateRows(&contents,
                                                                  1,
                                                                  {QStringLiteral("10 20"),
                                                                   QStringLiteral("altitude ."),
                                                                   QStringLiteral("adjust horizontal"),
                                                                   QStringLiteral("30 40")},
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\n"
                                           "  10 20\n"
                                           "  altitude .\n"
                                           "  adjust horizontal\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLineCoordinateRows should allow arbitrary standalone line-point rows and keep them in output.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -id line-1 # keep\n"
                              "  10 20 30 40\n"
                              "  50 60\n"
                              "endline\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineCoordinateRows(&contents,
                                                                  1,
                                                                  {QStringLiteral("11 22 33 44 55 66"),
                                                                   QStringLiteral("77 88")},
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -id line-1 # keep\r\n"
                                           "  11 22 33 44 55 66\r\n"
                                           "  77 88\r\n"
                                           "endline\r\n"),
                "rewriteLineCoordinateRows should replace coordinate rows and preserve start/end lines with CRLF.")) {
        return 1;
    }

    return 0;
}

int runCorpusStyleRewriteFixtureTest()
{
    QString errorMessage;
    QString contents = QStringLiteral(
        "encoding utf-8\r\n"
        "##XTHERION## xth_me_area_adjust -128 -1152 851 128\r\n"
        "scrap clopy01 -projection plan -author 1985.04.29 \"Hugo Havel\" -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]\r\n"
        "  point 397.5 -969.5 station -name 0@hp\r\n"
        "  point 378.5 -908.0 station -name 1@hp\r\n"
        "  line wall -id line-8\r\n"
        "    445 -796.5 435 -800.5 432 -776.5\r\n"
        "    smooth off\r\n"
        "    413.25 -624.25 422.8 -574.1 420.5 -562\r\n"
        "    -subtype temporary 100 200\r\n"
        "  endline\r\n"
        "  area water -id area-1\r\n"
        "    404.0 -958.5 395.5 -959.5 394.0 -945.5\r\n"
        "    smooth off 999 888\r\n"
        "    384.53 -909.37 388.05 -914.52 397.5 -913.5 % anchor note\r\n"
        "  endarea\r\n"
        "endscrap\r\n");

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointCoordinates(&contents, 4, QPointF(400.0, -970.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 6, QStringLiteral("line"), 4, QPointF(421.0, -573.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLineAreaVertex(&contents, 12, QStringLiteral("area"), 5, QPointF(398.0, -912.0), &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }

    const QString expected = QStringLiteral(
        "encoding utf-8\r\n"
        "##XTHERION## xth_me_area_adjust -128 -1152 851 128\r\n"
        "scrap clopy01 -projection plan -author 1985.04.29 \"Hugo Havel\" -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]\r\n"
        "  point 400.0 -970.0 station -name 0@hp\r\n"
        "  point 378.5 -908.0 station -name 1@hp\r\n"
        "  line wall -id line-8\r\n"
        "    445 -796.5 435 -800.5 432 -776.5\r\n"
        "    smooth off\r\n"
        "    413.25 -624.25 421.0 -573.0 420.5 -562\r\n"
        "    -subtype temporary 100 200\r\n"
        "  endline\r\n"
        "  area water -id area-1\r\n"
        "    404.0 -958.5 395.5 -959.5 394.0 -945.5\r\n"
        "    smooth off 999 888\r\n"
        "    384.53 -909.37 388.05 -914.52 398.0 -912.0 % anchor note\r\n"
        "  endarea\r\n"
        "endscrap\r\n");

    if (!expect(contents == expected,
                "Corpus-style rewrite fixture should update only targeted vertices while preserving comments, metadata, and CRLF formatting.")) {
        return 1;
    }

    const auto expectRewriteFailureWithoutMutation = [&](const QString &label,
                                                         const QString &originalContents,
                                                         const std::function<bool(QString *, QString *)> &rewrite) {
        QString candidate = originalContents;
        QString candidateError;
        if (rewrite(&candidate, &candidateError)) {
            std::cerr << label.toUtf8().constData() << '\n';
            return false;
        }

        if (candidate != originalContents) {
            std::cerr << "Corpus-style failure path mutated document contents unexpectedly.\n";
            return false;
        }

        if (candidateError.trimmed().isEmpty()) {
            std::cerr << "Corpus-style failure path did not provide an error message.\n";
            return false;
        }

        return true;
    };

    const QString malformedLineBlock = QStringLiteral(
        "encoding utf-8\r\n"
        "scrap broken -projection plan\r\n"
        "  line wall -id line-bad\r\n"
        "    1 2 3 4\r\n"
        "    smooth off 9 9\r\n"
        "endscrap\r\n");
    if (!expect(expectRewriteFailureWithoutMutation(
                    QStringLiteral("rewriteLineAreaVertex should fail when corpus-like line blocks are missing endline."),
                    malformedLineBlock,
                    [](QString *candidate, QString *candidateError) {
                        return TherionDocumentEditor::rewriteLineAreaVertex(candidate,
                                                                            3,
                                                                            QStringLiteral("line"),
                                                                            1,
                                                                            QPointF(10.0, 20.0),
                                                                            candidateError);
                    }),
                "rewriteLineAreaVertex should fail without mutating malformed corpus-like line blocks.")) {
        return 1;
    }

    const QString malformedAreaTuple = QStringLiteral(
        "encoding utf-8\r\n"
        "scrap broken -projection plan\r\n"
        "  area water -id area-bad\r\n"
        "    1 2 3\r\n"
        "  endarea\r\n"
        "endscrap\r\n");
    if (!expect(expectRewriteFailureWithoutMutation(
                    QStringLiteral("rewriteLineAreaVertex should fail when area coordinate tuples are incomplete."),
                    malformedAreaTuple,
                    [](QString *candidate, QString *candidateError) {
                        return TherionDocumentEditor::rewriteLineAreaVertex(candidate,
                                                                            3,
                                                                            QStringLiteral("area"),
                                                                            1,
                                                                            QPointF(8.0, 9.0),
                                                                            candidateError);
                    }),
                "rewriteLineAreaVertex should fail without mutating malformed corpus-like area tuples.")) {
        return 1;
    }

    return 0;
}

int runRewriteOrientationOptionsTest()
{
    QString errorMessage;

    QString contents = QStringLiteral("point 10 20 station -name a1\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointOrientation(&contents, 1, true, 370.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 station -name a1 -orientation 10\n"),
                "rewritePointOrientation should append normalized orientation in 0..<360 range.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointOrientation(&contents, 1, false, 0.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 station -name a1\n"),
                "rewritePointOrientation should remove orientation option when disabled.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s\r\npoint 10 20 station -name a2 # keep\nendscrap\r");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewritePointOrientation(&contents, 2, true, 45.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s\r\npoint 10 20 station -name a2 -orientation 45 # keep\nendscrap\r"),
                "rewritePointOrientation should preserve mixed physical line endings when replacing one command line.")) {
        return 1;
    }

    contents = QStringLiteral("line slope\n"
                              "  10 20 -orientation 45\n"
                              "  30 40\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointOrientation(&contents, 1, 0, true, -15.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20 -orientation 345\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLinePointOrientation should rewrite orientation for selected source vertex row.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointOrientation(&contents, 1, 1, true, 90.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20 -orientation 345\n"
                                           "  30 40\n"
                                           "  orientation 90\n"
                                           "endline\n"),
                "rewriteLinePointOrientation should append XTherion-style orientation row when missing.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointOrientation(&contents, 1, 1, false, 0.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20 -orientation 345\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLinePointOrientation should remove orientation option row when disabled.")) {
        return 1;
    }

    contents = QStringLiteral("line slope\n"
                              "  10 20\n"
                              "  orientation 45\n"
                              "  30 40\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointOrientation(&contents, 1, 0, true, 180.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20\n"
                                           "  orientation 180\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLinePointOrientation should rewrite standalone XTherion orientation rows.")) {
        return 1;
    }

    contents = QStringLiteral("line slope\n"
                              "  10 20\n"
                              "  30 40\n"
                              "endline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointLeftSize(&contents, 1, 0, true, 40.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20\n"
                                           "  l-size 40.0\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLinePointLeftSize should append XTherion-style l-size rows.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointLeftSize(&contents, 1, 0, true, 12.5, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20\n"
                                           "  l-size 12.5\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLinePointLeftSize should rewrite existing l-size rows.")) {
        return 1;
    }

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteLinePointLeftSize(&contents, 1, 0, false, 0.0, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line slope\n"
                                           "  10 20\n"
                                           "  30 40\n"
                                           "endline\n"),
                "rewriteLinePointLeftSize should remove standalone l-size rows.")) {
        return 1;
    }

    return 0;
}

int runRewriteScrapScaleTest()
{
    QString errorMessage;
    QString contents = QStringLiteral("scrap s1 -projection plan\nendscrap\n");

    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteScrapScale(&contents,
                                                         1,
                                                         QStringLiteral("[0 0 100 0 0 0 10 0 m]"),
                                                         &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -projection plan -scale [0 0 100 0 0 0 10 0 m]\nendscrap\n"),
                "rewriteScrapScale should append a missing scrap scale option.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s1 -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m] -projection plan\r\nendscrap\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteScrapScale(&contents,
                                                         1,
                                                         QStringLiteral("[1 2 3 4 5 6 7 8 m]"),
                                                         &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -scale [1 2 3 4 5 6 7 8 m] -projection plan\r\nendscrap\r\n"),
                "rewriteScrapScale should replace bracketed XTherion/Therion scale and preserve CRLF.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s1 -projection plan -scale 100 10 m -author 2026.01.01 Test\nendscrap\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteScrapScale(&contents,
                                                         1,
                                                         QStringLiteral("[0 0 100 0 0 0 10 0 m]"),
                                                         &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -projection plan -scale [0 0 100 0 0 0 10 0 m] -author 2026.01.01 Test\nendscrap\n"),
                "rewriteScrapScale should replace non-bracketed multi-token scale values without consuming following options.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s1 -projection plan # keep comment\nendscrap\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteScrapScale(&contents,
                                                         1,
                                                         QStringLiteral("[0 0 1 0 0 0 1 0 m]"),
                                                         &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -projection plan -scale [0 0 1 0 0 0 1 0 m] # keep comment\nendscrap\n"),
                "rewriteScrapScale should insert the scale option before an inline comment.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\nendline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteScrapScale(&contents,
                                                          1,
                                                          QStringLiteral("[0 0 1 0 0 0 1 0 m]"),
                                                          &errorMessage),
                "rewriteScrapScale should reject non-scrap commands.")) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\nendline\n"), "rewriteScrapScale should not mutate rejected commands.")) {
        return 1;
    }

    return 0;
}

int runRewriteMapObjectQuickFieldsTest()
{
    QString errorMessage;
    QString contents = QStringLiteral("scrap old -projection plan # keep\nendscrap\n");
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QString(),
                                                                   QString(),
                                                                   QStringLiteral("new-scrap"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap new-scrap -projection plan # keep\nendscrap\n"),
                "rewriteMapObjectQuickFields should rewrite scrap ID and preserve other content.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -id old -close on # keep\nendline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("border"),
                                                                   QStringLiteral("invisible"),
                                                                   QStringLiteral("line-1"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line border -id line-1 -close on -subtype invisible # keep\nendline\n"),
                "rewriteMapObjectQuickFields should rewrite line type, id, and append subtype before comments.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -id line-1 -subtype invisible # keep\nendline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("wall"),
                                                                   QString(),
                                                                   QStringLiteral("line-1"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall -id line-1 # keep\nendline\n"),
                "rewriteMapObjectQuickFields should remove cleared line subtype.")) {
        return 1;
    }

    contents = QStringLiteral("line rock-border -close on -clip off\nendline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("rock-border"),
                                                                   QStringLiteral("-clip off"),
                                                                   QString(),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line rock-border -close on -clip off\nendline\n"),
                "rewriteMapObjectQuickFields should ignore option-like line subtype values.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 station -name old-name\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("station"),
                                                                   QString(),
                                                                   QStringLiteral("point-1"),
                                                                   QStringLiteral("1@hp"),
                                                                   true,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 station -name 1@hp -id point-1\n"),
                "rewriteMapObjectQuickFields should rewrite point ID separately from station name.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 -id old\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("station"),
                                                                   QString(),
                                                                   QStringLiteral("point-2"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 station -id point-2\n"),
                "rewriteMapObjectQuickFields should insert a missing point type after coordinates.")) {
        return 1;
    }

    contents = QStringLiteral("point 463.0 -495.75 station-name -id old\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("station"),
                                                                   QStringLiteral("fixed"),
                                                                   QStringLiteral("point-3"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 463.0 -495.75 station -id point-3 -subtype fixed\n"),
                "rewriteMapObjectQuickFields should treat negative point coordinates as coordinates, not options.")) {
        return 1;
    }

    contents = QStringLiteral("point 463.0 -495.75 station -id point-3 -subtype fixed\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("station"),
                                                                   QString(),
                                                                   QStringLiteral("point-3"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 463.0 -495.75 station -id point-3\n"),
                "rewriteMapObjectQuickFields should remove cleared point subtype.")) {
        return 1;
    }

    contents = QStringLiteral("area water -id a1 -subtype temporary\nendarea\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("sand"),
                                                                   QStringLiteral("blocks"),
                                                                   QStringLiteral("area-1"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("area sand -id area-1 -subtype blocks\nendarea\n"),
                "rewriteMapObjectQuickFields should rewrite area type, subtype, and ID.")) {
        return 1;
    }

    contents = QStringLiteral("area water -id a1 -subtype temporary\nendarea\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   1,
                                                                   QStringLiteral("sand"),
                                                                   QString(),
                                                                   QString(),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("area sand\nendarea\n"),
                "rewriteMapObjectQuickFields should remove cleared area subtype and ID options.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s1\r\nline wall -id old # keep\nendline\rendscrap");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectQuickFields(&contents,
                                                                   2,
                                                                   QStringLiteral("border"),
                                                                   QStringLiteral("invisible"),
                                                                   QStringLiteral("line-1"),
                                                                   QString(),
                                                                   false,
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1\r\nline border -id line-1 -subtype invisible # keep\nendline\rendscrap"),
                "rewriteMapObjectQuickFields should preserve mixed physical line endings when replacing one command line.")) {
        return 1;
    }

    contents = QStringLiteral("line label -id l1 # keep\n  0 0\n  10 0\nendline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectTextOption(&contents,
                                                                  1,
                                                                  QStringLiteral("Main passage"),
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line label -id l1 -text \"Main passage\" # keep\n  0 0\n  10 0\nendline\n"),
                "rewriteMapObjectTextOption should insert line label text before comments.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 label -text old\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectTextOption(&contents,
                                                                  1,
                                                                  QStringLiteral("New label"),
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 label -text \"New label\"\n"),
                "rewriteMapObjectTextOption should replace point label text.")) {
        return 1;
    }

    contents = QStringLiteral("line wall -text stale\nendline\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectTextOption(&contents,
                                                                  1,
                                                                  QString(),
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\nendline\n"),
                "rewriteMapObjectTextOption should allow clearing stale label text after type changes.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\nendline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteMapObjectTextOption(&contents,
                                                                   1,
                                                                   QStringLiteral("Invalid"),
                                                                   &errorMessage),
                "rewriteMapObjectTextOption should reject non-label object text.")) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\nendline\n"),
                "rewriteMapObjectTextOption should not mutate rejected non-label objects.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s\r\nline label # keep\nendline\nendscrap");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectTextOption(&contents,
                                                                  2,
                                                                  QStringLiteral("Mixed label"),
                                                                  &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s\r\nline label -text \"Mixed label\" # keep\nendline\nendscrap"),
                "rewriteMapObjectTextOption should preserve mixed physical line endings when replacing one command line.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 altitude # keep\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectValueOption(&contents,
                                                                   1,
                                                                   QStringLiteral("[fix 1300]"),
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 altitude -value [fix 1300] # keep\n"),
                "rewriteMapObjectValueOption should insert bracketed point values before comments.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 passage-height -value [+4 -2 m]\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectValueOption(&contents,
                                                                   1,
                                                                   QStringLiteral("3.5"),
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 passage-height -value 3.5\n"),
                "rewriteMapObjectValueOption should replace the full bracketed value group.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 station -value stale\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectValueOption(&contents,
                                                                   1,
                                                                   QString(),
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 station\n"),
                "rewriteMapObjectValueOption should allow clearing stale values after type changes.")) {
        return 1;
    }

    contents = QStringLiteral("point 10 20 station\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteMapObjectValueOption(&contents,
                                                                    1,
                                                                    QStringLiteral("42"),
                                                                    &errorMessage),
                "rewriteMapObjectValueOption should reject unsupported point value types.")) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("point 10 20 station\n"),
                "rewriteMapObjectValueOption should not mutate rejected unsupported point types.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s\r\npoint 10 20 altitude # keep\nendscrap\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteMapObjectValueOption(&contents,
                                                                   2,
                                                                   QStringLiteral("[fix 1200]"),
                                                                   &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s\r\npoint 10 20 altitude -value [fix 1200] # keep\nendscrap\n"),
                "rewriteMapObjectValueOption should preserve mixed physical line endings when replacing one command line.")) {
        return 1;
    }

    return 0;
}

int runRewriteScrapProjectionTest()
{
    QString errorMessage;
    QString contents = QStringLiteral("scrap s1 # keep\nendscrap\n");
    if (!expect(TherionDocumentEditor::rewriteScrapProjection(&contents,
                                                              1,
                                                              QStringLiteral("plan"),
                                                              &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -projection plan # keep\nendscrap\n"),
                "rewriteScrapProjection should append a missing projection before comments.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s1 -projection [elevation 10 deg] -scale [0 0 1 0 0 0 1 0 m]\r\nendscrap\r\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteScrapProjection(&contents,
                                                              1,
                                                              QStringLiteral("extended"),
                                                              &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -projection extended -scale [0 0 1 0 0 0 1 0 m]\r\nendscrap\r\n"),
                "rewriteScrapProjection should replace bracketed projections and preserve CRLF.")) {
        return 1;
    }

    contents = QStringLiteral("scrap s1 -projection plan -author 2026.01.01 Test # keep\nendscrap\n");
    errorMessage.clear();
    if (!expect(TherionDocumentEditor::rewriteScrapProjection(&contents,
                                                              1,
                                                              QString(),
                                                              &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("scrap s1 -author 2026.01.01 Test # keep\nendscrap\n"),
                "rewriteScrapProjection should remove a cleared projection without consuming following options.")) {
        return 1;
    }

    contents = QStringLiteral("line wall\nendline\n");
    errorMessage.clear();
    if (!expect(!TherionDocumentEditor::rewriteScrapProjection(&contents,
                                                               1,
                                                               QStringLiteral("plan"),
                                                               &errorMessage),
                "rewriteScrapProjection should reject non-scrap commands.")) {
        return 1;
    }
    if (!expect(contents == QStringLiteral("line wall\nendline\n"),
                "rewriteScrapProjection should not mutate rejected commands.")) {
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

    const int rewriteLineAreaResult = runRewriteLineAreaVertexTest();
    if (rewriteLineAreaResult != 0) {
        return rewriteLineAreaResult;
    }

    const int rewriteLineOptionToggleResult = runRewriteLineOptionToggleTest();
    if (rewriteLineOptionToggleResult != 0) {
        return rewriteLineOptionToggleResult;
    }

    const int rewriteOrientationOptionsResult = runRewriteOrientationOptionsTest();
    if (rewriteOrientationOptionsResult != 0) {
        return rewriteOrientationOptionsResult;
    }

    const int rewriteScrapScaleResult = runRewriteScrapScaleTest();
    if (rewriteScrapScaleResult != 0) {
        return rewriteScrapScaleResult;
    }

    const int rewriteScrapProjectionResult = runRewriteScrapProjectionTest();
    if (rewriteScrapProjectionResult != 0) {
        return rewriteScrapProjectionResult;
    }

    const int rewriteMapObjectQuickFieldsResult = runRewriteMapObjectQuickFieldsTest();
    if (rewriteMapObjectQuickFieldsResult != 0) {
        return rewriteMapObjectQuickFieldsResult;
    }

    const int rewriteLineCoordinateRowsResult = runRewriteLineCoordinateRowsTest();
    if (rewriteLineCoordinateRowsResult != 0) {
        return rewriteLineCoordinateRowsResult;
    }

    return runCorpusStyleRewriteFixtureTest();
}
