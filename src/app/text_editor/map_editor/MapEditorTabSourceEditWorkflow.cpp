#include "MapEditorTab.h"

#include "MapEditorSceneSupport.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionBackgroundMetadata.h"

#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QPainterPath>
#include <cmath>

namespace TherionStudio
{
namespace
{
QString plannerSourceWithAreaAdjust(const QString &beforeText, const std::optional<QRectF> &initialAreaAdjustRect)
{
    QString plannerSource = beforeText;
    if (initialAreaAdjustRect.has_value()
        && initialAreaAdjustRect->isValid()
        && !parseTherionAreaAdjust(plannerSource).valid) {
        plannerSource = upsertTherionAreaAdjustMetadata(plannerSource, *initialAreaAdjustRect);
    }
    return plannerSource;
}
}

QVector<TherionParsedLine> MapEditorTab::parsedLinesForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return {};
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedParsedLinesValid_ && cachedParsedLinesRevision_ == currentRevision) {
        return cachedParsedLines_;
    }

    cachedParsedLines_ = TherionDocumentParser::parseTokenLines(textEditor_->text());
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
        resolvedBounds = xtherionAutoAreaAdjustRect();
    }

    cachedMapSourceBoundsValid_ = true;
    cachedMapSourceBoundsRevision_ = currentRevision;
    cachedMapSourceBounds_ = resolvedBounds;
    return cachedMapSourceBounds_;
}

std::optional<QRectF> MapEditorTab::initialAreaAdjustRectForDraftInsertion() const
{
    if (textEditor_ == nullptr) {
        return std::nullopt;
    }

    const TherionAreaAdjust areaAdjust = parseTherionAreaAdjust(textEditor_->text());
    if (areaAdjust.valid && areaAdjust.modelRect.isValid()) {
        return std::nullopt;
    }

    const QRectF autoRect = xtherionAutoAreaAdjustRect();
    if (!autoRect.isValid() || autoRect.width() <= 0.0 || autoRect.height() <= 0.0) {
        return std::nullopt;
    }
    return autoRect;
}

QRectF MapEditorTab::sourceBoundsForInteractiveDraft() const
{
    if (const std::optional<QRectF> initialAreaAdjust = initialAreaAdjustRectForDraftInsertion();
        initialAreaAdjust.has_value()) {
        return *initialAreaAdjust;
    }
    return mapSourceBoundsForCurrentDocument();
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

    const QRectF sourceBounds = sourceBoundsForInteractiveDraft();
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
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::SmartArea) {
        return interactiveDrawState_.smartAreaPreviewActive_;
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
    const bool lineGeometry = geometryKind.trimmed().compare(QStringLiteral("line"), Qt::CaseInsensitive) == 0;
    const QString plannerSource = plannerSourceWithAreaAdjust(beforeText, initialAreaAdjustRectForDraftInsertion());
    QVector<TherionSourceTextEdit> sourceEdits;
    const bool planned = lineGeometry
        ? TherionDocumentEditor::appendDraftLineGeometryEdits(plannerSource,
                                                              bezierLineCoordinateRowsForFreehandStroke(vertices),
                                                              &sourceEdits,
                                                              &insertedLineNumber,
                                                              &errorMessage,
                                                              QString(),
                                                              pendingDraftObjectOptions(QStringLiteral("line")))
        : TherionDocumentEditor::appendDraftGeometryEdits(plannerSource,
                                                          geometryKind,
                                                          vertices,
                                                          &sourceEdits,
                                                          &insertedLineNumber,
                                                          &errorMessage,
                                                          pendingDraftObjectOptions(geometryKind));
    if (!planned) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }

    QString afterText = plannerSource;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }

    bool applied = false;
    applySourceTextChangeWithSnapshot(
        tr("Complete Draft"),
        beforeText,
        afterText,
        insertedLineNumber,
        [this, successLabel, insertedLineNumber, &applied]() {
            applied = true;
            toolbarStatusNote_ = insertedLineNumber > 0
                ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(successLabel, QString::number(insertedLineNumber))
                : tr("Complete Draft wrote %1 geometry to source.").arg(successLabel);
        });
    return applied;
}

bool MapEditorTab::previewSmartAreaAt(const QPointF &scenePosition)
{
    interactiveDrawState_.smartAreaPreviewActive_ = false;
    interactiveDrawState_.smartAreaCandidates_.clear();
    interactiveDrawState_.smartAreaCandidate_ = {};
    interactiveDrawState_.smartAreaCandidateIndex_ = 0;

    if (std::isnan(scenePosition.x()) || std::isnan(scenePosition.y())) {
        if (interactiveDrawState_.previewPath_ != nullptr) {
            interactiveDrawState_.previewPath_->setPath(QPainterPath());
            interactiveDrawState_.previewPath_->setVisible(false);
        }
        return false;
    }

    const QPointF sourcePoint = sourcePointFromScenePosition(scenePosition);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLinesForCurrentDocument());
    const QVector<MapEditorSmartAreaCandidate> candidates = mapEditorSmartAreaCandidatesAt(features, sourcePoint);
    if (candidates.isEmpty()) {
        if (interactiveDrawState_.previewPath_ != nullptr) {
            interactiveDrawState_.previewPath_->setPath(QPainterPath());
            interactiveDrawState_.previewPath_->setVisible(false);
        }
        return false;
    }

    interactiveDrawState_.smartAreaCandidates_ = candidates;
    interactiveDrawState_.smartAreaCandidate_ = interactiveDrawState_.smartAreaCandidates_.first();
    interactiveDrawState_.smartAreaPreviewActive_ = true;

    updateSmartAreaPreviewPath();
    return true;
}

