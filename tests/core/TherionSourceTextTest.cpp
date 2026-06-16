#include "../../src/core/TherionSourceText.h"
#include "../../src/core/TherionStringUtils.h"

#include <QString>
#include <QStringList>
#include <QtTest/QtTest>

using TherionStudio::TherionSourceText;

namespace
{
class TherionSourceTextTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsMixedPhysicalLineEndings();
    void matchesLegacySplitAndJoinBehavior();
    void replacesLineRangeWithoutMergingFollowingLines();
    void rejectsInvalidRanges();
};

void TherionSourceTextTest::roundTripsMixedPhysicalLineEndings()
{
    const QString text = QStringLiteral("encoding utf-8\r\n# comment\nline wall\rpoint 1 2 station\n");
    const TherionSourceText source = TherionSourceText::fromText(text);

    QCOMPARE(source.toText(), text);
    QCOMPARE(source.physicalLines().size(), 5);
    QCOMPARE(source.textLines(), QStringList({QStringLiteral("encoding utf-8"),
                                              QStringLiteral("# comment"),
                                              QStringLiteral("line wall"),
                                              QStringLiteral("point 1 2 station"),
                                              QString()}));
}

void TherionSourceTextTest::matchesLegacySplitAndJoinBehavior()
{
    QCOMPARE(TherionSourceText::splitTextLines(QStringLiteral("a\r\nb\n")),
             QStringList({QStringLiteral("a"), QStringLiteral("b"), QString()}));
    QCOMPARE(TherionStudio::splitLinesNormalizingLineEndings(QStringLiteral("a\rb\r\nc\n")),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("b"),
                          QStringLiteral("c"),
                          QString()}));

    QCOMPARE(TherionSourceText::joinTextLines(QStringLiteral("a\r\n"), {QStringLiteral("x"),
                                                                         QStringLiteral("y"),
                                                                         QString()}),
             QStringLiteral("x\r\ny\r\n"));
}

void TherionSourceTextTest::replacesLineRangeWithoutMergingFollowingLines()
{
    TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\r\nb\r\nc\r\n"));

    QVERIFY(source.replaceLineRange(1, 1, {QStringLiteral("B1"), QStringLiteral("B2")}));
    QCOMPARE(source.toText(), QStringLiteral("a\r\nB1\r\nB2\r\nc\r\n"));
}

void TherionSourceTextTest::rejectsInvalidRanges()
{
    TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\nb\n"));

    QVERIFY(!source.replaceLineRange(-1, 1, {}));
    QVERIFY(!source.replaceLineRange(1, -1, {}));
    QVERIFY(!source.replaceLineRange(4, 0, {}));
    QVERIFY(!source.replaceLineRange(2, 2, {}));
    QCOMPARE(source.toText(), QStringLiteral("a\nb\n"));
}
}

int runTherionSourceTextTest(int argc, char **argv)
{
    TherionSourceTextTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceTextTest.moc"
