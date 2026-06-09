#include "../src/core/TherionSourceLogicalDocument.h"

#include <cstdlib>
#include <cstdio>

using namespace TherionStudio;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionSourceLogicalDocumentTest failed: %s\n", message);
        std::exit(1);
    }
}

const TherionSourceLogicalCommand &commandAt(const TherionSourceLogicalDocument &document,
                                             int zeroBasedIndex)
{
    if (zeroBasedIndex < 0 || zeroBasedIndex >= document.commands().size()) {
        std::fprintf(stderr,
                     "TherionSourceLogicalDocumentTest failed: missing command index %d\n",
                     zeroBasedIndex);
        std::exit(1);
    }
    return document.commands().at(zeroBasedIndex);
}

void groupsContinuationLinesIntoOneLogicalCommand()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    require(document.commands().size() == 3,
            "logical document should group continuation lines while preserving other physical entries");

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    require(survey.startLineNumber == 1 && survey.endLineNumber == 2,
            "continued survey command should span both physical lines");
    require(survey.physicalLineNumbers == QVector<int>({1, 2}),
            "continued survey command should list all physical line numbers");
    require(survey.normalizedDirective == QStringLiteral("survey"),
            "continued command should parse from the first directive token");
    require(survey.parsed.tokens.contains(QStringLiteral("-person-rename")),
            "continued command should include option tokens from the continuation line");
    require(survey.shouldValidateCommandCatalog(),
            "continued command rows should remain validator candidates");

    const TherionSourceLogicalCommand &close = commandAt(document, 1);
    require(close.normalizedDirective == QStringLiteral("endsurvey"),
            "line after continuation should remain a separate logical command");
    require(!close.hasUnmatchedClose(),
            "logical close should preserve source document block matching state");
}

void keepsBlockContentRowsAsNonCommandLogicalEntries()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "scrap s1\n"
        "line wall\n"
        "  0 0\n"
        "  smooth off\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n"));

    const TherionSourceLogicalCommand &coordinate = commandAt(document, 2);
    require(coordinate.role == TherionSourceLineRole::BlockContent,
            "line coordinates should remain block-content logical entries");
    require(!coordinate.shouldValidateCommandCatalog(),
            "line coordinates should not be validator command candidates");

    const TherionSourceLogicalCommand &areaReference = commandAt(document, 6);
    require(areaReference.role == TherionSourceLineRole::BlockContent,
            "area references should remain block-content logical entries");
    require(!areaReference.shouldValidateCommandCatalog(),
            "area references should not be validator command candidates");
}

void normalizesCentrelineAliasOnLogicalCommands()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "centreline\n"
        "  data normal from to length compass clino\n"
        "endcentreline\n"));

    const TherionSourceLogicalCommand &open = commandAt(document, 0);
    require(open.normalizedDirective == QStringLiteral("centerline"),
            "logical document should normalize centreline open directive");
    require(open.opensBlock,
            "logical centreline alias should preserve block opener state");

    const TherionSourceLogicalCommand &body = commandAt(document, 1);
    require(body.role == TherionSourceLineRole::BlockContent,
            "centerline body rows should remain block-content logical entries");

    const TherionSourceLogicalCommand &close = commandAt(document, 2);
    require(close.normalizedDirective == QStringLiteral("endcenterline"),
            "logical document should normalize centreline close directive");
    require(!close.hasUnmatchedClose(),
            "logical centreline close alias should preserve matching state");
}
}

int main()
{
    groupsContinuationLinesIntoOneLogicalCommand();
    keepsBlockContentRowsAsNonCommandLogicalEntries();
    normalizesCentrelineAliasOnLogicalCommands();
    return 0;
}
