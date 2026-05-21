#include "TextEditorStatusController.h"

#include "TextEditorTab.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QWidget>

namespace TherionStudio
{
TextEditorStatusController::TextEditorStatusController(TextEditorTab *owner)
    : owner_(owner)
{
}

QString TextEditorStatusController::displayName() const
{
    if (owner_ == nullptr || owner_->filePath_.isEmpty()) {
        return TextEditorTab::tr("Untitled");
    }

    QString name = QFileInfo(owner_->filePath_).fileName();
    if (name.isEmpty()) {
        name = TextEditorTab::tr("Untitled");
    }

    if (owner_->dirty_) {
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
    if (owner_ == nullptr) {
        return QStringLiteral("UTF-8");
    }
    return owner_->fileEncodingLabel_;
}

void TextEditorStatusController::setInlineStatusVisible(bool visible)
{
    if (owner_ == nullptr) {
        return;
    }

    owner_->inlineStatusRequestedVisible_ = visible;
    refreshStatus();
}

void TextEditorStatusController::refreshTitle()
{
    if (owner_ == nullptr) {
        return;
    }

    refreshStatus();
    emit owner_->titleChanged();
}

void TextEditorStatusController::refreshStatus()
{
    if (owner_ == nullptr) {
        return;
    }

    if (owner_->encodingNoteLabel_ != nullptr) {
        owner_->encodingNoteLabel_->setText(owner_->encodingStatusNote_);
        owner_->encodingNoteLabel_->setVisible(!owner_->encodingStatusNote_.isEmpty());
    }
    if (owner_->convertEncodingButton_ != nullptr) {
        const bool showConversion = owner_->fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0;
        owner_->convertEncodingButton_->setVisible(showConversion);
        owner_->convertEncodingButton_->setEnabled(showConversion);
    }

    const bool showInlineRow = owner_->inlineStatusRequestedVisible_
        && (!owner_->encodingStatusNote_.isEmpty()
            || (owner_->convertEncodingButton_ != nullptr && owner_->convertEncodingButton_->isVisible()));
    if (owner_->statusRow_ != nullptr) {
        owner_->statusRow_->setVisible(showInlineRow);
    }
}

bool TextEditorStatusController::isCurrentStateDirty() const
{
    if (owner_ == nullptr) {
        return false;
    }

    const bool textDirty = owner_->editor_ != nullptr && owner_->editor_->toPlainText() != owner_->cleanTextSnapshot_;
    const bool encodingDirty = owner_->fileEncodingName_.compare(owner_->cleanEncodingNameSnapshot_, Qt::CaseInsensitive) != 0;
    return textDirty || encodingDirty;
}

void TextEditorStatusController::applyDirtyStateFromCurrentState()
{
    if (owner_ == nullptr) {
        return;
    }

    const bool dirtyNow = isCurrentStateDirty();
    if (owner_->editor_ != nullptr && owner_->editor_->document() != nullptr) {
        owner_->editor_->document()->setModified(dirtyNow);
    }
    if (owner_->dirty_ == dirtyNow) {
        return;
    }

    owner_->dirty_ = dirtyNow;
    refreshTitle();
    emit owner_->dirtyStateChanged(owner_->dirty_);
}

QString TextEditorStatusController::displayPath() const
{
    if (owner_ == nullptr || owner_->filePath_.isEmpty()) {
        return TextEditorTab::tr("No file open");
    }

    if (owner_->projectRootPath_.isEmpty()) {
        return owner_->filePath_;
    }

    const QString relativePath = QDir(owner_->projectRootPath_).relativeFilePath(owner_->filePath_);
    if (relativePath.isEmpty() || relativePath.startsWith(QStringLiteral(".."))) {
        return owner_->filePath_;
    }

    return relativePath;
}
}