void MapEditorTab::updateSmartAreaPreviewPath()
{
    if (!interactiveDrawState_.smartAreaPreviewActive_
        || interactiveDrawState_.smartAreaCandidates_.isEmpty()
        || interactiveDrawState_.smartAreaCandidateIndex_ < 0
        || interactiveDrawState_.smartAreaCandidateIndex_ >= interactiveDrawState_.smartAreaCandidates_.size()) {
        if (interactiveDrawState_.previewPath_ != nullptr) {
            interactiveDrawState_.previewPath_->setPath(QPainterPath());
            interactiveDrawState_.previewPath_->setVisible(false);
        }
        return;
    }

    interactiveDrawState_.smartAreaCandidate_ =
        interactiveDrawState_.smartAreaCandidates_.at(interactiveDrawState_.smartAreaCandidateIndex_);

    QPainterPath path;
    const QVector<QPointF> &vertices = interactiveDrawState_.smartAreaCandidate_.vertices;
    if (!vertices.isEmpty()) {
        path.moveTo(scenePointFromSourcePosition(vertices.first()));
        for (int index = 1; index < vertices.size(); ++index) {
            path.lineTo(scenePointFromSourcePosition(vertices.at(index)));
        }
        path.closeSubpath();
    }

    if (interactiveDrawState_.previewPath_ == nullptr) {
        interactiveDrawState_.previewPath_ = new QGraphicsPathItem();
        interactiveDrawState_.previewPath_->setAcceptedMouseButtons(Qt::NoButton);
        interactiveDrawState_.previewPath_->setZValue(28.0);
        if (mapScene_ != nullptr) {
            mapScene_->addItem(interactiveDrawState_.previewPath_);
        }
    } else if (mapScene_ != nullptr && interactiveDrawState_.previewPath_->scene() != mapScene_) {
        mapScene_->addItem(interactiveDrawState_.previewPath_);
    }
    interactiveDrawState_.previewPath_->setPath(path);
    interactiveDrawState_.previewPath_->setVisible(true);
    updateInteractiveDrawPreview();
}

bool MapEditorTab::hasSmartAreaPreview() const
{
    return interactiveDrawState_.smartAreaPreviewActive_;
}

bool MapEditorTab::cycleSmartAreaCandidate(int delta)
{
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::SmartArea
        || !interactiveDrawState_.smartAreaPreviewActive_
        || interactiveDrawState_.smartAreaCandidates_.size() <= 1) {
        return false;
    }

    const int count = interactiveDrawState_.smartAreaCandidates_.size();
    interactiveDrawState_.smartAreaCandidateIndex_ =
        (interactiveDrawState_.smartAreaCandidateIndex_ + delta + count) % count;
    updateSmartAreaPreviewPath();
    toolbarStatusNote_ = tr("Smart Area candidate %1 of %2 selected. Press Enter or Complete Draft to insert.")
        .arg(interactiveDrawState_.smartAreaCandidateIndex_ + 1)
        .arg(count);
    refreshToolbarSummary();
    updateCommandSurfaceState();
    return true;
}

bool MapEditorTab::commitSmartAreaPreview()
{
    if (textEditor_ == nullptr || !interactiveDrawState_.smartAreaPreviewActive_) {
        return false;
    }

    QVector<TherionReferencedAreaBoundaryLine> boundaryLines;
    boundaryLines.reserve(interactiveDrawState_.smartAreaCandidate_.boundaryLines.size());
    for (const MapEditorSmartAreaBoundaryLine &line : std::as_const(interactiveDrawState_.smartAreaCandidate_.boundaryLines)) {
        boundaryLines.append(TherionReferencedAreaBoundaryLine{line.lineNumber, line.identifier});
    }

    const QString beforeText = textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    int insertedLineNumber = 0;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::appendReferencedAreaEdits(beforeText,
                                                         interactiveDrawState_.smartAreaCandidate_.scrapLineNumber,
                                                         boundaryLines,
                                                         &sourceEdits,
                                                         &insertedLineNumber,
                                                         &errorMessage,
                                                         pendingDraftObjectOptions(QStringLiteral("area")))
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Smart Area insert failed.")
            : tr("Smart Area insert failed: %1").arg(errorMessage);
        return false;
    }

    bool applied = false;
    applySourceTextChangeWithSnapshot(
        tr("Insert Smart Area"),
        beforeText,
        afterText,
        insertedLineNumber,
        [this, insertedLineNumber, &applied]() {
            applied = true;
            toolbarStatusNote_ = insertedLineNumber > 0
                ? tr("Smart Area inserted at source line %1.").arg(insertedLineNumber)
                : tr("Smart Area inserted.");
        });
    return applied;
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
