#include "TextEditorTab.h"
#include "block_editor/BlockEditorApplyExecutor.h"
#include "block_editor/BlockEditorApplyStateController.h"
#include "block_editor/BlockEditorCanvasSelectionController.h"
#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCanvasItem.h"
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
#include "TextEditorEncodingController.h"
#include "TextEditorModeController.h"
#include "TextEditorSourceRewriteController.h"
#include "TextEditorStatusController.h"
#include "TextEditorSurfaceStyler.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFrame>
#include <QGraphicsScene>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextCursor>
#include <QStyleHints>
#include <QVBoxLayout>

#include <QPen>

#include "../../core/DocumentFile.h"
#include "../../core/TherionCommandSyntax.h"
#include "../../editor/TherionSyntaxHighlighter.h"

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

constexpr int kPanelPadding = 12;
constexpr int kPanelSpacing = 8;

}

namespace TherionStudio
{
TextEditorTab::TextEditorTab(QWidget *parent)
    : QWidget(parent)
{
    appearanceController_ = std::make_unique<TextEditorAppearanceController>(this);
    contextHelpController_ = std::make_unique<TextEditorContextHelpController>(this);
    cursorController_ = std::make_unique<TextEditorCursorController>(this);
    encodingController_ = std::make_unique<TextEditorEncodingController>(this);
    rawEditorFindController_ = std::make_unique<RawEditorFindController>(this);
    rawEditorCompletionController_ = std::make_unique<RawEditorCompletionController>(this);
    editorModeController_ = std::make_unique<TextEditorModeController>(this);
    sourceRewriteController_ = std::make_unique<TextEditorSourceRewriteController>(this);
    statusController_ = std::make_unique<TextEditorStatusController>(this);
    blockDetailsOptionArgsController_ = std::make_unique<BlockEditorOptionArgsController>(this);
    blockDetailsHelpController_ = std::make_unique<BlockEditorDetailsHelpController>(this);
    blockDetailsLineBuildService_ = std::make_unique<BlockEditorLineBuildService>(this);
    blockDetailsApplyStateController_ = std::make_unique<BlockEditorApplyStateController>(this);
    blockDetailsApplyExecutor_ = std::make_unique<BlockEditorApplyExecutor>(this);
    blockDetailsSelectionDetailsController_ = std::make_unique<BlockEditorSelectionDetailsController>(this);
    blockDetailsPaneController_ = std::make_unique<BlockEditorDetailsPaneController>(this);
    blockToolboxController_ = std::make_unique<BlockEditorToolboxController>(this);
    blockDetailsToolboxDetailsController_ = std::make_unique<BlockEditorToolboxDetailsController>(this);
    blockDetailsCanvasSelectionController_ = std::make_unique<BlockEditorCanvasSelectionController>(this);
    blockDetailsMovePreviewController_ = std::make_unique<BlockEditorMovePreviewController>(this);
    blockDetailsCanvasBoundaryController_ = std::make_unique<BlockEditorCanvasBoundaryController>(this);
    blockDetailsCanvasRebuildController_ = std::make_unique<BlockEditorCanvasRebuildController>(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildRawEditorPanel();

    modeRow_ = new QWidget(this);
    auto *modeLayout = new QHBoxLayout(modeRow_);
    modeLayout->setContentsMargins(kPanelPadding, kPanelSpacing, kPanelPadding, kPanelSpacing);
    modeLayout->setSpacing(kPanelSpacing);
    modeLayout->addWidget(new QLabel(tr("Mode:"), modeRow_));
    rawModeButton_ = new QPushButton(tr("Raw"), modeRow_);
    rawModeButton_->setCheckable(true);
    blocksModeButton_ = new QPushButton(tr("Blocks"), modeRow_);
    blocksModeButton_->setCheckable(true);
    blocksModeButton_->setToolTip(tr("Structured block canvas for .th and .thconfig files."));
    modeLayout->addWidget(rawModeButton_);
    modeLayout->addWidget(blocksModeButton_);
    modeLayout->addStretch(1);
    modeRow_->setMaximumHeight(modeSelectorRequestedVisible_ ? QWIDGETSIZE_MAX : 0);

    buildBlockEditorPanel();

    editorModeStack_ = new QStackedWidget(this);
    editorModeStack_->addWidget(rawEditorPanel_);
    editorModeStack_->addWidget(blocksPanel_);

    auto *toolbarSeparator = new QFrame(this);
    toolbarSeparator->setFrameShape(QFrame::NoFrame);
    toolbarSeparator->setFixedHeight(1);
    toolbarSeparator->setStyleSheet(QStringLiteral("background-color: palette(mid);"));

    layout->addWidget(searchBar_);
    layout->addWidget(modeRow_);
    layout->addWidget(toolbarSeparator);
    layout->addWidget(editorModeStack_, 1);
    layout->addWidget(statusRow_);

    connect(editor_, &QPlainTextEdit::textChanged, this, &TextEditorTab::handleTextChanged);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &TextEditorTab::handleCursorPositionChanged);
    connect(rawModeButton_, &QPushButton::clicked, this, &TextEditorTab::handleRawModeRequested);
    connect(blocksModeButton_, &QPushButton::clicked, this, &TextEditorTab::handleBlocksModeRequested);

    refreshBlocksModeAvailability();
    refreshEditorModeUi();
    rebuildBlocksCanvasFromText();
    clearBlockDetailsPane();
    refreshStatus();
    refreshCurrentLineHighlight();
    updateContextHelp();

    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            handleApplicationAppearanceChanged();
        });
    }
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }
    handleApplicationAppearanceChanged();
}

