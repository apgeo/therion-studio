#include "../src/core/TherionSourceText.h"
#include "../src/core/TherionStringUtils.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

using TherionStudio::TherionSourceText;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionSourceTextTest failed: %s\n", message);
        std::exit(1);
    }
}

void roundTripsMixedPhysicalLineEndings()
{
    const QString text = QStringLiteral("encoding utf-8\r\n# comment\nline wall\rpoint 1 2 station\n");
    const TherionSourceText source = TherionSourceText::fromText(text);

    require(source.toText() == text, "source text should preserve physical line endings exactly");
    require(source.physicalLines().size() == 5, "trailing newline should keep a trailing empty physical line");
    require(source.textLines() == QStringList({QStringLiteral("encoding utf-8"),
                                               QStringLiteral("# comment"),
                                               QStringLiteral("line wall"),
                                               QStringLiteral("point 1 2 station"),
                                               QString()}),
            "text lines should normalize CR, LF, and CRLF boundaries");
}

void matchesLegacySplitAndJoinBehavior()
{
    require(TherionSourceText::splitTextLines(QStringLiteral("a\r\nb\n"))
                == QStringList({QStringLiteral("a"), QStringLiteral("b"), QString()}),
            "split helper should preserve trailing empty parts");
    require(TherionStudio::splitLinesNormalizingLineEndings(QStringLiteral("a\rb\r\nc\n"))
                == QStringList({QStringLiteral("a"),
                                QStringLiteral("b"),
                                QStringLiteral("c"),
                                QString()}),
            "normalizing split helper should handle CR, CRLF, and LF boundaries consistently");

    require(TherionSourceText::joinTextLines(QStringLiteral("a\r\n"), {QStringLiteral("x"),
                                                                        QStringLiteral("y"),
                                                                        QString()})
                == QStringLiteral("x\r\ny\r\n"),
            "join helper should preserve the original CRLF preference");
}

void replacesLineRangeWithoutMergingFollowingLines()
{
    TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\r\nb\r\nc\r\n"));

    require(source.replaceLineRange(1, 1, {QStringLiteral("B1"), QStringLiteral("B2")}),
            "valid range should be replaceable");
    require(source.toText() == QStringLiteral("a\r\nB1\r\nB2\r\nc\r\n"),
            "replacement inserted before existing lines should keep a separator after the last replacement");
}

void rejectsInvalidRanges()
{
    TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\nb\n"));

    require(!source.replaceLineRange(-1, 1, {}), "negative start should be rejected");
    require(!source.replaceLineRange(1, -1, {}), "negative remove count should be rejected");
    require(!source.replaceLineRange(4, 0, {}), "start past the end should be rejected");
    require(!source.replaceLineRange(2, 2, {}), "range past the end should be rejected");
    require(source.toText() == QStringLiteral("a\nb\n"), "invalid ranges should not mutate source text");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    roundTripsMixedPhysicalLineEndings();
    matchesLegacySplitAndJoinBehavior();
    replacesLineRangeWithoutMergingFollowingLines();
    rejectsInvalidRanges();
    return 0;
}
