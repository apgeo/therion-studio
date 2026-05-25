#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/TherionDocumentParser.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QComboBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>

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

void pumpEventsFast()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
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
    pumpEventsFast();
    return !view->scene()->selectedItems().isEmpty();
}

bool selectBlockByKind(QGraphicsView *view, QLabel *statusLabel, const QString &kindToken)
{
    if (view == nullptr || statusLabel == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> sceneItems = view->scene()->items(Qt::AscendingOrder);
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr || !(item->flags() & QGraphicsItem::ItemIsSelectable)) {
            continue;
        }

        const QRectF sceneRect = item->sceneBoundingRect();
        if (!sceneRect.isValid() || sceneRect.width() < 16.0 || sceneRect.height() < 8.0) {
            continue;
        }

        if (!selectBlockAtScenePoint(view, sceneRect.center())) {
            continue;
        }

        const QString statusText = statusLabel->text().trimmed().toLower();
        QString commandToken = statusText;
        const QString prefix = QStringLiteral("command:");
        if (commandToken.startsWith(prefix)) {
            commandToken = commandToken.mid(prefix.size()).trimmed();
        }
        if (commandToken == kindToken.trimmed().toLower()) {
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

QStringList readingsOrderTagTokens(QWidget *root)
{
    if (root == nullptr) {
        return {};
    }
    QWidget *tagEditor = root->findChild<QWidget *>(QStringLiteral("blockDetailsReadingsTagEditor"));
    if (tagEditor == nullptr) {
        return {};
    }
    QStringList tokens;
    const QList<QToolButton *> chips = tagEditor->findChildren<QToolButton *>();
    for (QToolButton *chip : chips) {
        if (chip == nullptr) {
            continue;
        }
        QString text = chip->text();
        const int crossIndex = text.lastIndexOf(QChar(0x2715)); // ✕
        if (crossIndex >= 0) {
            text = text.left(crossIndex);
        }
        text = text.trimmed();
        if (!text.isEmpty()) {
            tokens.append(text);
        }
    }
    return tokens;
}

void clearReadingsOrderTagTokens(QWidget *root)
{
    if (root == nullptr) {
        return;
    }
    QWidget *tagEditor = root->findChild<QWidget *>(QStringLiteral("blockDetailsReadingsTagEditor"));
    if (tagEditor == nullptr) {
        return;
    }

    int safetyCounter = 0;
    while (safetyCounter < 64) {
        ++safetyCounter;
        const QList<QToolButton *> chips = tagEditor->findChildren<QToolButton *>();
        if (chips.isEmpty()) {
            break;
        }
        QToolButton *chip = chips.first();
        if (chip == nullptr) {
            break;
        }
        chip->click();
        pumpEvents();
    }
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
    file.write("survey demo -title old # survey comment\ncenterline\n# caret-target line for typing test\nteam oldteam\nexplo-team old-discovery-team\ndata normal from to tape compass clino\n  1 2 3 4 5 6\nendcenterline\nendsurvey\n");
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
    auto *detailsHelp = tab.findChild<QTextEdit *>(QStringLiteral("blockDetailsHelpBrowser"));
    if (!expect(detailsHelp != nullptr, "Failed to find block details help browser.")) {
        return 1;
    }
    auto *toolboxList = tab.findChild<QListWidget *>();
    if (!expect(toolboxList != nullptr, "Failed to find block toolbox list.")) {
        return 1;
    }
    QComboBox *scopeCombo = nullptr;
    const QList<QComboBox *> combos = tab.findChildren<QComboBox *>();
    for (QComboBox *combo : combos) {
        if (combo == nullptr) {
            continue;
        }
        bool hasAutoScope = false;
        for (int row = 0; row < combo->count(); ++row) {
            if (combo->itemData(row).toString() == QStringLiteral("__auto__")) {
                hasAutoScope = true;
                break;
            }
        }
        if (hasAutoScope) {
            scopeCombo = combo;
            break;
        }
    }
    if (!expect(scopeCombo != nullptr, "Failed to find block toolbox scope combo.")) {
        return 1;
    }
    auto *primaryEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsPrimaryEdit"));
    auto *secondaryEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsSecondaryEdit"));
    auto *readingsTagInput = tab.findChild<QLineEdit *>(QStringLiteral("tokenTagEditorInput"));
    auto *commentEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsCommentEdit"));
    auto *primaryLabel = tab.findChild<QLabel *>(QStringLiteral("blockDetailsPrimaryLabel"));
    auto *secondaryLabel = tab.findChild<QLabel *>(QStringLiteral("blockDetailsSecondaryLabel"));
    auto *optionsLabel = tab.findChild<QLabel *>(QStringLiteral("blockDetailsOptionsLabel"));
    auto *optionsTable = tab.findChild<QTableWidget *>(QStringLiteral("blockDetailsOptionsTable"));
    auto *addOptionButton = tab.findChild<QPushButton *>(QStringLiteral("blockDetailsAddOptionButton"));
    auto *applyButton = tab.findChild<QPushButton *>(QStringLiteral("blockDetailsApplyButton"));
    if (!expect(primaryEdit != nullptr, "Failed to find blockDetailsPrimaryEdit.")) {
        return 1;
    }
    if (!expect(secondaryEdit != nullptr, "Failed to find blockDetailsSecondaryEdit.")) {
        return 1;
    }
    if (!expect(commentEdit != nullptr, "Failed to find blockDetailsCommentEdit.")) {
        return 1;
    }
    if (!expect(readingsTagInput != nullptr, "Failed to find tokenTagEditorInput.")) {
        return 1;
    }
    if (!expect(primaryLabel != nullptr, "Failed to find blockDetailsPrimaryLabel.")) {
        return 1;
    }
    if (!expect(secondaryLabel != nullptr, "Failed to find blockDetailsSecondaryLabel.")) {
        return 1;
    }
    if (!expect(optionsLabel != nullptr, "Failed to find blockDetailsOptionsLabel.")) {
        return 1;
    }
    if (!expect(optionsTable != nullptr, "Failed to find blockDetailsOptionsTable.")) {
        return 1;
    }
    if (!expect(addOptionButton != nullptr, "Failed to find blockDetails add option button.")) {
        return 1;
    }
    if (!expect(applyButton != nullptr, "Failed to find blockDetailsApplyButton.")) {
        return 1;
    }
    const QString blocksText = tab.text();
    if (!expect(blocksText.startsWith(QStringLiteral("encoding utf-8")),
                "Blocks mode should normalize source to `encoding utf-8` at line 1.")) {
        return 1;
    }
    int encodingDirectiveCount = 0;
    const QStringList blocksLines = blocksText.split(QLatin1Char('\n'));
    for (int index = 0; index < blocksLines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(blocksLines.at(index), index + 1);
        if (parsedLine.directive.trimmed().compare(QStringLiteral("encoding"), Qt::CaseInsensitive) == 0) {
            ++encodingDirectiveCount;
        }
    }
    if (!expect(encodingDirectiveCount == 1,
                "Blocks mode should keep exactly one `encoding` directive.")) {
        return 1;
    }

    if (!expect(findToolboxCommandItem(toolboxList, QStringLiteral("encoding")) == nullptr,
                "Toolbox should not expose `encoding` command templates.")) {
        return 1;
    }

    const int allScopeIndex = scopeCombo->findData(QStringLiteral("all"));
    if (!expect(allScopeIndex >= 0, "Failed to find `All` scope in toolbox scope selector.")) {
        return 1;
    }
    scopeCombo->setCurrentIndex(allScopeIndex);
    pumpEvents();

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
    const QString toolboxHelpText = detailsHelp->toPlainText().toLower();
    const QString toolboxHelpHtml = detailsHelp->toHtml().toLower();
    if (!expect(toolboxHelpText.trimmed().size() > 12,
                "Selecting toolbox command should show non-empty compact descriptive help.")) {
        return 1;
    }
    if (!expect(toolboxHelpText.contains(QStringLiteral("summary:")),
                "Selecting toolbox command should keep 'Summary:' label for consistency.")) {
        return 1;
    }
    if (!expect(!toolboxHelpText.contains(QStringLiteral("inspect help before drag/drop insertion")),
                "Toolbox command preview should not include introductory instruction sentence.")) {
        return 1;
    }
    if (!expect(!toolboxHelpHtml.contains(QStringLiteral("<h4>arguments</h4>")),
                "Toolbox command preview should not show full argument sections.")) {
        return 1;
    }
    if (!expect(!toolboxHelpHtml.contains(QStringLiteral("<h4>accepted values</h4>")),
                "Toolbox command preview should not show full accepted-values sections.")) {
        return 1;
    }
    if (!expect(!toolboxHelpHtml.contains(QStringLiteral("<h4>options</h4>")),
                "Toolbox command preview should not show full options sections.")) {
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
    if (!expect(!secondaryEdit->isVisible(),
                "Extra arguments field should stay hidden when no extra positional tokens exist.")) {
        return 1;
    }
    if (!expect(optionsLabel->isVisible(),
                "Options section should be visible for commands that support options.")) {
        return 1;
    }
    if (!expect(commentEdit->isVisible()
                    && commentEdit->text().trimmed().compare(QStringLiteral("survey comment"), Qt::CaseInsensitive) == 0,
                "Selected block should expose editable inline comment field with parsed comment text.")) {
        return 1;
    }
    const int surveyOptionRowCount = optionsTable->rowCount();
    addOptionButton->click();
    pumpEvents();
    if (!expect(optionsTable->rowCount() == surveyOptionRowCount + 1,
                "Add-option header button should append a new option row.")) {
        return 1;
    }
    QTableWidgetItem *newOptionItem = optionsTable->item(optionsTable->rowCount() - 1, 0);
    if (!expect(newOptionItem != nullptr && newOptionItem->text().trimmed().isEmpty(),
                "Add-option header button should keep new option key empty (no prepopulated placeholder).")) {
        return 1;
    }
    optionsTable->removeRow(optionsTable->rowCount() - 1);
    pumpEvents();
    if (!expect(!detailsHelp->toPlainText().toLower().contains(QStringLiteral("option: |title")),
                "Selecting a block should show command/parameter help by default, not sticky option-row help.")) {
        return 1;
    }
    optionsTable->setFocus();
    optionsTable->setCurrentCell(0, 0);
    pumpEvents();
    if (!expect(!detailsHelp->toPlainText().toLower().contains(QStringLiteral("option: |title")),
                "Focusing an option row should keep parent command/parameter help visible.")) {
        return 1;
    }
    blockView->setFocus();
    pumpEvents();
    if (!expect(!detailsHelp->toPlainText().toLower().contains(QStringLiteral("option: |title")),
                "Leaving option-table focus should restore command/parameter help.")) {
        return 1;
    }
    if (!expect(detailsStatus->text().toLower().contains(QStringLiteral("command: survey"))
                    && !detailsStatus->text().toLower().contains(QStringLiteral("line ")),
                "Block Details status should show command-only context without line number.")) {
        return 1;
    }
    if (!expect(!detailsHelp->toHtml().toLower().contains(QStringLiteral("<strong>syntax:</strong>")),
                "Block details contextual help should hide Syntax section in Blocks mode.")) {
        return 1;
    }

    primaryEdit->setText(QStringLiteral("demo2"));
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("survey demo2 -title old")),
                "Applying details for survey block should update survey id.")) {
        return 1;
    }
    if (!expect(editor->toPlainText().contains(QStringLiteral("# survey comment")),
                "Applying block details should preserve existing inline comment.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("team")),
                "Failed to select team block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryEdit->isVisible() && secondaryEdit->isVisible(),
                "Team block should expose separate Person and Roles fields.")) {
        return 1;
    }
    if (!expect(primaryLabel->text().trimmed().compare(QStringLiteral("Person"), Qt::CaseInsensitive) == 0,
                "Team block primary field should be labeled Person.")) {
        return 1;
    }
    if (!expect(secondaryLabel->text().trimmed().compare(QStringLiteral("Roles"), Qt::CaseInsensitive) == 0,
                "Team block secondary field should be labeled Roles.")) {
        return 1;
    }
    if (!expect(!optionsLabel->isVisible(),
                "Options section should stay hidden for commands without options support.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("ZO /CSS 6-28"));
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("team \"ZO /CSS 6-28\"")),
                "Applying details for team block should quote spaced team value.")) {
        return 1;
    }
    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("team")),
                "Failed to reselect team block after apply.")) {
        return 1;
    }
    if (!expect(secondaryEdit->text().trimmed().isEmpty(),
                QStringLiteral("Team block without roles should parse empty Roles field (actual: `%1`).")
                    .arg(secondaryEdit->text())
                    .toUtf8()
                    .constData())) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("explo-team")),
                "Failed to select explo-team block in blocks view.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("Babicka Speleo Group"));
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("explo-team \"Babicka Speleo Group\"")),
                "Applying details for explo-team block should quote spaced person value.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("data")),
                "Failed to select data block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryLabel->text().trimmed().compare(QStringLiteral("Style"), Qt::CaseInsensitive) == 0,
                "Data block primary field should be labeled Style.")) {
        return 1;
    }
    if (!expect(secondaryLabel->isVisible()
                    && secondaryLabel->text().trimmed().compare(QStringLiteral("Readings Order"), Qt::CaseInsensitive) == 0,
                "Data block secondary field should be visible and labeled Readings Order.")) {
        return 1;
    }
    if (!expect(readingsOrderTagTokens(&tab)
                    == QStringList({QStringLiteral("from"),
                                    QStringLiteral("to"),
                                    QStringLiteral("tape"),
                                    QStringLiteral("compass"),
                                    QStringLiteral("clino")}),
                "Data block should parse existing readings order into tag chips.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("normal"));
    clearReadingsOrderTagTokens(&tab);
    readingsTagInput->setFocus();
    readingsTagInput->setText(QStringLiteral("from to length compass clino"));
    sendKey(readingsTagInput, QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"));
    sendKey(readingsTagInput, QEvent::KeyRelease, Qt::Key_Return);
    pumpEvents();
    applyButton->click();
    pumpEvents();
    if (!expect(editor->toPlainText().contains(QStringLiteral("data normal from to length compass clino")),
                "Applying details for data block should update style + readings order.")) {
        return 1;
    }
    if (!expect(editor->toPlainText().contains(QStringLiteral("  1 2 3 4 5 6")),
                "Applying data header edit should preserve existing data rows.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("centerline")),
                "Failed to select centerline block in blocks view.")) {
        return 1;
    }
    const int centerlineOptionRows = optionsTable->rowCount();
    addOptionButton->click();
    pumpEvents();
    if (!expect(optionsTable->rowCount() == centerlineOptionRows + 1,
                "Centerline Add Option should append a new option row.")) {
        return 1;
    }
    const int newRow = optionsTable->rowCount() - 1;
    optionsTable->setItem(newRow, 0, new QTableWidgetItem(QStringLiteral("-walls")));
    optionsTable->setItem(newRow, 1, new QTableWidgetItem(QStringLiteral("invalid-value")));
    pumpEvents();
    optionsTable->removeRow(newRow);
    pumpEvents();

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
            if (!expect(!crashGuardTab->isDirty(),
                        "Switching to Blocks mode must not mark a clean document as modified.")) {
                delete crashGuardTab;
                return 1;
            }
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

    const QString configPath = tempDir.path() + QStringLiteral("/thconfig");
    QFile configFile(configPath);
    if (!expect(configFile.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create thconfig test file.")) {
        return 1;
    }
    configFile.write("encoding utf-8\nsource index.th\nselect 1302.m@1302\nexport map -output _output/1302_plan.pdf -projection plan\n");
    configFile.close();

    {
        auto *configTab = new TextEditorTab;
        if (!expect(configTab->loadFile(configPath, &errorMessage),
                    "Failed to load thconfig test file in TextEditorTab.")) {
            std::cerr << errorMessage.toStdString() << '\n';
            delete configTab;
            return 1;
        }
        configTab->resize(960, 640);
        configTab->show();
        pumpEvents();

        QPushButton *configBlocksButton = findButtonByText(configTab, QStringLiteral("Blocks"));
        if (!expect(configBlocksButton != nullptr, "Failed to find Blocks mode button for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        configBlocksButton->click();
        pumpEvents();

        auto *configView = configTab->findChild<QGraphicsView *>();
        auto *configStatus = configTab->findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
        auto *configToolboxList = configTab->findChild<QListWidget *>();
        QComboBox *configScopeCombo = nullptr;
        const QList<QComboBox *> configCombos = configTab->findChildren<QComboBox *>();
        for (QComboBox *combo : configCombos) {
            if (combo == nullptr) {
                continue;
            }
            bool hasAutoScope = false;
            for (int row = 0; row < combo->count(); ++row) {
                if (combo->itemData(row).toString() == QStringLiteral("__auto__")) {
                    hasAutoScope = true;
                    break;
                }
            }
            if (hasAutoScope) {
                configScopeCombo = combo;
                break;
            }
        }
        if (!expect(configView != nullptr, "Failed to find block canvas view for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        if (!expect(configStatus != nullptr, "Failed to find block details status for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        if (!expect(configToolboxList != nullptr, "Failed to find block toolbox list for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        if (!expect(configScopeCombo != nullptr, "Failed to find scope combo for thconfig tab.")) {
            delete configTab;
            return 1;
        }

        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("select")) != nullptr,
                    "Top-level toolbox commands for thconfig should include `select`.")) {
            delete configTab;
            return 1;
        }
        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("export")) != nullptr,
                    "Top-level toolbox commands for thconfig should include `export`.")) {
            delete configTab;
            return 1;
        }

        if (!expect(selectBlockByKind(configView, configStatus, QStringLiteral("select")),
                    "Blocks view should render top-level `select` command cards for thconfig files.")) {
            delete configTab;
            return 1;
        }
        if (!expect(selectBlockByKind(configView, configStatus, QStringLiteral("export")),
                    "Blocks view should render top-level `export` command cards for thconfig files.")) {
            delete configTab;
            return 1;
        }
        if (!expect(selectBlockByKind(configView, configStatus, QStringLiteral("source")),
                    "Blocks view should render `source` command card for thconfig files.")) {
            delete configTab;
            return 1;
        }
        configScopeCombo->setCurrentIndex(configScopeCombo->findData(QStringLiteral("__auto__")));
        pumpEvents();
        const QString autoScopeTooltip = configScopeCombo->toolTip().toLower();
        if (!expect(autoScopeTooltip.contains(QStringLiteral("top-level")),
                    "Inline `source file` should behave as leaf (Auto scope should resolve to top-level when selected).")) {
            delete configTab;
            return 1;
        }

        delete configTab;
        pumpEvents();
    }

    return 0;
}
