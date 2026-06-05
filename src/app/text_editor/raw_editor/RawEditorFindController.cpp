#include "RawEditorFindController.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCoreApplication>
#include <QTextCursor>

namespace TherionStudio
{
RawEditorFindController::RawEditorFindController(RawEditorFindContext context)
    : context_(std::move(context))
{
}

void RawEditorFindController::showFindBar(bool replaceMode)
{
    if (context_.editor == nullptr || context_.searchBar == nullptr || context_.findEdit == nullptr) {
        return;
    }

    if (context_.blocksModeActive != nullptr
        && *context_.blocksModeActive
        && context_.setBlocksModeActive) {
        context_.setBlocksModeActive(false);
    }

    updateSearchVisibility(replaceMode);
    context_.searchBar->setVisible(true);

    if (context_.findEdit->text().isEmpty() && !context_.editor->textCursor().selectedText().isEmpty()) {
        context_.findEdit->setText(context_.editor->textCursor().selectedText());
    }

    context_.findEdit->setFocus();
    context_.findEdit->selectAll();
    updateSearchResults(trText("Ready"));
}

void RawEditorFindController::showFindBarWithText(const QString &findText,
                                                  bool replaceMode,
                                                  bool wholeWord,
                                                  bool matchCase)
{
    showFindBar(replaceMode);
    if (context_.findEdit == nullptr) {
        return;
    }

    context_.findEdit->setText(findText);
    if (context_.wholeWordCheck != nullptr) {
        context_.wholeWordCheck->setChecked(wholeWord);
    }
    if (context_.caseSensitiveCheck != nullptr) {
        context_.caseSensitiveCheck->setChecked(matchCase);
    }
    context_.findEdit->selectAll();
    performFind(true, false);
}

void RawEditorFindController::hideFindBar()
{
    if (context_.searchBar == nullptr || context_.editor == nullptr) {
        return;
    }

    context_.searchBar->setVisible(false);
    context_.editor->setFocus();
}

bool RawEditorFindController::findNext()
{
    return performFind(true, true);
}

bool RawEditorFindController::findPrevious()
{
    return performFind(false, true);
}

bool RawEditorFindController::replaceCurrent()
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(trText("Enter text to find."), true);
        return false;
    }

    QTextCursor cursor = context_.editor->textCursor();
    if (!cursor.hasSelection() || cursor.selectedText() != findText) {
        if (!performFind(true, true)) {
            return false;
        }

        cursor = context_.editor->textCursor();
    }

    cursor.insertText(currentReplaceText());
    context_.editor->setTextCursor(cursor);
    updateSearchResults(trText("Replaced current match."));
    return performFind(true, true);
}

int RawEditorFindController::replaceAll()
{
    if (context_.editor == nullptr) {
        return 0;
    }

    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(trText("Enter text to find."), true);
        return 0;
    }

    const QString replaceText = currentReplaceText();
    QTextCursor cursor(context_.editor->document());
    cursor.beginEditBlock();

    int replacements = 0;
    cursor = context_.editor->document()->find(findText, cursor, findFlags());
    while (!cursor.isNull()) {
        cursor.insertText(replaceText);
        ++replacements;
        cursor = context_.editor->document()->find(findText, cursor, findFlags());
    }

    cursor.endEditBlock();
    context_.editor->moveCursor(QTextCursor::Start);
    updateSearchResults(trText("Replaced %1 occurrence(s).").arg(replacements));
    return replacements;
}

void RawEditorFindController::handleFindTextEdited()
{
    if (context_.searchBar == nullptr || !context_.searchBar->isVisible()) {
        return;
    }

    updateSearchResults(trText("Ready"));
}

void RawEditorFindController::handleReplaceTextEdited()
{
    if (context_.searchBar == nullptr || !context_.searchBar->isVisible()) {
        return;
    }

    updateSearchResults(trText("Ready"));
}

void RawEditorFindController::handleSearchOptionsChanged()
{
    if (context_.searchBar == nullptr || !context_.searchBar->isVisible()) {
        return;
    }

    updateSearchResults(trText("Ready"));
}

void RawEditorFindController::handleFindNextTriggered()
{
    performFind(true, true);
}

void RawEditorFindController::handleFindPreviousTriggered()
{
    performFind(false, true);
}

void RawEditorFindController::handleReplaceTriggered()
{
    replaceCurrent();
}

void RawEditorFindController::handleReplaceAllTriggered()
{
    replaceAll();
}

void RawEditorFindController::handleCloseSearchTriggered()
{
    hideFindBar();
}

void RawEditorFindController::updateSearchResults(const QString &message, bool error) const
{
    if (context_.searchStatusLabel == nullptr) {
        return;
    }

    context_.searchStatusLabel->setText(message);
    context_.searchStatusLabel->setStyleSheet(error ? QStringLiteral("color: #cc6666;") : QString());
}

void RawEditorFindController::updateSearchVisibility(bool replaceMode) const
{
    if (context_.replaceRow != nullptr) {
        context_.replaceRow->setVisible(replaceMode);
    }
    if (context_.replaceButton != nullptr) {
        context_.replaceButton->setVisible(replaceMode);
    }
    if (context_.replaceAllButton != nullptr) {
        context_.replaceAllButton->setVisible(replaceMode);
    }
}

bool RawEditorFindController::performFind(bool forward, bool wrapSearch)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(trText("Enter text to find."), true);
        return false;
    }

    QTextDocument::FindFlags flags = findFlags();
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }

    QTextCursor cursor = context_.editor->textCursor();
    if (cursor.hasSelection()) {
        cursor.setPosition(forward ? cursor.selectionEnd() : cursor.selectionStart());
    }

    QTextCursor foundCursor = context_.editor->document()->find(findText, cursor, flags);
    bool wrapped = false;

    if (foundCursor.isNull() && wrapSearch) {
        wrapped = true;
        QTextCursor wrapCursor(context_.editor->document());
        wrapCursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        foundCursor = context_.editor->document()->find(findText, wrapCursor, flags);
    }

    if (foundCursor.isNull()) {
        updateSearchResults(trText("No matches found."), true);
        return false;
    }

    context_.editor->setTextCursor(foundCursor);
    context_.editor->ensureCursorVisible();
    updateSearchResults(wrapped ? trText("Wrapped search.") : trText("Match found."));
    return true;
}

QTextDocument::FindFlags RawEditorFindController::findFlags() const
{
    QTextDocument::FindFlags flags;
    if (context_.wholeWordCheck != nullptr && context_.wholeWordCheck->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    if (context_.caseSensitiveCheck != nullptr && context_.caseSensitiveCheck->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    return flags;
}

QString RawEditorFindController::currentFindText() const
{
    if (context_.findEdit == nullptr) {
        return QString();
    }
    return context_.findEdit->text().trimmed();
}

QString RawEditorFindController::currentReplaceText() const
{
    if (context_.replaceEdit == nullptr) {
        return QString();
    }
    return context_.replaceEdit->text();
}

QString RawEditorFindController::trText(const char *sourceText)
{
    return QCoreApplication::translate("TherionStudio::TextEditorTab", sourceText);
}
}