TextEditorTab::~TextEditorTab()
{
    tearingDown_ = true;
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
}

bool TextEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    QString contents;
    QString loadedEncoding = QStringLiteral("UTF-8");
    QString loadedEncodingLabel;
    if (!DocumentFile::readTextFile(filePath, &contents, &loadedEncoding, &loadedEncodingLabel, errorMessage)) {
        return false;
    }

    filePath_ = filePath;
    fileEncodingName_ = loadedEncoding.trimmed().isEmpty() ? QStringLiteral("UTF-8") : loadedEncoding.trimmed();
    fileEncodingLabel_ = loadedEncodingLabel.isEmpty() ? QStringLiteral("UTF-8") : loadedEncodingLabel;
    if (fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        encodingStatusNote_.clear();
    } else {
        encodingStatusNote_ = tr("Opened as %1. Save keeps this encoding unless you convert to UTF-8.")
                                  .arg(fileEncodingLabel_);
    }
    loading_ = true;
    editor_->setPlainText(contents);
    loading_ = false;
    const QTextCursor cursor = editor_->textCursor();
    currentLineNumber_ = cursor.blockNumber() + 1;
    currentColumnNumber_ = cursor.positionInBlock() + 1;
    highlightedLineNumber_ = currentLineNumber_;
    cleanTextSnapshot_ = editor_->toPlainText();
    cleanEncodingNameSnapshot_ = fileEncodingName_;
    editor_->document()->setModified(false);
    dirty_ = false;
    refreshBlocksModeAvailability();
    if (blocksModeActive_ && !isBlocksModeSupportedForCurrentFile()) {
        setBlocksModeActive(false);
    }
    blockDetailsSelectedLineNumber_ = 0;
    blockDetailsSelectedKind_.clear();
    rebuildBlocksCanvasFromText();
    clearBlockDetailsPane();
    populateBlockToolbox();
    refreshEditorModeUi();
    refreshTitle();
    refreshCurrentLineHighlight();
    emit dirtyStateChanged(false);
    updateContextHelp();
    return true;
}

void TextEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    refreshStatus();
}

bool TextEditorTab::save(QString *errorMessage)
{
    if (filePath_.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("This document does not have a file path yet.");
        }
        return false;
    }

    if (!DocumentFile::writeTextFile(filePath_, editor_->toPlainText(), fileEncodingName_, errorMessage)) {
        return false;
    }

    cleanTextSnapshot_ = editor_->toPlainText();
    cleanEncodingNameSnapshot_ = fileEncodingName_;
    editor_->document()->setModified(false);
    dirty_ = false;
    if (fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        encodingStatusNote_.clear();
    } else {
        encodingStatusNote_ = tr("Saved using %1 encoding.").arg(fileEncodingLabel_);
    }
    refreshTitle();
    emit dirtyStateChanged(false);
    return true;
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

bool TextEditorTab::rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteStructureEntryName(lineNumber, category, newName, errorMessage);
}

