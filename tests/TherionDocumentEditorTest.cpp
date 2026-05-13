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
}

int main()
{
    return runRewritePreservesOtherContentTest();
}