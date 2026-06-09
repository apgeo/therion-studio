#include "../src/core/TherionSourceDocument.h"

#include <cstdlib>
#include <cstdio>

using namespace TherionStudio;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionSourceDocumentTest failed: %s\n", message);
        std::exit(1);
    }
}

const TherionSourceDocumentLine &lineAt(const TherionSourceDocument &document, int oneBasedLineNumber)
{
    for (const TherionSourceDocumentLine &line : document.lines()) {
        if (line.sourceLine.lineNumber == oneBasedLineNumber) {
            return line;
        }
    }
    std::fprintf(stderr, "TherionSourceDocumentTest failed: missing line %d\n", oneBasedLineNumber);
    std::exit(1);
}

void classifiesCommandAndBlockContentLines()
{
    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral(
        "scrap s1\n"
        "line wall\n"
        "  0 0\n"
        "  smooth off\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n"));

    require(document.lines().size() == 10, "source document should preserve final physical line");
    require(lineAt(document, 1).role == TherionSourceLineRole::Command, "scrap opener should be a command line");
    require(lineAt(document, 2).role == TherionSourceLineRole::Command, "line opener should be a command line");
    require(lineAt(document, 3).role == TherionSourceLineRole::BlockContent, "line coordinates should be block content");
    require(lineAt(document, 4).role == TherionSourceLineRole::BlockContent, "line point directives should be block content");
    require(lineAt(document, 5).role == TherionSourceLineRole::Command, "line closer should be a command line");
    require(lineAt(document, 7).role == TherionSourceLineRole::BlockContent, "area references should be block content");
    require(document.openBlocksAtEnd().isEmpty(), "balanced document should not leave open blocks");
}

void recordsBlockCloseAndUnclosedState()
{
    const TherionSourceDocument unmatched = TherionSourceDocument::fromText(QStringLiteral("endscrap\n"));
    require(lineAt(unmatched, 1).hasUnmatchedClose(), "standalone endscrap should be unmatched");

    const TherionSourceDocument unclosed = TherionSourceDocument::fromText(QStringLiteral("survey demo\nscrap s1\n"));
    require(unclosed.openBlocksAtEnd().size() == 2, "unclosed document should retain open block stack");
    require(unclosed.openBlocksAtEnd().at(0).directive == QStringLiteral("survey"),
            "first open block should be survey");
    require(unclosed.openBlocksAtEnd().at(1).directive == QStringLiteral("scrap"),
            "second open block should be scrap");
}

void normalizesCentrelineAliases()
{
    require(normalizedTherionDirectiveToken(QStringLiteral("centreline")) == QStringLiteral("centerline"),
            "centreline should normalize to centerline");
    require(normalizedTherionDirectiveToken(QStringLiteral("endcentreline")) == QStringLiteral("endcenterline"),
            "endcentreline should normalize to endcenterline");

    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral(
        "centreline\n"
        "  data normal from to length compass clino\n"
        "endcentreline\n"));
    require(lineAt(document, 1).opensBlock, "centreline should be a block opener");
    require(lineAt(document, 2).role == TherionSourceLineRole::BlockContent,
            "centerline data rows should be block content");
    require(!lineAt(document, 3).hasUnmatchedClose(), "endcentreline should match centreline");
}
}

int main()
{
    classifiesCommandAndBlockContentLines();
    recordsBlockCloseAndUnclosedState();
    normalizesCentrelineAliases();
    return 0;
}
