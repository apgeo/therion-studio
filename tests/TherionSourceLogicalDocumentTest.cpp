#include "../src/core/TherionSourceLogicalDocument.h"
#include "../src/core/TherionCommandSyntax.h"

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

void mapsLogicalTokenRangesBackToPhysicalLines()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    const int optionStart = survey.text.indexOf(QStringLiteral("-person-rename"));
    TherionSourcePhysicalRange range;
    require(optionStart >= 0,
            "continued command should contain the continuation option token");
    require(survey.physicalRangeForLogicalRange(optionStart,
                                                QStringLiteral("-person-rename").size(),
                                                &range),
            "logical continuation option token should map to a physical source range");
    require(range.lineNumber == 2,
            "continuation option token should map to the continuation physical line");
    require(range.columnNumber == 3,
            "continuation option token should preserve its physical source column");
    require(range.columnLength == QStringLiteral("-person-rename").size(),
            "continuation option token should preserve its physical source length");
    require(range.lineText == QStringLiteral("  -person-rename \"Old Name\" \"New Name\""),
            "physical range should carry the actual source line text for UI previews");
}

void exposesPhysicalRangesForParsedTokens()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    require(survey.tokenRanges.size() == survey.parsed.tokenSpans.size(),
            "logical command should expose one source range per parsed token span");

    const int titleValueIndex = survey.parsed.tokens.indexOf(QStringLiteral("Cave"));
    require(titleValueIndex >= 0, "test should find the quoted title value token");
    require(survey.tokenRanges.at(titleValueIndex).text == QStringLiteral("Cave"),
            "token range should preserve the parser token value");
    require(survey.tokenRanges.at(titleValueIndex).type == TherionTokenType::QuotedString,
            "token range should preserve quoted-string token type");
    require(survey.tokenRanges.at(titleValueIndex).physicalRange.lineNumber == 1,
            "quoted title token should map to the first physical line");
    require(survey.tokenRanges.at(titleValueIndex).physicalRange.columnNumber == 20,
            "quoted title token should preserve its physical source column including quote delimiter");
    require(survey.tokenRanges.at(titleValueIndex).physicalRange.columnLength == 6,
            "quoted title token range should include quote delimiters like parser token spans");

    const int renameOptionIndex = survey.parsed.tokens.indexOf(QStringLiteral("-person-rename"));
    require(renameOptionIndex >= 0, "test should find the continuation option token");
    TherionSourcePhysicalRange range;
    require(survey.physicalRangeForTokenIndex(renameOptionIndex, &range),
            "logical command should map a parsed token index to a physical source range");
    require(range.lineNumber == 2 && range.columnNumber == 3,
            "continuation option token range should map to its physical line and column");
    require(range.columnLength == QStringLiteral("-person-rename").size(),
            "continuation option token range should preserve source token length");
}

