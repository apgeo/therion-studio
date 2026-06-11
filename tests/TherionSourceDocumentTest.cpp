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

void recordsNestedBlockRanges()
{
    const QString text = QStringLiteral(
        "survey demo\n"
        "  scrap s1\n"
        "    line wall\n"
        "      0 0\n"
        "    endline\n"
        "  endscrap\n"
        "endsurvey\n");
    const TherionSourceDocument document = TherionSourceDocument::fromText(text);
    const QVector<TherionSourceBlockRange> &ranges = document.blockRanges();

    require(ranges.size() == 3, "source document should record every opened block range");

    const TherionSourceBlockRange &survey = ranges.at(0);
    require(survey.directive == QStringLiteral("survey"), "first block range should be the survey");
    require(survey.openLineNumber == 1 && survey.closeLineNumber == 7,
            "survey block range should span its matching close line");
    require(survey.startOffset == 0 && survey.endOffset == text.size(),
            "survey block range should preserve absolute source offsets");
    require(survey.openLineText == QStringLiteral("survey demo")
                && survey.closeLineText == QStringLiteral("endsurvey"),
            "survey block range should preserve opener and closer source text");
    require(survey.parentStack.isEmpty(), "top-level survey should not have a parent stack");
    require(survey.isClosed(), "balanced survey range should report closed state");

    const TherionSourceBlockRange &scrap = ranges.at(1);
    require(scrap.directive == QStringLiteral("scrap"), "second block range should be the scrap");
    require(scrap.openLineNumber == 2 && scrap.closeLineNumber == 6,
            "scrap block range should span its matching close line");
    require(scrap.parentStack.size() == 1
                && scrap.parentStack.constFirst().directive == QStringLiteral("survey"),
            "scrap block range should record its survey parent");

    const TherionSourceBlockRange &line = ranges.at(2);
    require(line.directive == QStringLiteral("line"), "third block range should be the line");
    require(line.openLineNumber == 3 && line.closeLineNumber == 5,
            "line block range should span its matching close line");
    require(line.parentStack.size() == 2
                && line.parentStack.at(0).directive == QStringLiteral("survey")
                && line.parentStack.at(1).directive == QStringLiteral("scrap"),
            "line block range should record nested survey and scrap parents");
}

void keepsUnclosedBlockRangesOpen()
{
    const QString text = QStringLiteral(
        "survey demo\n"
        "scrap s1\n");
    const TherionSourceDocument document = TherionSourceDocument::fromText(text);
    const QVector<TherionSourceBlockRange> &ranges = document.blockRanges();

    require(ranges.size() == 2, "unclosed document should still record opened block ranges");
    require(ranges.at(0).directive == QStringLiteral("survey")
                && ranges.at(0).openLineNumber == 1
                && !ranges.at(0).isClosed(),
            "unclosed survey range should remain open");
    require(ranges.at(1).directive == QStringLiteral("scrap")
                && ranges.at(1).openLineNumber == 2
                && !ranges.at(1).isClosed(),
            "unclosed scrap range should remain open");
    require(ranges.at(1).parentStack.size() == 1
                && ranges.at(1).parentStack.constFirst().directive == QStringLiteral("survey"),
            "unclosed nested range should still record its parent stack");
}

void preservesSourceSnapshotMetadata()
{
    const TherionSourceDocument defaultDocument = TherionSourceDocument::fromText(QStringLiteral("survey demo\n"));
    require(defaultDocument.sourceType() == TherionSourceDocumentType::Unknown,
            "default source snapshot type should be unknown");
    require(defaultDocument.encodingName().isEmpty(),
            "default source snapshot encoding should be empty until file IO provides it");
    require(defaultDocument.revisionId() == 0,
            "default source snapshot revision should be zero");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.encodingName = QStringLiteral("windows-1250");
    metadata.revisionId = 42;

    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral("scrap s1\nendscrap\n"),
                                                                          metadata);
    require(document.metadata().sourceType == TherionSourceDocumentType::TherionMap,
            "source snapshot should preserve explicit document type metadata");
    require(document.metadata().encodingName == QStringLiteral("windows-1250"),
            "source snapshot should preserve explicit encoding metadata");
    require(document.metadata().revisionId == 42,
            "source snapshot should preserve explicit revision metadata");
    require(document.sourceType() == TherionSourceDocumentType::TherionMap
                && document.encodingName() == QStringLiteral("windows-1250")
                && document.revisionId() == 42,
            "source snapshot metadata accessors should mirror the stored metadata");
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
    recordsNestedBlockRanges();
    keepsUnclosedBlockRangesOpen();
    preservesSourceSnapshotMetadata();
    normalizesCentrelineAliases();
    return 0;
}
