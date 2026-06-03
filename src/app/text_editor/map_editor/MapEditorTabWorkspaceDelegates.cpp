#include "MapEditorTab.h"

#include <QFileInfo>
#include <QFrame>
#include <QLayout>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSplitter>
#include <QUndoStack>

#include "../TextEditorTab.h"

namespace TherionStudio
{
bool MapEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    const QString currentFilePath = this->filePath();
    if (!currentFilePath.isEmpty() && QFileInfo(currentFilePath).canonicalFilePath() != QFileInfo(filePath).canonicalFilePath()) {
        clearDraftGeometryItems();
        clearBackgroundImageItems();
    }

    const bool loaded = textEditor_->loadFile(filePath, errorMessage);
    if (!loaded) {
        return false;
    }

    refreshMapScene();
    loadBackgroundLayersFromSession();
    loadBackgroundLayersFromDocumentMetadata();
    fitMapToView();
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshTitle();
    refreshStatus();
    return true;
}

bool MapEditorTab::save(QString *errorMessage)
{
    return textEditor_->save(errorMessage);
}

void MapEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    textEditor_->setProjectRootPath(projectRootPath);
    refreshStatus();
}

void MapEditorTab::showFindBar(bool replaceMode)
{
    textEditor_->showFindBar(replaceMode);
}

void MapEditorTab::hideFindBar()
{
    textEditor_->hideFindBar();
}

void MapEditorTab::goToLine(int lineNumber)
{
    textEditor_->goToLine(lineNumber);
}

QString MapEditorTab::filePath() const
{
    return textEditor_->filePath();
}

QString MapEditorTab::displayName() const
{
    return textEditor_->displayName();
}

bool MapEditorTab::isDirty() const
{
    return textEditor_->isDirty();
}

int MapEditorTab::currentLineNumber() const
{
    return textEditor_->currentLineNumber();
}

QString MapEditorTab::text() const
{
    return textEditor_ != nullptr ? textEditor_->text() : QString();
}

QString MapEditorTab::statusPathText() const
{
    return textEditor_ != nullptr ? textEditor_->statusPathText() : tr("No file open");
}

QString MapEditorTab::statusEncodingText() const
{
    return textEditor_ != nullptr ? textEditor_->statusEncodingText() : tr("UTF-8");
}

QString MapEditorTab::statusModeText() const
{
    return interactiveDrawState_.mode_ == InteractiveDrawMode::None
        ? tr("Map mode: Select")
        : tr("Map mode: Insert");
}

int MapEditorTab::zoomPercent() const
{
    return qRound(zoomFactor_ * 100.0);
}

bool MapEditorTab::canUndo() const
{
    return (undoStack_ != nullptr && undoStack_->canUndo())
        || (textEditor_ != nullptr && textEditor_->canUndo());
}

bool MapEditorTab::canRedo() const
{
    return (undoStack_ != nullptr && undoStack_->canRedo())
        || (textEditor_ != nullptr && textEditor_->canRedo());
}

MapEditorTab::InteractiveDrawMode MapEditorTab::interactiveDrawMode() const
{
    return interactiveDrawState_.mode_;
}

bool MapEditorTab::canCompleteDraftAction() const
{
    const bool mapReady = mapScene_ != nullptr;
    return mapReady && (selectedDraftGeometryItem() != nullptr || hasCompletableInteractiveDrawSession());
}

void MapEditorTab::triggerUndo()
{
    handleUndoTriggered();
}

void MapEditorTab::triggerRedo()
{
    handleRedoTriggered();
}

void MapEditorTab::triggerZoomIn()
{
    handleZoomInTriggered();
}

void MapEditorTab::triggerZoomOut()
{
    handleZoomOutTriggered();
}

void MapEditorTab::triggerFit()
{
    handleFitTriggered();
}

void MapEditorTab::triggerFitWithBackground()
{
    handleFitWithBackgroundTriggered();
}

void MapEditorTab::triggerSelectMode()
{
    handleSelectModeTriggered();
}

void MapEditorTab::triggerInsertScrap()
{
    handleInsertScrapTriggered();
}

void MapEditorTab::triggerCompleteDraft()
{
    handleCompleteDraftTriggered();
}

void MapEditorTab::triggerAddPoint()
{
    handleAddPointTriggered();
}

void MapEditorTab::triggerAddLine()
{
    handleAddLineTriggered();
}

void MapEditorTab::triggerAddFreehandLine()
{
    handleAddFreehandLineTriggered();
}

void MapEditorTab::triggerAddArea()
{
    handleAddAreaTriggered();
}

bool MapEditorTab::isInsertModeActive() const
{
    return interactiveDrawState_.mode_ != InteractiveDrawMode::None;
}

bool MapEditorTab::isMapPaneDetached() const
{
    return detachedPaneState_.detached_;
}

QString MapEditorTab::mapPaneWindowActionText() const
{
    return detachedPaneState_.detached_ ? tr("Return Map") : tr("Separate Map");
}