void exposesPositionalAndOptionEntryRanges()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    require(survey.positionalArgumentRanges.size() == 1,
            "survey command should expose the leading cave name as one positional argument");
    require(survey.positionalArgumentRanges.constFirst().text == QStringLiteral("cave"),
            "positional argument range should preserve the argument token value");
    require(survey.positionalArgumentRanges.constFirst().physicalRange.lineNumber == 1
                && survey.positionalArgumentRanges.constFirst().physicalRange.columnNumber == 8,
            "positional argument range should preserve physical source location");
    require(survey.positionalArgumentGroupRange.isValid()
                && survey.positionalArgumentGroupRange.firstTokenIndex == 1
                && survey.positionalArgumentGroupRange.lastTokenIndex == 1
                && survey.positionalArgumentGroupRange.text == QStringLiteral("cave"),
            "positional argument group should cover the leading cave name token");
    require(survey.metadata.commandName == QStringLiteral("survey"),
            "logical command metadata should expose the normalized command name");
    require(survey.metadata.positionalArgumentCount == 1,
            "logical command metadata should expose the positional argument count");

    require(survey.optionEntryRanges.size() == 2,
            "continued survey command should expose both option entries");
    const TherionSourceLogicalOptionEntryRange &title = survey.optionEntryRanges.at(0);
    require(title.key == QStringLiteral("-title"),
            "first option entry should be -title");
    require(title.optionTokenIndex == 2
                && title.firstValueTokenIndex == 3
                && title.lastValueTokenIndex == 3
                && title.nextTokenIndex == 4,
            "title option entry should expose parser-compatible token indexes");
    require(title.rawValueTokens == QStringList({QStringLiteral("Cave")})
                && title.logicalValueCount == 1,
            "title option entry should expose raw value tokens and logical value count");
    require(title.optionRange.lineNumber == 1 && title.optionRange.columnNumber == 13,
            "title option range should preserve physical source location");
    require(title.valueRanges.size() == 1
                && title.valueRanges.constFirst().text == QStringLiteral("Cave")
                && title.valueRanges.constFirst().physicalRange.lineNumber == 1
                && title.valueRanges.constFirst().physicalRange.columnNumber == 20,
            "title value range should preserve quoted value source location");
    require(title.valueGroupRange.isValid()
                && title.valueGroupRange.text == QStringLiteral("Cave")
                && title.valueGroupRange.firstTokenIndex == title.valueRanges.constFirst().tokenIndex
                && title.valueGroupRange.lastTokenIndex == title.valueRanges.constFirst().tokenIndex,
            "single-value option group should cover the title value token");
    require(survey.metadata.normalizedOptionNames.contains(QStringLiteral("-title")),
            "logical command metadata should expose normalized option names");
    require(survey.metadata.optionEntryIndexesByNormalizedName.value(QStringLiteral("-title")) == QVector<int>({0}),
            "logical command metadata should map normalized options to option entry indexes");

    const TherionSourceLogicalOptionEntryRange &rename = survey.optionEntryRanges.at(1);
    require(rename.key == QStringLiteral("-person-rename"),
            "second option entry should be -person-rename");
    require(rename.optionTokenIndex == 4
                && rename.firstValueTokenIndex == 5
                && rename.lastValueTokenIndex == 6
                && rename.nextTokenIndex == 7,
            "continued option entry should expose parser-compatible token indexes");
    require(rename.rawValueTokens == QStringList({QStringLiteral("Old Name"), QStringLiteral("New Name")})
                && rename.logicalValueCount == 2,
            "continued option entry should expose raw value tokens and logical value count");
    require(rename.optionRange.lineNumber == 2 && rename.optionRange.columnNumber == 3,
            "continued option entry should preserve its physical source location");
    require(rename.valueRanges.size() == 2
                && rename.valueRanges.at(0).text == QStringLiteral("Old Name")
                && rename.valueRanges.at(1).text == QStringLiteral("New Name"),
            "continued option entry should expose both quoted value ranges");
    require(rename.valueRanges.at(0).physicalRange.lineNumber == 2
                && rename.valueRanges.at(1).physicalRange.lineNumber == 2,
            "continued option values should preserve their physical source line");
    require(rename.valueGroupRange.isValid()
                && rename.valueGroupRange.firstTokenIndex == rename.valueRanges.constFirst().tokenIndex
                && rename.valueGroupRange.lastTokenIndex == rename.valueRanges.constLast().tokenIndex
                && rename.valueGroupRange.text == QStringLiteral("Old Name New Name")
                && rename.valueGroupRange.argumentRanges.size() == 2,
            "multi-value option group should cover both person-rename value tokens");
    require(survey.metadata.normalizedOptionNames.contains(QStringLiteral("-person-rename")),
            "logical command metadata should include continuation-line option names");
    require(survey.metadata.optionEntryIndexesByNormalizedName.value(QStringLiteral("-person-rename")) == QVector<int>({1}),
            "logical command metadata should map continuation-line options to option entry indexes");
}

void findsLogicalCommandsAndTokensByPhysicalCursorPosition()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand *continuedCommand = document.commandAtPhysicalLine(2);
    require(continuedCommand != nullptr
                && continuedCommand->startLineNumber == 1
                && continuedCommand->endLineNumber == 2
                && continuedCommand->normalizedDirective == QStringLiteral("survey"),
            "physical continuation line should resolve to the continued survey logical command");

    const TherionSourceLogicalTokenRange *continuedOption = document.tokenAtPhysicalPosition(2, 5);
    require(continuedOption != nullptr
                && continuedOption->text == QStringLiteral("-person-rename")
                && continuedOption->physicalRange.lineNumber == 2,
            "physical cursor position on continuation option should resolve to that logical token");

    const TherionSourceLogicalTokenRange *quotedTitle = document.tokenAtPhysicalPosition(1, 22);
    require(quotedTitle != nullptr
                && quotedTitle->text == QStringLiteral("Cave")
                && quotedTitle->type == TherionTokenType::QuotedString,
            "physical cursor position inside quoted title should resolve to the quoted logical token");

    require(document.tokenAtPhysicalPosition(3, 20) == nullptr,
            "cursor position outside any token should not resolve to a token range");
}