bool TextEditorTab::insertScrapBlock(const QString &preferredName,
                                     int *insertedLineNumber,
                                     QString *errorMessage,
                                     const QString &options)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertScrapBlock(preferredName, insertedLineNumber, errorMessage, options);
}

bool TextEditorTab::insertDraftGeometry(const QString &kind,
                                        const QVector<QPointF> &vertices,
                                        int *insertedLineNumber,
                                        QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftGeometry(kind, vertices, insertedLineNumber, errorMessage);
}

bool TextEditorTab::insertDraftLineGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftLineGeometry(coordinateRows, insertedLineNumber, errorMessage);
}

bool TextEditorTab::insertDraftAreaGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftAreaGeometry(coordinateRows, insertedLineNumber, errorMessage);
}

bool TextEditorTab::rewritePointCoordinates(int lineNumber,
                                            const QPointF &point,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewritePointCoordinates(lineNumber, point, errorMessage);
}

bool TextEditorTab::rewriteLineAreaVertex(int lineNumber,
                                          const QString &kind,
                                          int vertexIndex,
                                          const QPointF &point,
                                          QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLineAreaVertex(lineNumber, kind, vertexIndex, point, errorMessage);
}

bool TextEditorTab::rewriteLineOptionToggle(int lineNumber,
                                            const QString &optionName,
                                            bool enabled,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLineOptionToggle(lineNumber, optionName, enabled, errorMessage);
}

bool TextEditorTab::rewritePointOrientation(int lineNumber,
                                            bool enabled,
                                            qreal orientationDegrees,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewritePointOrientation(lineNumber, enabled, orientationDegrees, errorMessage);
}

bool TextEditorTab::rewriteLinePointOrientation(int lineNumber,
                                                int sourceVertexIndex,
                                                bool enabled,
                                                qreal orientationDegrees,
                                                QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLinePointOrientation(lineNumber,
                                                                 sourceVertexIndex,
                                                                 enabled,
                                                                 orientationDegrees,
                                                                 errorMessage);
}

bool TextEditorTab::rewriteLinePointLeftSize(int lineNumber,
                                             int sourceVertexIndex,
                                             bool enabled,
                                             qreal sizeValue,
                                             QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLinePointLeftSize(lineNumber,
                                                              sourceVertexIndex,
                                                              enabled,
                                                              sizeValue,
                                                              errorMessage);
}

bool TextEditorTab::rewriteLineCoordinateRows(int lineNumber,
                                              const QStringList &coordinateRows,
                                              QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLineCoordinateRows(lineNumber, coordinateRows, errorMessage);
}

bool TextEditorTab::configureCommandAtLine(const QString &kind, int lineNumber)
{
    if (lineNumber <= 0 || editor_ == nullptr) {
        return false;
    }

    handleBlockConfigureRequest(kind, lineNumber);
    return true;
}

bool TextEditorTab::deleteCommandAtLine(int lineNumber)
{
    return handleBlockDeleteRequest(lineNumber);
}

void TextEditorTab::replaceTextForCommand(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->replaceTextForCommand(contents);
    }
}

QString TextEditorTab::filePath() const
{
    return filePath_;
}

QString TextEditorTab::displayName() const
{
    return statusController_ != nullptr ? statusController_->displayName() : tr("Untitled");
}

bool TextEditorTab::isDirty() const
{
    return dirty_;
}

int TextEditorTab::currentLineNumber() const
{
    return cursorController_ != nullptr ? cursorController_->currentLineNumber() : 1;
}

int TextEditorTab::currentColumnNumber() const
{
    return cursorController_ != nullptr ? cursorController_->currentColumnNumber() : 1;
}

QString TextEditorTab::text() const
{
    return editor_->toPlainText();
}

QColor TextEditorTab::sourceSurfaceColor() const
{
    if (appearanceController_ == nullptr) {
        return palette().color(QPalette::Window);
    }
    return appearanceController_->sourceSurfaceColor();
}

QString TextEditorTab::statusPathText() const
{
    return statusController_ != nullptr ? statusController_->statusPathText() : tr("No file open");
}

QString TextEditorTab::statusEncodingText() const
{
    return statusController_ != nullptr ? statusController_->statusEncodingText() : QStringLiteral("UTF-8");
}

bool TextEditorTab::canUndo() const
{
    return editor_ != nullptr && editor_->document() != nullptr && editor_->document()->isUndoAvailable();
}

