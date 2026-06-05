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
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    parsesQuotedStringsAndComments();
    parsesCommentOnlyLinesWithoutDirectiveTokens();
    parseTextKeepsOnlyTokenLinesWithPhysicalLineNumbers();
    return 0;
}