void attachesCatalogMetadataToLogicalCommands()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {QStringLiteral("survey"), QStringLiteral("line")};
    catalog.commandContexts.insert(QStringLiteral("survey"), {QStringLiteral("none"), QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("line"), {QStringLiteral("scrap")});
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("survey"), 1);
    catalog.commandOptionNames.insert(QStringLiteral("survey"),
                                      {QStringLiteral("-title"), QStringLiteral("-person-rename")});
    catalog.commandOptionNames.insert(QStringLiteral("line"),
                                      {QStringLiteral("-close"), QStringLiteral("-subtype")});
    catalog.commandArgumentAllowedValuesByKey.insert(commandArgumentValueKey(QStringLiteral("line"), 0),
                                                     {QStringLiteral("wall"), QStringLiteral("border")});
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"),
                                                                       QStringLiteral("-close")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionAllowedValuesByKey.insert(commandOptionValueKey(QStringLiteral("line"),
                                                                         QStringLiteral("-close")),
                                                   {QStringLiteral("on"), QStringLiteral("off")});
    catalog.commandSubtypeValuesByTypeKey.insert(commandSubtypeValueKey(QStringLiteral("line"),
                                                                        QStringLiteral("wall")),
                                                 {QStringLiteral("bedrock"), QStringLiteral("blocks")});

    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(
        QStringLiteral(
            "survey cave -title \"Cave\" \\\n"
            "  -person-rename \"Old Name\" \"New Name\"\n"
            "endsurvey\n"
            "scrap s1\n"
            "line wall -close maybe -subtype bedrock\n"
            "endline\n"
            "endscrap\n"),
        catalog);

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    require(survey.metadata.catalogCommandKnown,
            "catalog-enriched logical command metadata should mark known commands");
    require(survey.metadata.catalogRequiredPositionalCount == 1,
            "catalog-enriched logical command metadata should expose required positional arity");
    require(survey.metadata.catalogContexts == QStringList({QStringLiteral("none"), QStringLiteral("survey")}),
            "catalog-enriched logical command metadata should expose catalog contexts");
    require(survey.metadata.catalogCurrentContext == QStringLiteral("none"),
            "catalog-enriched logical command metadata should expose the current source context");
    require(survey.metadata.catalogContextAllowed,
            "catalog-enriched logical command metadata should mark top-level survey context as allowed");
    require(survey.metadata.catalogOptionNames.contains(QStringLiteral("-title"))
                && survey.metadata.catalogOptionNames.contains(QStringLiteral("-person-rename")),
            "catalog-enriched logical command metadata should expose known option names");

    const TherionSourceLogicalCommand &close = commandAt(document, 1);
    require(!close.metadata.catalogCommandKnown,
            "catalog-enriched logical command metadata should leave unknown commands marked unknown");

    const TherionSourceLogicalCommand &line = commandAt(document, 3);
    require(line.metadata.catalogCommandKnown,
            "catalog-enriched logical command metadata should mark line commands known");
    require(line.metadata.catalogContexts == QStringList({QStringLiteral("scrap")}),
            "catalog-enriched logical command metadata should expose command-specific catalog contexts");
    require(line.metadata.catalogCurrentContext == QStringLiteral("scrap"),
            "catalog-enriched logical command metadata should expose nested scrap context");
    require(line.metadata.catalogContextAllowed,
            "catalog-enriched logical command metadata should mark line-in-scrap context as allowed");
    require(line.metadata.catalogArgumentAllowedValuesByIndex.value(0)
                == QStringList({QStringLiteral("wall"), QStringLiteral("border")}),
            "catalog-enriched logical command metadata should expose positional argument allowed values");
    require(line.metadata.catalogOptionValueArityTokens.value(QStringLiteral("-close")) == QStringLiteral("EXACTLY_ONE"),
            "catalog-enriched logical command metadata should expose option value arity");
    require(line.metadata.catalogOptionAllowedValuesByName.value(QStringLiteral("-close"))
                == QStringList({QStringLiteral("on"), QStringLiteral("off")}),
            "catalog-enriched logical command metadata should expose option allowed values");
    require(line.metadata.catalogOptionAllowedValuesByName.value(QStringLiteral("-subtype"))
                == QStringList({QStringLiteral("bedrock"), QStringLiteral("blocks")}),
            "catalog-enriched logical command metadata should expose type-specific subtype values");
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

void carriesSourceSnapshotMetadata()
{
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;
    metadata.encodingName = QStringLiteral("UTF-8");
    metadata.revisionId = 7;

    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(
        QStringLiteral("source index.th\n"),
        metadata);

    require(document.metadata().sourceType == TherionSourceDocumentType::TherionConfig,
            "logical source projection should preserve source document type metadata");
    require(document.metadata().encodingName == QStringLiteral("UTF-8"),
            "logical source projection should preserve source encoding metadata");
    require(document.metadata().revisionId == 7,
            "logical source projection should preserve source revision metadata");
}
}

int main()
{
    groupsContinuationLinesIntoOneLogicalCommand();
    mapsLogicalTokenRangesBackToPhysicalLines();
    exposesPhysicalRangesForParsedTokens();
    exposesPositionalAndOptionEntryRanges();
    findsLogicalCommandsAndTokensByPhysicalCursorPosition();
    attachesCatalogMetadataToLogicalCommands();
    keepsBlockContentRowsAsNonCommandLogicalEntries();
    normalizesCentrelineAliasOnLogicalCommands();
    carriesSourceSnapshotMetadata();
    return 0;
}
