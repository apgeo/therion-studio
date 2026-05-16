#include "../src/app/TextEditorTab.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>

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

QPushButton *findButtonByText(QWidget *root, const QString &text)
{
    if (root == nullptr) {
        return nullptr;
    }
    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button != nullptr && button->text().trimmed() == text) {
            return button;
        }
    }
    return nullptr;
}

bool selectBlockAtScenePoint(QGraphicsView *view, const QPointF &scenePoint)
{
    if (view == nullptr || view->scene() == nullptr) {
        return false;
    }

    const QPoint viewportPoint = view->mapFromScene(scenePoint);
    QWidget *viewport = view->viewport();
    if (viewport == nullptr) {
        return false;
    }

    sendMouse(viewport, QEvent::MouseButtonPress, viewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(viewport, QEvent::MouseButtonRelease, viewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    return !view->scene()->selectedItems().isEmpty();
}

bool selectBlockByKind(QGraphicsView *view, QLabel *statusLabel, const QString &kindToken)
{
    if (view == nullptr || statusLabel == nullptr) {
        return false;
    }

    for (int y = 24; y <= 360; y += 22) {
        if (!selectBlockAtScenePoint(view, QPointF(84.0, y))) {
            continue;
        }
        const QString statusText = statusLabel->text().toLower();
        if (statusText.contains(kindToken.toLower())) {
            return true;
        }
    }
    return false;
}

QListWidgetItem *findToolboxCommandItem(QListWidget *toolboxList, const QString &commandToken)
{
    if (toolboxList == nullptr) {
        return nullptr;
    }
    const QString normalizedToken = commandToken.trimmed().toLower();
    for (int row = 0; row < toolboxList->count(); ++row) {
        QListWidgetItem *item = toolboxList->item(row);
        if (item == nullptr) {
            continue;
        }
        if (item->data(Qt::UserRole).toString().trimmed().toLower() == normalizedToken) {
            return item;
        }
    }
    return nullptr;
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
    file.write("survey demo -title old\ncenterline\nteam oldteam\ndata normal from to tape compass clino\n  1 2 3 4 5 6\nendcenterline\nendsurvey\n");
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

    QPushButton *blocksModeButton = findButtonByText(&tab, QStringLiteral("Blocks"));
    if (!expect(blocksModeButton != nullptr, "Failed to find Blocks mode button.")) {
        return 1;
    }
    blocksModeButton->click();
    pumpEvents();

    auto *blockView = tab.findChild<QGraphicsView *>();
    if (!expect(blockView != nullptr, "Failed to find block canvas view.")) {
        return 1;
    }
    auto *detailsStatus = tab.findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
    if (!expect(detailsStatus != nullptr, "Failed to find block details status label.")) {
        return 1;
    }
    auto *detailsHelp = tab.findChild<QTextEdit *>();
    if (!expect(detailsHelp != nullptr, "Failed to find block details help browser.")) {
        return 1;
    }
    auto *toolboxList = tab.findChild<QListWidget *>();
    if (!expect(toolboxList != nullptr, "Failed to find block toolbox list.")) {
        return 1;
    }
    auto *primaryEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsPrimaryEdit"));
    auto *applyButton = tab.findChild<QPushButton *>(QStringLiteral("blockDetailsApplyButton"));
    if (!expect(primaryEdit != nullptr && applyButton != nullptr,
                "Failed to find block details primary edit/apply button.")) {
        return 1;
    }

    QListWidgetItem *toolboxDataItem = findToolboxCommandItem(toolboxList, QStringLiteral("data"));
    if (!expect(toolboxDataItem != nullptr, "Failed to find `data` command in block toolbox list.")) {
        return 1;
    }
    toolboxList->setCurrentItem(toolboxDataItem);
    pumpEvents();
    if (!expect(!primaryEdit->isVisible(),
                "Selecting toolbox command should hide editable Block Details section.")) {
        return 1;
    }
    if (!expect(detailsHelp->toPlainText().toLower().contains(QStringLiteral("syntax")),
                "Selecting toolbox command should show command contextual help in third column.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("survey")),
                "Failed to select survey block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryEdit->isVisible(),
                "Selecting a canvas block should show editable Block Details section.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("demo2"));
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("survey demo2 -title old")),
                "Applying details for survey block should update survey id.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("team")),
                "Failed to select team block in blocks view.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("newteam"));
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("team newteam")),
                "Applying details for team block should update team value.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("data")),
                "Failed to select data block in blocks view.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("normal from to length compass clino"));
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("data normal from to length compass clino")),
                "Applying details for data block should update data header columns.")) {
        return 1;
    }
    if (!expect(editor->toPlainText().contains(QStringLiteral("  1 2 3 4 5 6")),
                "Applying data header edit should preserve existing data rows.")) {
        return 1;
    }

    {
        auto *crashGuardTab = new TextEditorTab;
        if (!expect(crashGuardTab->loadFile(filePath, &errorMessage),
                    "Failed to load crash-guard tab instance.")) {
            delete crashGuardTab;
            return 1;
        }
        crashGuardTab->resize(960, 640);
        crashGuardTab->show();
        pumpEvents();

        QPushButton *guardBlocksButton = findButtonByText(crashGuardTab, QStringLiteral("Blocks"));
        if (guardBlocksButton != nullptr) {
            guardBlocksButton->click();
            pumpEvents();
        }
        auto *guardView = crashGuardTab->findChild<QGraphicsView *>();
        auto *guardStatus = crashGuardTab->findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
        if (guardView != nullptr) {
            selectBlockByKind(guardView, guardStatus, QStringLiteral("survey"));
        }
        auto *guardPrimaryEdit = crashGuardTab->findChild<QLineEdit *>(QStringLiteral("blockDetailsPrimaryEdit"));
        if (guardPrimaryEdit != nullptr) {
            guardPrimaryEdit->setText(QStringLiteral("discard-change"));
        }
        delete crashGuardTab;
        pumpEvents();
    }

    return 0;
}
