#include "../src/app/TextEditorTab.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>

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

void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &localPos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    if (widget == nullptr) {
        return;
    }

    const QPoint globalPos = widget->mapToGlobal(localPos);
    QMouseEvent event(type,
                      QPointF(localPos),
                      QPointF(globalPos),
                      button,
                      buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
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

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = tempDir.path() + QStringLiteral("/caret-test.th");
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create test file.")) {
        return 1;
    }
    file.write("survey demo\ncenterline\nstation 1\nendsurvey\n");
    file.close();

    TextEditorTab tab;
    QString errorMessage;
    if (!expect(tab.loadFile(filePath, &errorMessage), "Failed to load test file in TextEditorTab.")) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }

    tab.resize(960, 640);
    tab.show();
    pumpEvents();

    auto *editor = tab.findChild<QPlainTextEdit *>();
    if (!expect(editor != nullptr, "Failed to find raw text editor widget.")) {
        return 1;
    }

    QTextBlock thirdLine = editor->document()->findBlockByLineNumber(2);
    if (!expect(thirdLine.isValid(), "Failed to resolve third line in editor document.")) {
        return 1;
    }

    QTextCursor targetCursor(thirdLine);
    targetCursor.movePosition(QTextCursor::StartOfBlock);
    editor->setTextCursor(targetCursor);
    pumpEvents();

    const QPoint clickPos = editor->cursorRect().center() + QPoint(45, 0);

    QTextCursor firstLineCursor(editor->document()->findBlockByLineNumber(0));
    firstLineCursor.movePosition(QTextCursor::StartOfBlock);
    editor->setTextCursor(firstLineCursor);
    editor->setFocus(Qt::OtherFocusReason);
    pumpEvents();

    QWidget *viewport = editor->viewport();
    sendMouse(viewport, QEvent::MouseButtonPress, clickPos, Qt::LeftButton, Qt::LeftButton);
    sendMouse(viewport, QEvent::MouseButtonRelease, clickPos, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    const QTextCursor clickedCursor = editor->textCursor();
    if (!expect(clickedCursor.blockNumber() == 2, "Caret click did not move cursor to the expected line.")) {
        return 1;
    }
    if (!expect(clickedCursor.positionInBlock() > 0, "Caret click did not place cursor inside the line content.")) {
        return 1;
    }

    const int beforeLength = editor->toPlainText().size();
    sendKey(editor, QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
    sendKey(editor, QEvent::KeyRelease, Qt::Key_X);
    pumpEvents();

    if (!expect(editor->toPlainText().size() == beforeLength + 1, "Typing did not insert text at caret.")) {
        return 1;
    }
    if (!expect(editor->textCursor().blockNumber() == 2, "Typing after click moved cursor away from target line.")) {
        return 1;
    }

    const QString beforeTabText = editor->toPlainText();
    const int beforeTabPos = editor->textCursor().position();
    sendKey(editor, QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
    sendKey(editor, QEvent::KeyRelease, Qt::Key_Tab);
    pumpEvents();

    const QString afterTabText = editor->toPlainText();
    if (!expect(afterTabText.size() == beforeTabText.size() + 4,
                "Tab should insert exactly four spaces.")) {
        return 1;
    }
    if (!expect(afterTabText.mid(beforeTabPos, 4) == QStringLiteral("    "),
                "Tab insertion should add four space characters at the caret.")) {
        return 1;
    }
    if (!expect(!afterTabText.contains(QLatin1Char('\t')),
                "Document should not contain tab characters after Tab insertion.")) {
        return 1;
    }

    return 0;
}
