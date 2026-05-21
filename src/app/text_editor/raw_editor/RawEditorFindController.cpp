#include "RawEditorFindController.h"

#include "../TextEditorTab.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>

namespace TherionStudio
{
RawEditorFindController::RawEditorFindController(TextEditorTab *owner)
    : owner_(owner)
{
}

void RawEditorFindController::showFindBar(bool replaceMode)
{
    if (owner_ == nullptr) {
        return;
    }

    if (owner_->blocksModeActive_) {
        owner_->setBlocksModeActive(false);
    }

    owner_->replaceMode_ = replaceMode;
    updateSearchVisibility(replaceMode);
    owner_->searchBar_->setVisible(true);

    if (owner_->findEdit_->text().isEmpty() && !owner_->editor_->textCursor().selectedText().isEmpty()) {
        owner_->findEdit_->setText(owner_->editor_->textCursor().selectedText());
    }

    owner_->findEdit_->setFocus();
    owner_->findEdit_->selectAll();
    updateSearchResults(owner_->tr("Ready"));
}

void RawEditorFindController::hideFindBar()
{
    if (owner_ == nullptr) {
        return;
    }

    owner_->searchBar_->setVisible(false);
    owner_->editor_->setFocus();
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
    if (owner_ == nullptr) {
        return false;
    }

    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(owner_->tr("Enter text to find."), true);
        return false;
    }

    QTextCursor cursor = owner_->editor_->textCursor();
    if (!cursor.hasSelection() || cursor.selectedText() != findText) {
        if (!performFind(true, true)) {
            return false;
        }

        cursor = owner_->editor_->textCursor();
    }

    cursor.insertText(currentReplaceText());
    owner_->editor_->setTextCursor(cursor);
    updateSearchResults(owner_->tr("Replaced current match."));
    return performFind(true, true);
}

int RawEditorFindController::replaceAll()
{
    if (owner_ == nullptr) {
        return 0;
    }

    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(owner_->tr("Enter text to find."), true);
        return 0;
    }

    const QString replaceText = currentReplaceText();
    QTextCursor cursor(owner_->editor_->document());
    cursor.beginEditBlock();

    int replacements = 0;
    cursor = owner_->editor_->document()->find(findText, cursor, findFlags());
    while (!cursor.isNull()) {
        cursor.insertText(replaceText);
        ++replacements;
        cursor = owner_->editor_->document()->find(findText, cursor, findFlags());
    }

    cursor.endEditBlock();
    owner_->editor_->moveCursor(QTextCursor::Start);
    updateSearchResults(owner_->tr("Replaced %1 occurrence(s).").arg(replacements));
    return replacements;
}

void RawEditorFindController::handleFindTextEdited()
{
    if (owner_ == nullptr || !owner_->searchBar_->isVisible()) {
        return;
    }

    updateSearchResults(owner_->tr("Ready"));
}

void RawEditorFindController::handleReplaceTextEdited()
{
    if (owner_ == nullptr || !owner_->searchBar_->isVisible()) {
        return;
    }

    updateSearchResults(owner_->tr("Ready"));
}

void RawEditorFindController::handleSearchOptionsChanged()
{
    if (owner_ == nullptr || !owner_->searchBar_->isVisible()) {
        return;
    }

    updateSearchResults(owner_->tr("Ready"));
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
    if (owner_ == nullptr || owner_->searchStatusLabel_ == nullptr) {
        return;
    }

    owner_->searchStatusLabel_->setText(message);
    owner_->searchStatusLabel_->setStyleSheet(error ? QStringLiteral("color: #cc6666;") : QString());
}

void RawEditorFindController::updateSearchVisibility(bool replaceMode) const
{
    if (owner_ == nullptr) {
        return;
    }

    owner_->replaceRow_->setVisible(replaceMode);
    owner_->replaceButton_->setVisible(replaceMode);
    owner_->replaceAllButton_->setVisible(replaceMode);
}

bool RawEditorFindController::performFind(bool forward, bool wrapSearch)
{
    if (owner_ == nullptr) {
        return false;
    }

    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(owner_->tr("Enter text to find."), true);
        return false;
    }

    QTextDocument::FindFlags flags = findFlags();
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }

    QTextCursor cursor = owner_->editor_->textCursor();
    if (cursor.hasSelection()) {
        cursor.setPosition(forward ? cursor.selectionEnd() : cursor.selectionStart());
    }

    QTextCursor foundCursor = owner_->editor_->document()->find(findText, cursor, flags);
    bool wrapped = false;

    if (foundCursor.isNull() && wrapSearch) {
        wrapped = true;
        QTextCursor wrapCursor(owner_->editor_->document());
        wrapCursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        foundCursor = owner_->editor_->document()->find(findText, wrapCursor, flags);
    }

    if (foundCursor.isNull()) {
        updateSearchResults(owner_->tr("No matches found."), true);
        return false;
    }

    owner_->editor_->setTextCursor(foundCursor);
    owner_->editor_->ensureCursorVisible();
    updateSearchResults(wrapped ? owner_->tr("Wrapped search.") : owner_->tr("Match found."));
    return true;
}

QTextDocument::FindFlags RawEditorFindController::findFlags() const
{
    QTextDocument::FindFlags flags;
    if (owner_ != nullptr && owner_->wholeWordCheck_->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    if (owner_ != nullptr && owner_->caseSensitiveCheck_->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    return flags;
}

QString RawEditorFindController::currentFindText() const
{
    if (owner_ == nullptr || owner_->findEdit_ == nullptr) {
        return QString();
    }
    return owner_->findEdit_->text().trimmed();
}

QString RawEditorFindController::currentReplaceText() const
{
    if (owner_ == nullptr || owner_->replaceEdit_ == nullptr) {
        return QString();
    }
    return owner_->replaceEdit_->text();
}
}
