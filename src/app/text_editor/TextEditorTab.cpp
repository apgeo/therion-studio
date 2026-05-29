#include "TextEditorTab.h"
#include "block_editor/BlockEditorApplyExecutor.h"
#include "block_editor/BlockEditorApplyStateController.h"
#include "block_editor/BlockEditorCanvasSelectionController.h"
#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCanvasItem.h"
#include "block_editor/BlockEditorCanvasView.h"
#include "block_editor/BlockEditorCanvasRebuildController.h"
#include "block_editor/BlockEditorConfigureController.h"
#include "block_editor/BlockEditorDeleteExecutor.h"
#include "block_editor/BlockEditorDetailsSupport.h"
#include "block_editor/BlockEditorDetailsHelpController.h"
#include "block_editor/BlockEditorDetailsPaneController.h"
#include "block_editor/BlockEditorDocumentOutlineBuilder.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorInsertionController.h"
#include "block_editor/BlockEditorLineBuildService.h"
#include "block_editor/BlockEditorMoveController.h"
#include "block_editor/BlockEditorMovePreviewController.h"
#include "block_editor/BlockEditorOptionArgsController.h"
#include "block_editor/BlockEditorSelectionDetailsController.h"
#include "block_editor/BlockEditorSourceController.h"
#include "block_editor/BlockEditorSourceText.h"
#include "block_editor/BlockEditorToolboxController.h"
#include "block_editor/BlockEditorTokenTagEditor.h"
#include "block_editor/BlockEditorToolboxDetailsController.h"
#include "raw_editor/RawEditorCompletionController.h"
#include "raw_editor/RawEditorFindController.h"
#include "raw_editor/RawEditorTextEdit.h"
#include "TextEditorAppearanceController.h"
#include "TextEditorContextHelpController.h"
#include "TextEditorCursorController.h"
#include "TextEditorDocumentController.h"
#include "TextEditorEncodingController.h"
#include "TextEditorModeController.h"
#include "TextEditorTabInteractionController.h"
#include "TextEditorSourceRewriteController.h"
#include "TextEditorStatusController.h"
#include "TextEditorSurfaceStyler.h"

#include <QLineEdit>

#include <utility>

#include "../../core/IFileSystem.h"
#include "../../core/TherionCommandSyntax.h"
#include "../../editor/TherionSyntaxHighlighter.h"

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

}

namespace TherionStudio
{
TextEditorTab::TextEditorTab(IFileSystem &fileSystem, CommandCatalogStore catalogStore, QWidget *parent)
    : QWidget(parent)
    , fileSystem_(&fileSystem)
    , catalogStore_(std::move(catalogStore))
{
    buildAll();
}

TextEditorTab::~TextEditorTab()
{
    tearingDown_ = true;
    if (auto *typedCanvasView = dynamic_cast<BlockEditorCanvasView *>(blockCanvasView_); typedCanvasView != nullptr) {
        typedCanvasView->onDropBlock = {};
        typedCanvasView->onDragPreview = {};
    }
    if (blockCanvasScene_ != nullptr) {
        disconnect(blockCanvasScene_, nullptr, this, nullptr);
    }
}

void TextEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void TextEditorTab::handleApplicationAppearanceChanged()
{
    if (appearanceController_ != nullptr) {
        appearanceController_->handleApplicationAppearanceChanged();
    }
    if (highlighter_ != nullptr) {
        highlighter_->reloadPaletteForApplicationAppearance();
    }
}

void TextEditorTab::goToLine(int lineNumber)
{
    if (cursorController_ != nullptr) {
        cursorController_->goToLine(lineNumber);
    }
}

void TextEditorTab::goToLineColumn(int lineNumber, int columnNumber)
{
    if (cursorController_ != nullptr) {
        cursorController_->goToLineColumn(lineNumber, columnNumber);
    }
}

void TextEditorTab::showFindBar(bool replaceMode)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->showFindBar(replaceMode);
    }
}

void TextEditorTab::hideFindBar()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->hideFindBar();
    }
}

bool TextEditorTab::findNext()
{
    if (rawEditorFindController_ == nullptr) {
        return false;
    }
    return rawEditorFindController_->findNext();
}

bool TextEditorTab::findPrevious()
{
    if (rawEditorFindController_ == nullptr) {
        return false;
    }
    return rawEditorFindController_->findPrevious();
}

bool TextEditorTab::replaceCurrent()
{
    if (rawEditorFindController_ == nullptr) {
        return false;
    }
    return rawEditorFindController_->replaceCurrent();
}

int TextEditorTab::replaceAll()
{
    if (rawEditorFindController_ == nullptr) {
        return 0;
    }
    return rawEditorFindController_->replaceAll();
}

QString TextEditorTab::filePath() const
{
    return filePath_;
}

void TextEditorTab::populateBlockToolbox()
{
    if (blockToolboxController_ != nullptr) {
        blockToolboxController_->populateBlockToolbox();
    }
}