bool TextEditorTab::canRedo() const
{
    return editor_ != nullptr && editor_->document() != nullptr && editor_->document()->isRedoAvailable();
}

void TextEditorTab::triggerUndo()
{
    if (editor_ != nullptr) {
        editor_->undo();
    }
}

void TextEditorTab::triggerRedo()
{
    if (editor_ != nullptr) {
        editor_->redo();
    }
}

void TextEditorTab::setInlineStatusVisible(bool visible)
{
    if (statusController_ != nullptr) {
        statusController_->setInlineStatusVisible(visible);
    }
}

void TextEditorTab::setModeSelectorVisible(bool visible)
{
    modeSelectorRequestedVisible_ = visible;
    if (modeRow_ != nullptr) {
        modeRow_->setVisible(modeSelectorRequestedVisible_);
        modeRow_->setMaximumHeight(modeSelectorRequestedVisible_ ? QWIDGETSIZE_MAX : 0);
        if (!modeSelectorRequestedVisible_) {
            modeRow_->setMinimumHeight(0);
        } else {
            modeRow_->setMinimumHeight(modeRow_->sizeHint().height());
        }
    }
    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        rootLayout->invalidate();
        rootLayout->activate();
    }
}

TextEditorTab::EditorMode TextEditorTab::editorMode() const
{
    return blocksModeActive_ ? EditorMode::Blocks : EditorMode::Raw;
}

bool TextEditorTab::isBlocksModeAvailable() const
{
    return isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::setEditorMode(EditorMode mode)
{
    setBlocksModeActive(mode == EditorMode::Blocks);
}

bool TextEditorTab::isBlocksModeSupportedForCurrentFile() const
{
    return editorModeController_ != nullptr
        && editorModeController_->isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::refreshBlocksModeAvailability()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshBlocksModeAvailability();
    }
}

void TextEditorTab::setBlocksModeActive(bool active)
{
    if (editorModeController_ != nullptr) {
        editorModeController_->setBlocksModeActive(active);
    }
}

bool TextEditorTab::ensureEncodingRootDirectiveForBlocks()
{
    return editorModeController_ != nullptr
        && editorModeController_->ensureEncodingRootDirectiveForBlocks();
}

void TextEditorTab::refreshEditorModeUi()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshEditorModeUi();
    }
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
    if (blockDetailsMovePreviewController_ != nullptr) {
        blockDetailsMovePreviewController_->updateMovePreview(sourceLineNumber, scenePos);
    }
}

void TextEditorTab::clearBlockMovePreview()
{
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
    BlockEditorInsertionController(this).insertFromDrop(kind, scenePos);
}

void TextEditorTab::handleBlockMoveRequest(int lineNumber, const QPointF &scenePos)
{
    BlockEditorMoveController(this).moveBlock(lineNumber, scenePos);
}

void TextEditorTab::handleBlockConfigureRequest(const QString &kind, int lineNumber)
{
    BlockEditorConfigureController(this).configureBlock(kind, lineNumber);
}

bool TextEditorTab::handleBlockDeleteRequest(int lineNumber)
{
    BlockEditorDeleteExecutor deleteExecutor(this);
    return deleteExecutor.deleteCommandAtLine(lineNumber);
}

bool TextEditorTab::isCurrentStateDirty() const
{
    return statusController_ != nullptr && statusController_->isCurrentStateDirty();
}

void TextEditorTab::applyDirtyStateFromCurrentState()
{
    if (statusController_ != nullptr) {
        statusController_->applyDirtyStateFromCurrentState();
    }
}

