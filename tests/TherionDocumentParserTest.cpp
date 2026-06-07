#include "../src/core/TherionDocumentParser.h"

#include <QCoreApplication>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

using namespace TherionStudio;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionDocumentParserTest failed: %s\n", message);
        std::exit(1);
    }
}

void parsesQuotedStringsAndComments()
{
    const TherionParsedLine parsed = TherionDocumentParser::parseLine(
        QStringLiteral("point 1 2 label -text \"Entrance #1\" # visible label"),
        7);

    require(parsed.lineNumber == 7, "line number should be preserved");
    require(parsed.rawText == QStringLiteral("point 1 2 label -text \"Entrance #1\" # visible label"),
            "raw line text should be preserved");
    require(parsed.directive == QStringLiteral("point"), "first token should become the lowercase directive");
    require(parsed.tokens == QStringList({QStringLiteral("point"),
                                          QStringLiteral("1"),
                                          QStringLiteral("2"),
                                          QStringLiteral("label"),
                                          QStringLiteral("-text"),
                                          QStringLiteral("Entrance #1")}),
            "quoted text should stay one token and embedded comment characters should not start a comment");
    require(parsed.commentStart == 36, "trailing comment start should be tracked by source column");
    require(parsed.commentText == QStringLiteral(" visible label"), "comment text should exclude the marker");
    require(parsed.tokenSpans.size() == 7, "token spans should include command tokens plus the comment");
    require(parsed.tokenSpans.at(5).type == TherionTokenType::QuotedString,
            "quoted token span should be marked as a quoted string");
    require(parsed.tokenSpans.at(5).start == 22 && parsed.tokenSpans.at(5).length == 13,
            "quoted token span should include the quote delimiters in source coordinates");
    require(parsed.tokenSpans.at(5).text == QStringLiteral("Entrance #1"),
            "quoted token span text should expose the unquoted token value");
    require(parsed.tokenSpans.at(6).type == TherionTokenType::Comment,
            "trailing comment should be represented as a comment span");
    require(parsed.tokenSpans.at(6).text == QStringLiteral("# visible label"),
            "comment span text should preserve the marker");
}

void parsesCommentOnlyLinesWithoutDirectiveTokens()
{
    const TherionParsedLine hashComment = TherionDocumentParser::parseLine(QStringLiteral("   # survey note"), 12);
    require(hashComment.tokens.isEmpty(), "comment-only hash lines should not expose directive tokens");
    require(hashComment.directive.isEmpty(), "comment-only hash lines should not set a directive");
    require(hashComment.commentStart == 3, "hash comment start should preserve leading whitespace");
    require(hashComment.commentText == QStringLiteral(" survey note"), "hash comment text should exclude the marker");
    require(hashComment.tokenSpans.size() == 1 && hashComment.tokenSpans.at(0).type == TherionTokenType::Comment,
            "comment-only hash lines should still expose a comment span");

    const TherionParsedLine percentComment = TherionDocumentParser::parseLine(QStringLiteral("% temporary branch"), 13);
    require(percentComment.tokens.isEmpty(), "comment-only percent lines should not expose directive tokens");
    require(percentComment.commentStart == 0, "percent comment start should be tracked");
    require(percentComment.commentText == QStringLiteral(" temporary branch"), "percent comment text should exclude the marker");
    require(percentComment.tokenSpans.size() == 1 && percentComment.tokenSpans.at(0).text == QStringLiteral("% temporary branch"),
            "percent comment span should preserve the marker and text");
}

void parseTextKeepsOnlyTokenLinesWithPhysicalLineNumbers()
{
    const QVector<TherionParsedLine> parsed = TherionDocumentParser::parseText(QStringLiteral(
        "# file header\r\n"
        "\r\n"
        "survey cave\r\n"
        "% local note\r\n"
        "endsurvey\r\n"));

    require(parsed.size() == 2, "parseText should currently skip blank and comment-only physical lines");
    require(parsed.at(0).lineNumber == 3 && parsed.at(0).directive == QStringLiteral("survey"),
            "first retained line should keep its physical line number");
    require(parsed.at(1).lineNumber == 5 && parsed.at(1).directive == QStringLiteral("endsurvey"),
            "second retained line should keep its physical line number");
    require(parsed.at(0).rawText == QStringLiteral("survey cave"),
            "CRLF line endings should be stripped from parsed raw line text");
}

