#include "../src/app/text_editor/ContextHelpController.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionDocumentParser.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextLayout>

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

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void sendKey(QWidget *widget,
             QEvent::Type type,
             int key,
             Qt::KeyboardModifiers modifiers = Qt::NoModifier,
             const QString &text = QString())
{
    if (widget == nullptr) {
        return;
    }

    QKeyEvent event(type, key, modifiers, text);
    QCoreApplication::sendEvent(widget, &event);
}

void triggerManualCompletion(QPlainTextEdit *editor)
{
    sendKey(editor, QEvent::KeyPress, Qt::Key_Space, Qt::ControlModifier, QStringLiteral(" "));
    sendKey(editor, QEvent::KeyRelease, Qt::Key_Space, Qt::ControlModifier);
    pumpEvents();
}

bool modelContainsToken(QCompleter *completer, const QString &token)
{
    if (completer == nullptr || completer->completionModel() == nullptr) {
        return false;
    }

    for (int row = 0; row < completer->completionModel()->rowCount(); ++row) {
        const QString candidate = completer->completionModel()->index(row, 0).data(Qt::DisplayRole).toString();
        if (candidate.compare(token, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QStringList completionTokens(QCompleter *completer)
{
    QStringList tokens;
    if (completer == nullptr || completer->completionModel() == nullptr) {
        return tokens;
    }

    for (int row = 0; row < completer->completionModel()->rowCount(); ++row) {
        tokens.append(completer->completionModel()->index(row, 0).data(Qt::DisplayRole).toString());
    }
    return tokens;
}

bool tokenHasWaveUnderline(const QTextBlock &block, const QString &token)
{
    if (!block.isValid() || block.layout() == nullptr) {
        return false;
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return false;
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())) {
            if (range.format.underlineStyle() == QTextCharFormat::WaveUnderline) {
                return true;
            }
        }
    }
    return false;
}

bool tokenHasBoldFormat(const QTextBlock &block, const QString &token)
{
    if (!block.isValid() || block.layout() == nullptr) {
        return false;
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return false;
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())) {
            if (range.format.fontWeight() >= QFont::Bold
                && range.format.underlineStyle() != QTextCharFormat::WaveUnderline) {
                return true;
            }
        }
    }
    return false;
}

QColor tokenForegroundColor(const QTextBlock &block, const QString &token, bool *found)
{
    if (found != nullptr) {
        *found = false;
    }
    if (!block.isValid() || block.layout() == nullptr) {
        return {};
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return {};
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())) {
            if (found != nullptr) {
                *found = true;
            }
            return range.format.foreground().color();
        }
    }
    return {};
}

