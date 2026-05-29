#include "TextEditorDocumentController.h"
#include "TextEditorDocumentPersistenceStateService.h"
#include "TextEditorDocumentWorkflowController.h"

#include "../../core/IFileSystem.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>

#include <utility>

namespace TherionStudio
{
TextEditorDocumentController::TextEditorDocumentController(IFileSystem &fileSystem,
                                                           TextEditorDocumentContext context)
    : fileSystem_(fileSystem)
    , context_(std::move(context))
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
    if (!fileSystem_.readTextFile(filePath, &contents, &loadedEncoding, &loadedEncodingLabel, errorMessage)) {
        return false;
    }

    (*context_.filePath) = filePath;
    (*context_.loading) = true;
    context_.editor->setPlainText(contents);
    (*context_.loading) = false;
    const QTextCursor cursor = context_.editor->textCursor();

    TextEditorDocumentPersistenceStateService::LoadStateInput loadInput;
    loadInput.filePath = filePath;
    loadInput.textContents = context_.editor->toPlainText();
    loadInput.loadedEncodingName = loadedEncoding;
    loadInput.loadedEncodingLabel = loadedEncodingLabel;
    loadInput.cursorLineNumber = cursor.blockNumber() + 1;
    loadInput.cursorColumnNumber = cursor.positionInBlock() + 1;
    loadInput.blocksModeActive = *context_.blocksModeActive;
    loadInput.blocksModeSupportedForCurrentFile = !context_.isBlocksModeSupportedForCurrentFile
        || context_.isBlocksModeSupportedForCurrentFile();
    loadInput.openedEncodingStatusTemplate = tr("Opened as %1. Save keeps this encoding unless you convert to UTF-8.");
    const auto loadUpdate = TextEditorDocumentPersistenceStateService::buildLoadStateUpdate(loadInput);

    (*context_.filePath) = loadUpdate.filePath;
    (*context_.fileEncodingName) = loadUpdate.fileEncodingName;
    (*context_.fileEncodingLabel) = loadUpdate.fileEncodingLabel;
    (*context_.encodingStatusNote) = loadUpdate.encodingStatusNote;
    (*context_.currentLineNumber) = loadUpdate.currentLineNumber;
    (*context_.currentColumnNumber) = loadUpdate.currentColumnNumber;
    (*context_.highlightedLineNumber) = loadUpdate.highlightedLineNumber;
    (*context_.cleanTextSnapshot) = loadUpdate.cleanTextSnapshot;
    (*context_.cleanEncodingNameSnapshot) = loadUpdate.cleanEncodingNameSnapshot;
    context_.editor->document()->setModified(false);
    (*context_.dirty) = loadUpdate.dirty;
    (*context_.blockDetailsSelectedLineNumber) = loadUpdate.blockDetailsSelectedLineNumber;
    (*context_.blockDetailsSelectedKind) = loadUpdate.blockDetailsSelectedKind;

    TextEditorDocumentWorkflowController::LoadActions loadActions;
    loadActions.refreshBlocksModeAvailability = context_.refreshBlocksModeAvailability;
    loadActions.setBlocksModeActive = context_.setBlocksModeActive;
    loadActions.rebuildBlocksCanvasFromText = context_.rebuildBlocksCanvasFromText;
    loadActions.clearBlockDetailsPane = context_.clearBlockDetailsPane;
    loadActions.populateBlockToolbox = context_.populateBlockToolbox;
    loadActions.refreshEditorModeUi = context_.refreshEditorModeUi;
    loadActions.refreshTitle = context_.refreshTitle;
    loadActions.refreshCurrentLineHighlight = context_.refreshCurrentLineHighlight;
    loadActions.dirtyStateChanged = context_.dirtyStateChanged;
    loadActions.updateContextHelp = context_.updateContextHelp;
    TextEditorDocumentWorkflowController::runPostLoadWorkflow(loadUpdate.disableBlocksMode, loadActions);
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

    if (!fileSystem_.writeTextFile((*context_.filePath),
                                     context_.editor->toPlainText(),
                                     (*context_.fileEncodingName),
                                     errorMessage)) {
        return false;
    }

    TextEditorDocumentPersistenceStateService::SaveStateInput saveInput;
    saveInput.textContents = context_.editor->toPlainText();
    saveInput.fileEncodingName = (*context_.fileEncodingName);
    saveInput.fileEncodingLabel = (*context_.fileEncodingLabel);
    saveInput.savedEncodingStatusTemplate = tr("Saved using %1 encoding.");
    const auto saveUpdate = TextEditorDocumentPersistenceStateService::buildSaveStateUpdate(saveInput);

    (*context_.cleanTextSnapshot) = saveUpdate.cleanTextSnapshot;
    (*context_.cleanEncodingNameSnapshot) = saveUpdate.cleanEncodingNameSnapshot;
    context_.editor->document()->setModified(false);
    (*context_.dirty) = saveUpdate.dirty;
    (*context_.encodingStatusNote) = saveUpdate.encodingStatusNote;
    TextEditorDocumentWorkflowController::SaveActions saveActions;
    saveActions.refreshTitle = context_.refreshTitle;
    saveActions.dirtyStateChanged = context_.dirtyStateChanged;
    TextEditorDocumentWorkflowController::runPostSaveWorkflow(saveActions);
    return true;
}

void TextEditorDocumentController::setProjectRootPath(const QString &projectRootPath)
{
    if (context_.projectRootPath == nullptr) {
        return;
    }

    (*context_.projectRootPath) = projectRootPath;
    TextEditorDocumentWorkflowController::ProjectRootActions actions;
    actions.refreshStatus = context_.refreshStatus;
    TextEditorDocumentWorkflowController::runPostProjectRootSetWorkflow(actions);
}
}