QString MapEditorTab::mapPaneWindowActionToolTip() const
{
    return detachedPaneState_.detached_
        ? tr("Return the map pane from the detached window into this tab.")
        : tr("Open the map pane in a separate window (for multi-monitor workflows).");
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceMode() const
{
    return workspaceMode_;
}

void MapEditorTab::setInlineWorkspaceModeSelectorVisible(bool visible)
{
    detachedPaneState_.inlineWorkspaceModeSelectorVisible_ = visible;
    if (workspaceModeRow_ != nullptr) {
        workspaceModeRow_->setVisible(detachedPaneState_.inlineWorkspaceModeSelectorVisible_);
        workspaceModeRow_->setMaximumHeight(detachedPaneState_.inlineWorkspaceModeSelectorVisible_ ? QWIDGETSIZE_MAX : 0);
        if (!detachedPaneState_.inlineWorkspaceModeSelectorVisible_) {
            workspaceModeRow_->setMinimumHeight(0);
        } else {
            workspaceModeRow_->setMinimumHeight(workspaceModeRow_->sizeHint().height());
        }
    }
    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        rootLayout->invalidate();
        rootLayout->activate();
    }
}

void MapEditorTab::setWorkspaceMode(WorkspaceMode mode)
{
    if (workspaceMode_ == mode) {
        refreshWorkspaceModeUi();
        return;
    }

    workspaceMode_ = mode;
    refreshWorkspaceModeUi();
    updateWorkspaceVisibility();
    emit workspaceModeChanged(workspaceMode_);
}

void MapEditorTab::refreshWorkspaceModeUi()
{
    if (visualModeButton_ == nullptr || rawModeButton_ == nullptr) {
        return;
    }

    visualModeButton_->setChecked(workspaceMode_ == WorkspaceMode::Visual);
    rawModeButton_->setChecked(workspaceMode_ == WorkspaceMode::Raw);
}

void MapEditorTab::handleTextEditorCurrentLineChanged(int lineNumber)
{
    if (!selectionSyncState_.textNavigationInProgress_) {
        syncMapSelectionFromTextCursor(lineNumber, textEditor_ != nullptr ? textEditor_->currentColumnNumber() : 1);
    }
    syncInspectorObjectSelectionToLine(lineNumber);
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleTextEditorCursorPositionChanged(int lineNumber, int columnNumber)
{
    if (selectionSyncState_.textNavigationInProgress_) {
        return;
    }

    if (lineNumber == selectionSyncState_.lastCursorSyncedLine_ && columnNumber == selectionSyncState_.lastCursorSyncedColumn_) {
        return;
    }

    syncMapSelectionFromTextCursor(lineNumber, columnNumber);
}

void MapEditorTab::handleUndoTriggered()
{
    if (undoStack_ != nullptr && undoStack_->canUndo()) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->undo();
        flushPendingMapSceneRefreshAfterCommand();
        return;
    }
    if (textEditor_ != nullptr && textEditor_->canUndo()) {
        textEditor_->triggerUndo();
    }
}

void MapEditorTab::handleRedoTriggered()
{
    if (undoStack_ != nullptr && undoStack_->canRedo()) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->redo();
        flushPendingMapSceneRefreshAfterCommand();
        return;
    }
    if (textEditor_ != nullptr && textEditor_->canRedo()) {
        textEditor_->triggerRedo();
    }
}

void MapEditorTab::handleZoomInTriggered()
{
    adjustMapZoom(1.2);
}

void MapEditorTab::handleZoomOutTriggered()
{
    adjustMapZoom(1.0 / 1.2);
}

void MapEditorTab::handleFitTriggered()
{
    fitBackgroundRequested_ = false;
    toolbarStatusNote_ = tr("Fit geometry: centered visible map content.");
    fitMapToView();
    refreshToolbarSummary();
}

void MapEditorTab::updateWorkspaceVisibility()
{
    if (textEditor_ == nullptr) {
        refreshStatus();
        return;
    }

    if (detachedPaneState_.detached_) {
        if (workspaceModeRow_ != nullptr) {
            workspaceModeRow_->setEnabled(false);
            workspaceModeRow_->setToolTip(tr("Map pane is detached: raw editor remains in this tab while visual map stays in the detached window."));
        }
        textEditor_->show();
        if (mapPaneContainer_ != nullptr) {
            // Detached window always hosts the visual map pane, even if main-tab mode is Raw.
            mapPaneContainer_->show();
        }
        if (mapPaneTopSeparator_ != nullptr) {
            mapPaneTopSeparator_->hide();
        }
        refreshStatus();
        return;
    }

    if (workspaceModeRow_ != nullptr) {
        workspaceModeRow_->setEnabled(true);
        workspaceModeRow_->setToolTip(QString());
    }

    const bool visualMode = workspaceMode_ == WorkspaceMode::Visual;
    textEditor_->setVisible(!visualMode);
    if (mapPaneContainer_ != nullptr) {
        mapPaneContainer_->setVisible(visualMode);
    }
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->setVisible(visualMode);
    }

    if (splitter_ != nullptr) {
        if (visualMode) {
            splitter_->setSizes(QList<int>{1, 0});
        } else {
            splitter_->setSizes(QList<int>{0, 1});
        }
    }

    refreshStatus();
}

void MapEditorTab::refreshTitle()
{
    emit titleChanged();
}
}
