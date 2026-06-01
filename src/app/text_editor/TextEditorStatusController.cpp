#include "TextEditorStatusController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QWidget>

#include <utility>

namespace TherionStudio
{
TextEditorStatusController::TextEditorStatusController(TextEditorStatusContext context)
    : context_(std::move(context))
{
}

QString TextEditorStatusController::displayName() const
{
    if (context_.filePath == nullptr || context_.filePath->isEmpty()) {
        return trText("Untitled");
    }

    QString name = QFileInfo(*context_.filePath).fileName();
    if (name.isEmpty()) {
        name = trText("Untitled");
    }

    if (context_.dirty != nullptr && *context_.dirty) {
        name.prepend(QStringLiteral("*"));
    }

    return name;
}

QString TextEditorStatusController::statusPathText() const
{
    return displayPath();
}

QString TextEditorStatusController::statusEncodingText() const
{
    if (context_.fileEncodingLabel == nullptr) {
        return QStringLiteral("UTF-8");
    }
    return *context_.fileEncodingLabel;
}

void TextEditorStatusController::setInlineStatusVisible(bool visible)
{
    if (context_.inlineStatusRequestedVisible == nullptr) {
        return;
    }

    *context_.inlineStatusRequestedVisible = visible;
    refreshStatus();
}

void TextEditorStatusController::refreshTitle()
{
    refreshStatus();
    if (context_.titleChanged) {
        context_.titleChanged();
    }
}

void TextEditorStatusController::refreshStatus()
{
    if (context_.fileEncodingName == nullptr
        || context_.encodingStatusNote == nullptr
        || context_.inlineStatusRequestedVisible == nullptr) {
        return;
    }

    if (context_.encodingNoteLabel != nullptr) {
        context_.encodingNoteLabel->setText(*context_.encodingStatusNote);
        context_.encodingNoteLabel->setVisible(!context_.encodingStatusNote->isEmpty());
    }
    if (context_.convertEncodingButton != nullptr) {
        const bool showConversion = context_.fileEncodingName->compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0;
        context_.convertEncodingButton->setVisible(showConversion);
        context_.convertEncodingButton->setEnabled(showConversion);
    }

    if (context_.statusRow != nullptr) {
        context_.statusRow->setVisible(false);
    }
}

bool TextEditorStatusController::isCurrentStateDirty() const
{
    if (context_.fileEncodingName == nullptr || context_.cleanEncodingNameSnapshot == nullptr) {
        return false;
    }

    const bool textDirty = context_.editor != nullptr
        && context_.cleanTextSnapshot != nullptr
        && context_.editor->toPlainText() != *context_.cleanTextSnapshot;
    const bool encodingDirty = context_.fileEncodingName->compare(*context_.cleanEncodingNameSnapshot, Qt::CaseInsensitive) != 0;
    return textDirty || encodingDirty;
}

void TextEditorStatusController::applyDirtyStateFromCurrentState()
{
    if (context_.dirty == nullptr) {
        return;
    }

    const bool dirtyNow = isCurrentStateDirty();
    if (context_.editor != nullptr && context_.editor->document() != nullptr) {
        context_.editor->document()->setModified(dirtyNow);
    }
    if (*context_.dirty == dirtyNow) {
        return;
    }

    *context_.dirty = dirtyNow;
    refreshTitle();
    if (context_.dirtyStateChanged) {
        context_.dirtyStateChanged(*context_.dirty);
    }
}

QString TextEditorStatusController::displayPath() const
{
    if (context_.filePath == nullptr || context_.filePath->isEmpty()) {
        return trText("No file open");
    }

    if (context_.projectRootPath == nullptr || context_.projectRootPath->isEmpty()) {
        return *context_.filePath;
    }

    const QString relativePath = QDir(*context_.projectRootPath).relativeFilePath(*context_.filePath);
    if (relativePath.isEmpty() || relativePath.startsWith(QStringLiteral(".."))) {
        return *context_.filePath;
    }

    return relativePath;
}

QString TextEditorStatusController::trText(const char *sourceText)
{
    return QCoreApplication::translate("TherionStudio::TextEditorTab", sourceText);
}
}
