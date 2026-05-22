#include "TextEditorDocumentController.h"

#include "../../core/DocumentFile.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>

#include <utility>

namespace TherionStudio
{
TextEditorDocumentController::TextEditorDocumentController(TextEditorDocumentContext context)
    : context_(std::move(context))
{
}

QString TextEditorDocumentController::tr(const char *text) const
{
    if (context_.translate) {
        return context_.translate(text);
    }
    return QString::fromUtf8(text);
}

bool TextEditorDocumentController::loadFile(const QString &filePath, QString *errorMessage)
{
    if (context_.editor == nullptr
        || context_.filePath == nullptr
        || context_.fileEncodingName == nullptr
        || context_.fileEncodingLabel == nullptr
        || context_.encodingStatusNote == nullptr
        || context_.loading == nullptr
        || context_.currentLineNumber == nullptr
        || context_.currentColumnNumber == nullptr
        || context_.highlightedLineNumber == nullptr
        || context_.cleanTextSnapshot == nullptr
        || context_.cleanEncodingNameSnapshot == nullptr
        || context_.dirty == nullptr
        || context_.blocksModeActive == nullptr
        || context_.blockDetailsSelectedLineNumber == nullptr
        || context_.blockDetailsSelectedKind == nullptr) {
        return false;
    }

    QString contents;
    QString loadedEncoding = QStringLiteral("UTF-8");
    QString loadedEncodingLabel;
    if (!DocumentFile::readTextFile(filePath, &contents, &loadedEncoding, &loadedEncodingLabel, errorMessage)) {
        return false;
    }

    (*context_.filePath) = filePath;
    (*context_.fileEncodingName) = loadedEncoding.trimmed().isEmpty() ? QStringLiteral("UTF-8") : loadedEncoding.trimmed();
    (*context_.fileEncodingLabel) = loadedEncodingLabel.isEmpty() ? QStringLiteral("UTF-8") : loadedEncodingLabel;
    if ((*context_.fileEncodingName).compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        (*context_.encodingStatusNote).clear();
    } else {
        (*context_.encodingStatusNote) = tr("Opened as %1. Save keeps this encoding unless you convert to UTF-8.")
                                          .arg((*context_.fileEncodingLabel));
    }
    (*context_.loading) = true;
    context_.editor->setPlainText(contents);
    (*context_.loading) = false;
    const QTextCursor cursor = context_.editor->textCursor();
    (*context_.currentLineNumber) = cursor.blockNumber() + 1;
    (*context_.currentColumnNumber) = cursor.positionInBlock() + 1;
    (*context_.highlightedLineNumber) = (*context_.currentLineNumber);
    (*context_.cleanTextSnapshot) = context_.editor->toPlainText();
    (*context_.cleanEncodingNameSnapshot) = (*context_.fileEncodingName);
    context_.editor->document()->setModified(false);
    (*context_.dirty) = false;
    if (context_.refreshBlocksModeAvailability) {
        context_.refreshBlocksModeAvailability();
    }
    if ((*context_.blocksModeActive)
        && context_.isBlocksModeSupportedForCurrentFile
        && !context_.isBlocksModeSupportedForCurrentFile()
        && context_.setBlocksModeActive) {
        context_.setBlocksModeActive(false);
    }
    (*context_.blockDetailsSelectedLineNumber) = 0;
    (*context_.blockDetailsSelectedKind).clear();
    if (context_.rebuildBlocksCanvasFromText) {
        context_.rebuildBlocksCanvasFromText();
    }
    if (context_.clearBlockDetailsPane) {
        context_.clearBlockDetailsPane();
    }
    if (context_.populateBlockToolbox) {
        context_.populateBlockToolbox();
    }
    if (context_.refreshEditorModeUi) {
        context_.refreshEditorModeUi();
    }
    if (context_.refreshTitle) {
        context_.refreshTitle();
    }
    if (context_.refreshCurrentLineHighlight) {
        context_.refreshCurrentLineHighlight();
    }
    if (context_.dirtyStateChanged) {
        context_.dirtyStateChanged(false);
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
    return true;
}

bool TextEditorDocumentController::save(QString *errorMessage)
{
    if (context_.editor == nullptr
        || context_.filePath == nullptr
        || context_.fileEncodingName == nullptr
        || context_.fileEncodingLabel == nullptr
        || context_.encodingStatusNote == nullptr
        || context_.cleanTextSnapshot == nullptr
        || context_.cleanEncodingNameSnapshot == nullptr
        || context_.dirty == nullptr) {
        return false;
    }
    if ((*context_.filePath).isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("This document does not have a file path yet.");
        }
        return false;
    }

    if (!DocumentFile::writeTextFile((*context_.filePath),
                                     context_.editor->toPlainText(),
                                     (*context_.fileEncodingName),
                                     errorMessage)) {
        return false;
    }

    (*context_.cleanTextSnapshot) = context_.editor->toPlainText();
    (*context_.cleanEncodingNameSnapshot) = (*context_.fileEncodingName);
    context_.editor->document()->setModified(false);
    (*context_.dirty) = false;
    if ((*context_.fileEncodingName).compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        (*context_.encodingStatusNote).clear();
    } else {
        (*context_.encodingStatusNote) = tr("Saved using %1 encoding.").arg((*context_.fileEncodingLabel));
    }
    if (context_.refreshTitle) {
        context_.refreshTitle();
    }
    if (context_.dirtyStateChanged) {
        context_.dirtyStateChanged(false);
    }
    return true;
}

void TextEditorDocumentController::setProjectRootPath(const QString &projectRootPath)
{
    if (context_.projectRootPath == nullptr) {
        return;
    }

    (*context_.projectRootPath) = projectRootPath;
    if (context_.refreshStatus) {
        context_.refreshStatus();
    }
}
}