void TextEditorTab::populateBlockToolboxScopeCombo()
{
    if (blockToolboxController_ != nullptr) {
        blockToolboxController_->populateBlockToolboxScopeCombo();
    }
}

QString TextEditorTab::selectedBlockInsertionContextToken() const
{
    return blockToolboxController_ != nullptr
        ? blockToolboxController_->selectedBlockInsertionContextToken()
        : QStringLiteral("none");
}

void TextEditorTab::updateBlockToolboxScopeLabel()
{
    if (blockToolboxController_ != nullptr) {
        blockToolboxController_->updateBlockToolboxScopeLabel();
    }
}

void TextEditorTab::rebuildBlocksCanvasFromText()
{
    if (blockDetailsCanvasRebuildController_ != nullptr) {
        blockDetailsCanvasRebuildController_->rebuildBlocksCanvasFromText();
    }
}

void TextEditorTab::updateBlockMovePreview(int sourceLineNumber, const QPointF &scenePos)
{
    if (tearingDown_) {
        return;
    }
    if (blockDetailsMovePreviewController_ != nullptr) {
        blockDetailsMovePreviewController_->updateMovePreview(sourceLineNumber, scenePos);
    }
}

void TextEditorTab::clearBlockMovePreview()
{
    if (tearingDown_) {
        return;
    }
    if (blockDetailsMovePreviewController_ != nullptr) {
        blockDetailsMovePreviewController_->clearMovePreview();
    }
}

int TextEditorTab::resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const
{
    if (blockDetailsCanvasBoundaryController_ == nullptr) {
        return 0;
    }
    return blockDetailsCanvasBoundaryController_->resolveEndHintContainerStartLineAtScenePos(scenePos);
}

bool TextEditorTab::supportsDetailsPaneForKind(const QString &kind) const
{
    const QString normalizedKind = normalizeDirective(kind);
    if (normalizedKind.isEmpty() || isBlockClosingDirective(normalizedKind)) {
        return false;
    }
    if (normalizedKind == QStringLiteral("encoding")) {
        return false;
    }
    if (isContainerBlockDirective(normalizedKind) || normalizedKind == QStringLiteral("data")) {
        return true;
    }
    if (!commandMetadata().commandOptionTokens.value(normalizedKind).isEmpty()) {
        return true;
    }
    return true;
}

void TextEditorTab::clearBlockDetailsPane()
{
    if (blockDetailsPaneController_ != nullptr) {
        blockDetailsPaneController_->clearDetailsPane();
    }
}

void TextEditorTab::showBlockDetailsForToolboxCommand(const QString &commandToken)
{
    if (blockDetailsToolboxDetailsController_ != nullptr) {
        blockDetailsToolboxDetailsController_->showToolboxCommandDetails(commandToken);
    }
}

void TextEditorTab::selectBlockInCanvasAndDetails(int lineNumber)
{
    if (blockDetailsCanvasSelectionController_ != nullptr) {
        blockDetailsCanvasSelectionController_->selectBlockInCanvasAndDetails(lineNumber);
    }
}

void TextEditorTab::refreshBlockDetailsSelectionFromScene()
{
    if (blockDetailsCanvasSelectionController_ != nullptr) {
        blockDetailsCanvasSelectionController_->refreshDetailsSelectionFromScene();
    }
}

bool TextEditorTab::loadBlockDetailsForSelection(const QString &kind, int lineNumber)
{
    if (blockDetailsSelectionDetailsController_ == nullptr) {
        return false;
    }
    return blockDetailsSelectionDetailsController_->loadSelectionDetails(kind, lineNumber);
}

void TextEditorTab::refreshBlockDetailsOptionArgumentEditors()
{
    if (blockDetailsOptionArgsController_ != nullptr) {
        blockDetailsOptionArgsController_->refreshOptionArgumentEditors();
    }
}

QGraphicsItem *TextEditorTab::resolveBlockCanvasItem(QGraphicsItem *item) const
{
    return blockDetailsCanvasSelectionController_ != nullptr
        ? blockDetailsCanvasSelectionController_->resolveBlockCanvasItem(item)
        : nullptr;
}

int TextEditorTab::blockCanvasItemLineNumber(const QGraphicsItem *item) const
{
    return blockDetailsCanvasSelectionController_ != nullptr
        ? blockDetailsCanvasSelectionController_->blockCanvasItemLineNumber(item)
        : 0;
}

QString TextEditorTab::blockCanvasItemKind(const QGraphicsItem *item) const
{
    return blockDetailsCanvasSelectionController_ != nullptr
        ? blockDetailsCanvasSelectionController_->blockCanvasItemKind(item)
        : QString();
}

void TextEditorTab::selectBlockCanvasItem(QGraphicsItem *item, bool centerView)
{
    if (blockDetailsCanvasSelectionController_ != nullptr) {
        blockDetailsCanvasSelectionController_->selectBlockCanvasItem(item, centerView);
    }
}

void TextEditorTab::updateBlockDetailsHelpForCurrentFocus()
{
    if (blockDetailsHelpController_ != nullptr) {
        blockDetailsHelpController_->updateHelpForCurrentFocus();
    }
}

