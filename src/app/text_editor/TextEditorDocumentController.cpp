#include "TextEditorDocumentController.h"

#include "TextEditorTab.h"

#include "../../core/DocumentFile.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>

namespace TherionStudio
{
TextEditorDocumentController::TextEditorDocumentController(TextEditorTab *owner)
    : owner_(owner)
{
}

bool TextEditorDocumentController::loadFile(const QString &filePath, QString *errorMessage)
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return false;
    }

    QString contents;
    QString loadedEncoding = QStringLiteral("UTF-8");
    QString loadedEncodingLabel;
    if (!DocumentFile::readTextFile(filePath, &contents, &loadedEncoding, &loadedEncodingLabel, errorMessage)) {
        return false;
    }

    owner_->filePath_ = filePath;
    owner_->fileEncodingName_ = loadedEncoding.trimmed().isEmpty() ? QStringLiteral("UTF-8") : loadedEncoding.trimmed();
    owner_->fileEncodingLabel_ = loadedEncodingLabel.isEmpty() ? QStringLiteral("UTF-8") : loadedEncodingLabel;
    if (owner_->fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        owner_->encodingStatusNote_.clear();
    } else {
        owner_->encodingStatusNote_ = owner_->tr("Opened as %1. Save keeps this encoding unless you convert to UTF-8.")
                                          .arg(owner_->fileEncodingLabel_);
    }
    owner_->loading_ = true;
    owner_->editor_->setPlainText(contents);
    owner_->loading_ = false;
    const QTextCursor cursor = owner_->editor_->textCursor();
    owner_->currentLineNumber_ = cursor.blockNumber() + 1;
    owner_->currentColumnNumber_ = cursor.positionInBlock() + 1;
    owner_->highlightedLineNumber_ = owner_->currentLineNumber_;
    owner_->cleanTextSnapshot_ = owner_->editor_->toPlainText();
    owner_->cleanEncodingNameSnapshot_ = owner_->fileEncodingName_;
    owner_->editor_->document()->setModified(false);
    owner_->dirty_ = false;
    owner_->refreshBlocksModeAvailability();
    if (owner_->blocksModeActive_ && !owner_->isBlocksModeSupportedForCurrentFile()) {
        owner_->setBlocksModeActive(false);
    }
    owner_->blockDetailsSelectedLineNumber_ = 0;
    owner_->blockDetailsSelectedKind_.clear();
    owner_->rebuildBlocksCanvasFromText();
    owner_->clearBlockDetailsPane();
    owner_->populateBlockToolbox();
    owner_->refreshEditorModeUi();
    owner_->refreshTitle();
    owner_->refreshCurrentLineHighlight();
    emit owner_->dirtyStateChanged(false);
    owner_->updateContextHelp();
    return true;
}

bool TextEditorDocumentController::save(QString *errorMessage)
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return false;
    }
    if (owner_->filePath_.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = owner_->tr("This document does not have a file path yet.");
        }
        return false;
    }

    if (!DocumentFile::writeTextFile(owner_->filePath_,
                                     owner_->editor_->toPlainText(),
                                     owner_->fileEncodingName_,
                                     errorMessage)) {
        return false;
    }

    owner_->cleanTextSnapshot_ = owner_->editor_->toPlainText();
    owner_->cleanEncodingNameSnapshot_ = owner_->fileEncodingName_;
    owner_->editor_->document()->setModified(false);
    owner_->dirty_ = false;
    if (owner_->fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        owner_->encodingStatusNote_.clear();
    } else {
        owner_->encodingStatusNote_ = owner_->tr("Saved using %1 encoding.").arg(owner_->fileEncodingLabel_);
    }
    owner_->refreshTitle();
    emit owner_->dirtyStateChanged(false);
    return true;
}

void TextEditorDocumentController::setProjectRootPath(const QString &projectRootPath)
{
    if (owner_ == nullptr) {
        return;
    }

    owner_->projectRootPath_ = projectRootPath;
    owner_->refreshStatus();
}
}