void parseSourceDocumentPreservesAllPhysicalLines()
{
    const QString text = QStringLiteral(
        "# file header\r\n"
        "\r"
        "survey cave\r\n"
        "  # local note\n"
        "  centerline\n"
        "    data normal from to tape compass clino\n"
        "  endcenterline\r"
        "endsurvey\r\n");

    const TherionParsedSourceDocument document = TherionDocumentParser::parseSourceDocument(text);
    require(document.toText() == text, "lossless source document should round-trip original text and line endings");
    require(document.lines.size() == 9, "lossless source document should keep trailing empty physical line");

    require(document.lines.at(0).lineNumber == 1
                && document.lines.at(0).isCommentOnly()
                && document.lines.at(0).lineEnding == QStringLiteral("\r\n"),
            "lossless source document should preserve comment-only CRLF lines");
    require(document.lines.at(1).lineNumber == 2
                && document.lines.at(1).isBlank()
                && document.lines.at(1).lineEnding == QStringLiteral("\r"),
            "lossless source document should preserve blank CR lines");
    require(document.lines.at(2).lineNumber == 3
                && document.lines.at(2).hasTokens()
                && document.lines.at(2).parsed.directive == QStringLiteral("survey"),
            "lossless source document should preserve parsed token lines with physical line numbers");
    require(document.lines.at(3).lineNumber == 4
                && document.lines.at(3).isCommentOnly()
                && document.lines.at(3).parsed.commentStart == 2,
            "lossless source document should preserve indented comment-only source columns");
    require(document.lines.last().lineNumber == 9
                && document.lines.last().isBlank()
                && document.lines.last().lineEnding.isEmpty(),
            "lossless source document should keep final empty line after trailing newline");

    const QVector<TherionParsedLine> tokenLines = document.tokenLines();
    require(tokenLines.size() == 5, "tokenLines should expose the legacy token-only projection");
    require(tokenLines.at(0).lineNumber == 3 && tokenLines.at(0).directive == QStringLiteral("survey"),
            "tokenLines should keep physical line numbers for token lines");
    require(tokenLines.last().lineNumber == 8 && tokenLines.last().directive == QStringLiteral("endsurvey"),
            "tokenLines should include the final command before the trailing empty line");
}

void parsesLinePointRowsAndReviseStations()
{
    const TherionParsedLine linePointRow = TherionDocumentParser::parseLine(
        QStringLiteral("  altitude . # keep auto altitude"),
        21);
    require(linePointRow.directive == QStringLiteral("altitude"), "standalone line-point rows should expose their directive");
    require(linePointRow.tokens == QStringList({QStringLiteral("altitude"), QStringLiteral(".")}),
            "standalone line-point row tokens should preserve dot values before comments");
    require(linePointRow.commentStart == 13 && linePointRow.commentText == QStringLiteral(" keep auto altitude"),
            "standalone line-point row comments should retain source position and text");

    const TherionParsedLine revise = TherionDocumentParser::parseLine(
        QStringLiteral("revise dd.s@dur.dur_dom -stations [4@monum.dur_dom 5@monum.dur_dom 6@monum.dur_dom]"),
        31);
    require(revise.directive == QStringLiteral("revise"), "revise command should expose its directive");
    require(revise.tokens == QStringList({QStringLiteral("revise"),
                                          QStringLiteral("dd.s@dur.dur_dom"),
                                          QStringLiteral("-stations"),
                                          QStringLiteral("[4@monum.dur_dom"),
                                          QStringLiteral("5@monum.dur_dom"),
                                          QStringLiteral("6@monum.dur_dom]")}),
            "parser should preserve revise station-list tokens for command option modeling");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    parsesQuotedStringsAndComments();
    parsesCommentOnlyLinesWithoutDirectiveTokens();
    parseTextKeepsOnlyTokenLinesWithPhysicalLineNumbers();
    parseSourceDocumentPreservesAllPhysicalLines();
    parsesLinePointRowsAndReviseStations();
    return 0;
}