QColor expectedIdentifierColor()
{
    const QColor baseColor = QGuiApplication::palette().color(QPalette::Base);
    const bool darkPalette = baseColor.isValid() && baseColor.lightnessF() < 0.5;
    return darkPalette ? QColor(QStringLiteral("#DCDCAA")) : QColor(QStringLiteral("#8250df"));
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QtFileSystem fileSystem;

    {
        const QString helpHtml = ContextHelpController::renderHelpHtml(QStringLiteral("survey"),
                                                                       QStringLiteral("Survey summary"),
                                                                       QStringLiteral("survey <id> [OPTIONS]"),
                                                                       {QStringLiteral("<id> = survey identifier")},
                                                                       {},
                                                                       {QStringLiteral("-title <text> = survey title")},
                                                                       true);
        if (!expect(helpHtml.contains(QStringLiteral("<code"))
                        && helpHtml.contains(QStringLiteral("<li"))
                        && helpHtml.contains(QStringLiteral("<b><tt"))
                        && helpHtml.contains(QStringLiteral("&lt;id&gt;"))
                        && helpHtml.contains(QStringLiteral("-title &lt;text&gt;")),
                    "Contextual help should render argument and option signatures as compact bold monospace bullet items.")) {
            return 1;
        }
    }

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    {
        QDir rootDir(tempDir.path());
        if (!expect(rootDir.mkpath(QStringLiteral("subdir")), "Failed to create subdir fixture.")) {
            return 1;
        }
        QFile nestedFixture(tempDir.path() + QStringLiteral("/subdir/child.th2"));
        if (!expect(nestedFixture.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create nested fixture file.")) {
            return 1;
        }
        nestedFixture.write("scrap s1 -projection plan\nendscrap\n");
        nestedFixture.close();

        QFile nestedCurrentFixture(tempDir.path() + QStringLiteral("/subdir/current.th"));
        if (!expect(nestedCurrentFixture.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create nested current fixture file.")) {
            return 1;
        }
        nestedCurrentFixture.write("input \n");
        nestedCurrentFixture.close();
    }

    const QString filePath = tempDir.path() + QStringLiteral("/completion-highlight-test.th");
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create test file.")) {
        return 1;
    }
    file.write("survey demo\nendsurvey\n");
    file.close();

    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    QString errorMessage;
    if (!expect(tab.loadFile(filePath, &errorMessage), "Failed to load test file in TextEditorTab.")) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }
    tab.setProjectRootPath(tempDir.path());

    tab.resize(960, 640);
    tab.show();
    pumpEvents();

    auto *editor = tab.findChild<QPlainTextEdit *>();
    if (!expect(editor != nullptr, "Failed to find raw text editor widget.")) {
        return 1;
    }
    auto *completer = tab.findChild<QCompleter *>();
    if (!expect(completer != nullptr, "Failed to find completion widget.")) {
        return 1;
    }
    auto *helpBrowser = tab.findChild<QTextBrowser *>(QStringLiteral("rawContextHelpBrowser"));
    if (!expect(helpBrowser != nullptr, "Failed to find contextual help widget.")) {
        return 1;
    }
    if (!expect(completer->popup() != nullptr, "Failed to find completion popup.")) {
        return 1;
    }

    editor->setPlainText(QStringLiteral("survey \nendsurvey\n"));
    pumpEvents();

    {
        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasBoldFormat(firstLine, QStringLiteral("survey")),
                    "Opening directive survey should be highlighted as structural keyword.")) {
            return 1;
        }
    }

    {
        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (completer->popup()->isVisible()) {
            std::cerr << "Unexpected completion list: "
                      << completionTokens(completer).join(QStringLiteral(", ")).toStdString()
                      << '\n';
        }
        if (!expect(!completer->popup()->isVisible(),
                    "Completion popup should stay hidden while required survey id argument is missing.")) {
            return 1;
        }
    }

    const QColor expectedIdColor = expectedIdentifierColor();
    {
        editor->setPlainText(QStringLiteral("survey demo bogus\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        bool idFound = false;
        bool plainFound = false;
        const QColor idColor = tokenForegroundColor(firstLine, QStringLiteral("demo"), &idFound);
        const QColor plainColor = tokenForegroundColor(firstLine, QStringLiteral("bogus"), &plainFound);
        if (!expect(idFound && plainFound,
                    "Failed to resolve survey id/plain token formatting for syntax highlighting validation.")) {
            return 1;
        }
        if (!expect(idColor == expectedIdColor,
                    "Survey positional id token should use identifier highlighting color.")) {
            return 1;
        }
        if (!expect(idColor != plainColor,
                    "Survey positional id token should be distinct from plain base-text token color.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -id line-8 -close on\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        bool idFound = false;
        const QColor idColor = tokenForegroundColor(firstLine, QStringLiteral("line-8"), &idFound);
        if (!expect(idFound, "Failed to resolve -id option value formatting.")) {
            return 1;
        }
        if (!expect(idColor == expectedIdColor,
                    "Value of -id option should use identifier highlighting color.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\nendsurvey demo\n"));
        pumpEvents();

        const QTextBlock closingLine = editor->document()->findBlockByLineNumber(1);
        bool idFound = false;
        const QColor idColor = tokenForegroundColor(closingLine, QStringLiteral("demo"), &idFound);
        if (!expect(idFound, "Failed to resolve closing directive id formatting.")) {
            return 1;
        }
        if (!expect(idColor == expectedIdColor,
                    "Optional closing directive id should use identifier highlighting color.")) {
            return 1;
        }
    }

    {
        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        sendKey(editor, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_Space);
        sendKey(editor, QEvent::KeyPress, Qt::Key_D, Qt::NoModifier, QStringLiteral("d"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_D);
        sendKey(editor, QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, QStringLiteral("e"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_E);
        sendKey(editor, QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, QStringLiteral("m"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_M);
        sendKey(editor, QEvent::KeyPress, Qt::Key_O, Qt::NoModifier, QStringLiteral("o"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_O);
        sendKey(editor, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_Space);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear after required survey id argument is provided.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("-title")),
                    "Survey options should include -title once required id is present.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo -titlle \"x\"\nendsurvey\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("-titlle")),
                    "Unknown option token should be marked with invalid highlighting (wave underline).")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line rock-border -close on -clip off \"-clip off\" \"-clip off\"\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("\"-clip off\"")),
                    "Quoted option/value tokens such as \"-clip off\" should be marked with invalid highlighting.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line label -text \"-clip off\"\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(!tokenHasWaveUnderline(firstLine, QStringLiteral("\"-clip off\"")),
                    "Quoted text values that start with '-' should remain valid option values.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -close maybe\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("maybe")),
                    "Invalid enum value for -close should be marked with invalid highlighting.")) {
            return 1;
        }

        const int maybePos = firstLine.text().indexOf(QStringLiteral("maybe"));
        if (!expect(maybePos >= 0, "Failed to locate invalid enum token for help validation.")) {
            return 1;
        }
        QTextCursor cursor(firstLine);
        cursor.setPosition(firstLine.position() + maybePos + 1);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        const QString tooltipText = editor->toolTip();
        if (!expect((helpText.contains(QStringLiteral("not allowed"), Qt::CaseInsensitive)
                         && helpText.contains(QStringLiteral("Allowed Values"), Qt::CaseInsensitive))
                        || tooltipText.contains(QStringLiteral("Allowed:"), Qt::CaseInsensitive),
                    "Validation guidance should surface in either contextual help or inline tooltip for invalid token.")) {
            return 1;
        }
        const bool allowedValuesInHelp = helpText.contains(QStringLiteral("on"), Qt::CaseInsensitive)
            && helpText.contains(QStringLiteral("off"), Qt::CaseInsensitive)
            && helpText.contains(QStringLiteral("auto"), Qt::CaseInsensitive);
        const bool allowedValuesInTooltip = tooltipText.contains(QStringLiteral("on"), Qt::CaseInsensitive)
            && tooltipText.contains(QStringLiteral("off"), Qt::CaseInsensitive)
            && tooltipText.contains(QStringLiteral("auto"), Qt::CaseInsensitive);
        if (!expect(allowedValuesInHelp || allowedValuesInTooltip,
                    "Allowed enum values should surface in either contextual help or inline tooltip for invalid option value.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("export map -o map.pdf -layout moj\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(!tokenHasWaveUnderline(firstLine, QStringLiteral("-o")),
                    "Known option aliases such as export -o should not be marked with invalid highlighting.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -close on\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(!tokenHasWaveUnderline(firstLine, QStringLiteral("on")),
                    "Valid enum value for -close should not be marked invalid.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -subtype cave\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("cave")),
                    "Subtype value incompatible with current line type should be marked invalid.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -context point \n"));
        pumpEvents();

        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for multi-value -context option after first value.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("line")),
                    "Multi-value -context completion should suggest remaining enum values.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("-close")),
                    "Multi-value value-context completion should not fall back to unrelated option tokens.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("point")),
                    "Multi-value completion should suppress already used values in the same option group.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  \nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock commandLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(commandLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear on blank command slot inside centerline block.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("data")),
                    "Centerline command-position completion should include inline command 'data'.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("date")),
                    "Centerline command-position completion should include inline command 'date'.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("what"))
                    && !modelContainsToken(completer, QStringLiteral("x"))
                    && !modelContainsToken(completer, QStringLiteral("y"))
                    && !modelContainsToken(completer, QStringLiteral("z"))
                    && !modelContainsToken(completer, QStringLiteral("zero")),
                    "Centerline command-position completion should not include argument-placeholder noise tokens.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  data \nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock dataLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(dataLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear after 'data ' inside centerline.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("normal")),
                    "Data value completion should include style token 'normal'.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("from"))
                    && modelContainsToken(completer, QStringLiteral("to")),
                    "Data value completion should include reading tokens 'from' and 'to'.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("length"))
                    && modelContainsToken(completer, QStringLiteral("compass"))
                    && modelContainsToken(completer, QStringLiteral("clino")),
                    "Data value completion should include reading tokens like length/compass/clino.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("-title")),
                    "Data value completion should not fall back to unrelated option tokens.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  data normal f\nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock dataLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(dataLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for partial data reading token prefix.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("from")),
                    "Data value completion should keep matching reading token 'from' for prefix 'f'.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  infer \nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock inferLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(inferLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear after 'infer ' inside centerline.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("on"))
                    && modelContainsToken(completer, QStringLiteral("off")),
                    "Infer value completion should include on/off tokens from centerline metadata.")) {
            return 1;
        }

        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        if (!expect(!helpText.trimmed().isEmpty(),
                    "Contextual help should remain non-empty when cursor is on infer line.")) {
            return 1;
        }
        if (!expect(!helpText.contains(QStringLiteral("Related Keywords"), Qt::CaseInsensitive),
                    "Contextual help should not render Related Keywords section by default.")) {
            return 1;
        }

        const QTextBlock exploDateLine = editor->document()->findBlockByLineNumber(2);
        if (!expect(tokenHasBoldFormat(exploDateLine, QStringLiteral("infer")),
                    "Centerline inline command token should be highlighted as keyword.")) {
            return 1;
        }

        const QTextBlock endCenterlineLine = editor->document()->findBlockByLineNumber(3);
        if (!expect(tokenHasBoldFormat(endCenterlineLine, QStringLiteral("endcenterline")),
                    "Closing directive endcenterline should be highlighted as a structural keyword.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncentreline\nendcentreline\nendsurvey\n"));
        pumpEvents();

        const QTextBlock endCentrelineLine = editor->document()->findBlockByLineNumber(2);
        if (!expect(tokenHasBoldFormat(endCentrelineLine, QStringLiteral("endcentreline")),
                    "Alias closing directive endcentreline should be highlighted as structural keyword.")) {
            return 1;
        }

        QTextCursor cursor(endCentrelineLine);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        if (!expect(!helpText.trimmed().isEmpty(),
                    "Contextual help should remain non-empty when cursor is on endcentreline.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\nscrap s1 -projection plan\nline wall\nendline\narea water\nendarea\nendscrap\nmap m1\nendmap\nendsurvey\n"));
        pumpEvents();

        const QTextBlock endLineLine = editor->document()->findBlockByLineNumber(3);
        if (!expect(tokenHasBoldFormat(endLineLine, QStringLiteral("endline")),
                    "Closing directive endline should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endAreaLine = editor->document()->findBlockByLineNumber(5);
        if (!expect(tokenHasBoldFormat(endAreaLine, QStringLiteral("endarea")),
                    "Closing directive endarea should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endScrapLine = editor->document()->findBlockByLineNumber(6);
        if (!expect(tokenHasBoldFormat(endScrapLine, QStringLiteral("endscrap")),
                    "Closing directive endscrap should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endMapLine = editor->document()->findBlockByLineNumber(8);
        if (!expect(tokenHasBoldFormat(endMapLine, QStringLiteral("endmap")),
                    "Closing directive endmap should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endSurveyLine = editor->document()->findBlockByLineNumber(9);
        if (!expect(tokenHasBoldFormat(endSurveyLine, QStringLiteral("endsurvey")),
                    "Closing directive endsurvey should be highlighted as structural keyword.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("input \n"));
        pumpEvents();

        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(!completer->popup()->isVisible(),
                    "Completion popup should stay hidden for input until ./ or ../ prefix is typed.")) {
            return 1;
        }

        const QString nestedCurrentPath = tempDir.path() + QStringLiteral("/subdir/current.th");
        if (!expect(tab.loadFile(nestedCurrentPath, &errorMessage), "Failed to load nested current file for relative-path completion test.")) {
            std::cerr << errorMessage.toStdString() << '\n';
            return 1;
        }
        tab.setProjectRootPath(tempDir.path());
        pumpEvents();

        firstLine = editor->document()->findBlockByLineNumber(0);
        cursor = QTextCursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(!completer->popup()->isVisible(),
                    "Completion popup should stay hidden in nested file until ./ or ../ prefix is typed.")) {
            return 1;
        }

        editor->setPlainText(QStringLiteral("input ./\n"));
        pumpEvents();

        firstLine = editor->document()->findBlockByLineNumber(0);
        cursor = QTextCursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for input ./ prefix in nested file.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("child.th2")),
                    "Input completion with ./ prefix should include same-directory targets.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("../completion-highlight-test.th")),
                    "Input completion with ./ prefix should not include parent-directory candidates.")) {
            return 1;
        }

        editor->setPlainText(QStringLiteral("input ../\n"));
        pumpEvents();

        firstLine = editor->document()->findBlockByLineNumber(0);
        cursor = QTextCursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for input ../ prefix in nested file.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("../completion-highlight-test.th")),
                    "Input completion with ../ prefix should include parent-directory candidates.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("scrap clopy01 -projection plan -author 1985.04.29 \"Hugo Havel\" -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        bool firstFound = false;
        bool referenceFound = false;
        const QColor firstColor = tokenForegroundColor(firstLine, QStringLiteral("-128"), &firstFound);
        const QColor referenceColor = tokenForegroundColor(firstLine, QStringLiteral("851"), &referenceFound);
        if (!expect(firstFound && referenceFound,
                    "Failed to resolve numeric token formatting in -scale array.")) {
            return 1;
        }
        if (!expect(firstColor == referenceColor,
                    "Bracket-attached numeric token in -scale array should be highlighted as number.")) {
            return 1;
        }
    }

    return 0;
}
