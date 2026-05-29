#include "MapEditorTab.h"

#include "MapEditorSceneSupport.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionBackgroundMetadata.h"

#include <QScopedValueRollback>

namespace TherionStudio
{
QVector<TherionParsedLine> MapEditorTab::parsedLinesForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return {};
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedParsedLinesValid_ && cachedParsedLinesRevision_ == currentRevision) {
        return cachedParsedLines_;
    }

    cachedParsedLines_ = TherionDocumentParser::parseText(textEditor_->text());
    cachedParsedLinesRevision_ = currentRevision;
    cachedParsedLinesValid_ = true;
    return cachedParsedLines_;
}

std::optional<MapEditorInteractiveLineControlHandleRef> MapEditorTab::interactiveLineControlAt(
    const QPointF &scenePosition,
    qreal sceneRadius) const
{
    return TherionStudio::interactiveLineControlAt(interactiveDrawState_.lineVertices_, scenePosition, sceneRadius);
}

QRectF MapEditorTab::mapSourceBoundsForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return QRectF();
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedMapSourceBoundsValid_ && cachedMapSourceBoundsRevision_ == currentRevision) {
        return cachedMapSourceBounds_;
    }

    const QString currentText = textEditor_->text();
    QRectF resolvedBounds;
    const TherionAreaAdjust areaAdjust = parseTherionAreaAdjust(currentText);
    if (areaAdjust.valid && areaAdjust.modelRect.isValid()) {
        resolvedBounds = areaAdjust.modelRect;
    } else {
        const QVector<TherionParsedLine> parsedLines = parsedLinesForCurrentDocument();
        const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
        resolvedBounds = geometryBoundsForFeatures(features);
    }

    cachedMapSourceBoundsValid_ = true;
    cachedMapSourceBoundsRevision_ = currentRevision;
    cachedMapSourceBounds_ = resolvedBounds;
    return cachedMapSourceBounds_;
}

QPointF MapEditorTab::sourcePointFromScenePosition(const QPointF &scenePosition) const
{
    if (textEditor_ == nullptr) {
        return scenePosition;
    }

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid()) {
        return scenePosition;
    }

    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    if (!sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return scenePosition;
    }

    return previewToSourcePoint(scenePosition, sourceBounds, previewBounds);
}

bool MapEditorTab::hasCompletableInteractiveDrawSession() const
{
    if (interactiveDrawState_.lineExtensionActive_) {
        return interactiveDrawState_.mode_ == InteractiveDrawMode::Line && interactiveDrawState_.lineVertices_.size() >= 2;
    }
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::Line) {
        return interactiveDrawState_.lineVertices_.size() >= 2;
    }
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::Area) {
        return interactiveDrawState_.lineVertices_.size() >= 3;
    }
    return false;
}

QStringList MapEditorTab::lineCoordinateRowsForInteractiveDraft() const
{
    return TherionStudio::lineCoordinateRowsForInteractiveDraft(interactiveDrawState_.lineVertices_);
}

QStringList MapEditorTab::areaCoordinateRowsForInteractiveDraft() const
{
    return TherionStudio::areaCoordinateRowsForInteractiveDraft(interactiveDrawState_.lineVertices_);
}

void MapEditorTab::captureInteractiveLineAnchor(const QPointF &anchorScenePoint,
                                                const std::optional<QPointF> &dragScenePoint)
{
    TherionStudio::captureInteractiveLineAnchor(&interactiveDrawState_.lineVertices_,
                                                anchorScenePoint,
                                                sourcePointFromScenePosition(anchorScenePoint),
                                                dragScenePoint,
                                                [this](const QPointF &scenePoint) {
                                                    return sourcePointFromScenePosition(scenePoint);
                                                });
    updateInteractiveDrawPreview();
}

bool MapEditorTab::setInteractiveLineControlScenePoint(const MapEditorInteractiveLineControlHandleRef &handle,
                                                       const QPointF &scenePoint)
{
    return TherionStudio::setInteractiveLineControlScenePoint(&interactiveDrawState_.lineVertices_,
                                                              handle,
                                                              scenePoint,
                                                              [this](const QPointF &scenePointToMap) {
                                                                  return sourcePointFromScenePosition(scenePointToMap);
                                                              });
}

bool MapEditorTab::commitInteractiveDrawVertices(const QString &geometryKind,
                                                 const QVector<QPointF> &vertices,
                                                 const QString &successLabel)
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        return false;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    const bool inserted = geometryKind.trimmed().compare(QStringLiteral("line"), Qt::CaseInsensitive) == 0
        ? textEditor_->insertDraftLineGeometry(bezierLineCoordinateRowsForFreehandStroke(vertices),
                                               &insertedLineNumber,
                                               &errorMessage,
                                               QString(),
                                               pendingDraftObjectOptions(QStringLiteral("line")))
        : textEditor_->insertDraftGeometry(geometryKind,
                                           vertices,
                                           &insertedLineNumber,
                                           &errorMessage,
                                           pendingDraftObjectOptions(geometryKind));
    if (!inserted) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }
    recordSourceTextSnapshot(tr("Complete Draft"), beforeText, textEditor_->text(), insertedLineNumber);

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(successLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(successLabel);
    return true;
}

QPointF MapEditorTab::scenePointFromSourcePosition(const QPointF &sourcePosition) const
{
    const QRectF previewBounds = mapPreviewBounds();
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    if (!previewBounds.isValid()
        || !sourceBounds.isValid()
        || sourceBounds.width() < 0.001
        || sourceBounds.height() < 0.001) {
        return sourcePosition;
    }
    return mapGeometryPointToPreview(sourcePosition, sourceBounds, previewBounds);
}
}