bool TextEditorTab::blockDetailsReadingsTagEditorHasEditorFocus() const
{
    if (auto *readingsTagEditor = blockEditorTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        return readingsTagEditor->hasEditorFocus();
    }
    return false;
}

QString TextEditorTab::blockDetailsReadingsOrderTextForBuild() const
{
    if (auto *readingsTagEditor = blockEditorTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        return readingsTagEditor->tokens().join(QLatin1Char(' ')).trimmed();
    }
    if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
        return blockDetailsAdditionalPositionalEdit_->text().trimmed();
    }
    return QString();
}

void TextEditorTab::setBlockDetailsReadingsTagEditor(const QString &placeholderText,
                                                     const QStringList &suggestions,
                                                     const QStringList &tokens)
{
    if (auto *readingsTagEditor = blockEditorTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        readingsTagEditor->setPlaceholderText(placeholderText);
        readingsTagEditor->setSuggestions(suggestions);
        readingsTagEditor->setTokens(tokens);
    }
}

void TextEditorTab::installBlockDetailsLineEditCompleter(QLineEdit *lineEdit,
                                                          const QStringList &values) const
{
    installBlockEditorLineEditCompleter(lineEdit, values);
}

bool TextEditorTab::isContainerBlockDirectiveForBlocks(const QString &directive) const
{
    return isContainerBlockDirective(directive);
}

bool TextEditorTab::isRequiredArgumentSignatureForBlocks(const QString &signature) const
{
    return isRequiredBlockArgumentSignature(signature);
}

bool TextEditorTab::buildUpdatedLineFromBlockDetails(QString *updatedLine, QString *validationError) const
{
    if (blockDetailsLineBuildService_ == nullptr) {
        return false;
    }
    return blockDetailsLineBuildService_->buildUpdatedLine(updatedLine, validationError);
}

void TextEditorTab::refreshBlockDetailsApplyState()
{
    if (blockDetailsApplyStateController_ != nullptr) {
        blockDetailsApplyStateController_->refreshApplyState();
    }
}

void TextEditorTab::applyBlockDetailsChanges()
{
    if (blockDetailsApplyExecutor_ != nullptr) {
        blockDetailsApplyExecutor_->applyChanges();
    }
}

void TextEditorTab::handleCanvasDrop(const QString &kind, const QPointF &scenePos)
{
    BlockEditorInsertionController(blockEditorInsertionContext()).insertFromDrop(kind, scenePos);
}

void TextEditorTab::handleBlockMoveRequest(int lineNumber, const QPointF &scenePos)
{
    BlockEditorMoveController(blockEditorMoveContext()).moveBlock(lineNumber, scenePos);
}

void TextEditorTab::handleBlockConfigureRequest(const QString &kind, int lineNumber, bool showCommandHelpOnly)
{
    BlockEditorConfigureController(blockEditorConfigureContext()).configureBlock(kind, lineNumber, showCommandHelpOnly);
}

bool TextEditorTab::handleBlockDeleteRequest(int lineNumber)
{
    BlockEditorDeleteExecutor deleteExecutor(blockEditorDeleteContext());
    return deleteExecutor.deleteCommandAtLine(lineNumber);
}

void TextEditorTab::handleTextChanged()
{
    TextEditorTabInteractionController::TextChangedActions actions;
    actions.rebuildBlocksCanvasFromText = [this]() {
        rebuildBlocksCanvasFromText();
    };
    actions.applyDirtyStateFromCurrentState = [this]() {
        applyDirtyStateFromCurrentState();
    };
    actions.emitDocumentTextChanged = [this]() {
        emit documentTextChanged();
    };
    TextEditorTabInteractionController::handleTextChanged(loading_, actions);
}

void TextEditorTab::handleFindTextEdited(const QString &)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleFindTextEdited();
    }
}

void TextEditorTab::handleReplaceTextEdited(const QString &)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleReplaceTextEdited();
    }
}

void TextEditorTab::handleSearchOptionsChanged(bool)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleSearchOptionsChanged();
    }
}

void TextEditorTab::handleFindNextTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleFindNextTriggered();
    }
}

void TextEditorTab::handleFindPreviousTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleFindPreviousTriggered();
    }
}

void TextEditorTab::handleReplaceTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleReplaceTriggered();
    }
}

void TextEditorTab::handleReplaceAllTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleReplaceAllTriggered();
    }
}

void TextEditorTab::handleCloseSearchTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleCloseSearchTriggered();
    }
}

void TextEditorTab::handleConvertToUtf8Triggered()
{
    if (encodingController_ != nullptr) {
        encodingController_->handleConvertToUtf8Triggered();
    }
}

void TextEditorTab::buildHelpPanel()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->buildHelpPanel();
    }
}

void TextEditorTab::loadHelpMetadata()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->loadHelpMetadata();
    }
}

void TextEditorTab::loadHelpMetadataFromCommandCatalog()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->loadHelpMetadataFromCommandCatalog();
    }
}

}