void TextEditorTab::handleTextChanged()
{
    if (loading_) {
        return;
    }

    rebuildBlocksCanvasFromText();
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
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

void TextEditorTab::handleRawModeRequested()
{
    setBlocksModeActive(false);
}

void TextEditorTab::handleBlocksModeRequested()
{
    setBlocksModeActive(true);
}

void TextEditorTab::handleConvertToUtf8Triggered()
{
    if (encodingController_ != nullptr) {
        encodingController_->handleConvertToUtf8Triggered();
    }
}

void TextEditorTab::handleCursorPositionChanged()
{
    if (cursorController_ != nullptr) {
        cursorController_->handleCursorPositionChanged();
    }
}

void TextEditorTab::refreshCurrentLineHighlight()
{
    if (cursorController_ != nullptr) {
        cursorController_->refreshCurrentLineHighlight();
    }
}

void TextEditorTab::refreshTitle()
{
    if (statusController_ != nullptr) {
        statusController_->refreshTitle();
    }
}

void TextEditorTab::refreshStatus()
{
    if (statusController_ != nullptr) {
        statusController_->refreshStatus();
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

bool TextEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && watched == qApp) {
        switch (event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            handleApplicationAppearanceChanged();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (rawEditorCompletionController_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    switch (rawEditorCompletionController_->handleEventFilter(watched, event)) {
    case RawEditorCompletionController::EventHandling::Consumed:
        return true;
    case RawEditorCompletionController::EventHandling::PassToBase:
        return QWidget::eventFilter(watched, event);
    case RawEditorCompletionController::EventHandling::NotHandled:
    default:
        return QWidget::eventFilter(watched, event);
    }
}

QString TextEditorTab::currentCompletionPrefix() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->currentCompletionPrefix();
}

QString TextEditorTab::normalizeCompletionContext(const QString &contextToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return normalizedCompletionContextToken(contextToken);
    }
    return rawEditorCompletionController_->normalizeCompletionContext(contextToken);
}

bool TextEditorTab::isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->isCompatibleChildKindForBlocks(parentKind, childKind);
}

bool TextEditorTab::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->isCommandDirectiveInScope(directive, scopeToken);
}

QStringList TextEditorTab::commandArgumentSignaturesFor(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return {};
    }
    return rawEditorCompletionController_->commandArgumentSignaturesFor(commandToken);
}

bool TextEditorTab::commandHasRequiredIdArgument(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->commandHasRequiredIdArgument(commandToken);
}

bool TextEditorTab::commandSupportsInlineIdField(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->commandSupportsInlineIdField(commandToken);
}

QString TextEditorTab::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->primaryInsertionScopeForCommand(commandToken);
}

QString TextEditorTab::resolveScopeForCommandAtLine(const QString &commandToken,
                                                    const QStringList &lines,
                                                    int lineNumber) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
}

QStringList TextEditorTab::activeCompletionScopeStack() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QStringList();
    }
    return rawEditorCompletionController_->activeCompletionScopeStack();
}

QString TextEditorTab::currentCompletionCommand() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->currentCompletionCommand();
}

QString TextEditorTab::currentCompletionScopeLabel() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return tr("top-level");
    }
    return rawEditorCompletionController_->currentCompletionScopeLabel();
}

QStringList TextEditorTab::projectInputFileCompletionCandidates() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return {};
    }
    return rawEditorCompletionController_->projectInputFileCompletionCandidates();
}

QStringList TextEditorTab::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QStringList();
    }
    return rawEditorCompletionController_->buildCompletionSuggestionsForCursor(prefix);
}

void TextEditorTab::triggerCompletionPopup()
{
    if (rawEditorCompletionController_ == nullptr) {
        return;
    }
    rawEditorCompletionController_->triggerCompletionPopup();
}

void TextEditorTab::insertCompletionToken(const QString &completion)
{
    if (rawEditorCompletionController_ == nullptr) {
        return;
    }
    rawEditorCompletionController_->insertCompletionToken(completion);
}

QString TextEditorTab::normalizedDirectiveToken(const QString &directive) const
{
    return normalizeDirective(directive);
}

QString TextEditorTab::openingDirectiveForClosingToken(const QString &directive) const
{
    return completionOpeningDirectiveForClosing(directive);
}

QString TextEditorTab::closingDirectiveForOpeningToken(const QString &directive) const
{
    return completionClosingDirectiveForOpening(directive);
}

bool TextEditorTab::isContainerDirectiveInstanceForParsedLine(const QString &directive,
                                                              const TherionParsedLine &parsedLine) const
{
    return isContainerDirectiveInstance(directive, parsedLine);
}

void TextEditorTab::updateValidationTooltipForCursor()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->updateValidationTooltipForCursor();
    }
}

void TextEditorTab::updateContextHelp()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->updateContextHelp();
    }
}

void TextEditorTab::setHelpCollapsed(bool collapsed)
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->setHelpCollapsed(collapsed);
    }
}

QString TextEditorTab::displayPath() const
{
    return statusController_ != nullptr ? statusController_->displayPath() : tr("No file open");
}

}
