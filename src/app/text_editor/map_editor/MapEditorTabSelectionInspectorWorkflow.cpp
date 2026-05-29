#include "MapEditorTab.h"

#include "MapEditorSceneSupport.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionBackgroundMetadata.h"

#include <QGraphicsView>
#include <QScopedValueRollback>

namespace TherionStudio
{
void MapEditorTab::handleAddPointTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Point);
    beginPendingInsertObject(QStringLiteral("point"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Point mode: click in map to insert a point.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Line);
    beginPendingInsertObject(QStringLiteral("line"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Line mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Freehand);
    beginPendingInsertObject(QStringLiteral("line"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Freehand mode: drag in map to draw a line, then release to finish.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Area);
    beginPendingInsertObject(QStringLiteral("area"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Area mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::None);
    clearPendingInsertObject();
    refreshObjectDetailsPanel();
    selectModeActive_ = true;
    toolbarStatusNote_ = tr("Selection mode: select map cards or draft objects.");
    if (mapView_ != nullptr) {
        mapView_->setDragMode(QGraphicsView::NoDrag);
    }
    refreshToolbarSummary();
}

void MapEditorTab::handleInsertScrapTriggered()
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Insert Scrap failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    lineExtensionActive_ = false;
    if (interactiveDrawMode_ != InteractiveDrawMode::None) {
        setInteractiveDrawMode(InteractiveDrawMode::None);
    }
    selectModeActive_ = true;

    if (pendingInsertFields_.commandKind.trimmed().toLower() != QStringLiteral("scrap")) {
        beginPendingInsertObject(QStringLiteral("scrap"));
        activateSelectionInspector();
        refreshObjectDetailsPanel();
        toolbarStatusNote_ = tr("Scrap insert: set ID/projection in Selection, then click Insert Scrap again.");
        refreshToolbarSummary();
        return;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    const QString scrapScaleOption = xtherionDefaultScrapScaleOption(mapSourceBoundsForCurrentDocument());
    if (!textEditor_->insertScrapBlock(pendingScrapPreferredName(),
                                       &insertedLineNumber,
                                       &errorMessage,
                                       pendingScrapOptions(scrapScaleOption))) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Insert Scrap failed.")
            : tr("Insert Scrap failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }
    recordSourceTextSnapshot(tr("Insert Scrap"), beforeText, textEditor_->text(), insertedLineNumber);

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Inserted scrap block at line %1.").arg(insertedLineNumber)
        : tr("Inserted scrap block.");
    clearPendingInsertObject();
    refreshObjectDetailsPanel();
    refreshToolbarSummary();
}

void MapEditorTab::handleCompleteDraftTriggered()
{
    if (commitInteractiveDrawSession()) {
        return;
    }

    auto *draftRectItem = selectedDraftGeometryItem();
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(draftRectItem);
    if (draftItem == nullptr || draftItem->isDraftComplete()) {
        return;
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    const QVector<QPointF> vertices = sourceVerticesForDraft(draftItem);
    if (vertices.isEmpty()) {
        toolbarStatusNote_ = tr("Complete Draft failed: unable to resolve draft geometry coordinates.");
        refreshToolbarSummary();
        return;
    }

    QString geometryKind;
    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        geometryKind = QStringLiteral("point");
        break;
    case DraftGeometryKind::Line:
        geometryKind = QStringLiteral("line");
        break;
    case DraftGeometryKind::Area:
        geometryKind = QStringLiteral("area");
        break;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    if (!textEditor_->insertDraftGeometry(geometryKind,
                                          vertices,
                                          &insertedLineNumber,
                                          &errorMessage,
                                          pendingDraftObjectOptions(geometryKind))) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }
    const QString geometryLabel = draftGeometryLabel(draftItem->kind());
    recordDraftCompletion(draftRectItem, tr("Complete Draft"), beforeText, textEditor_->text(), insertedLineNumber);
    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(geometryLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(geometryLabel);
    refreshToolbarSummary();
    updateHelpPanel();
}
}
