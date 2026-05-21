#include "MapEditorInteractiveDrawController.h"

#include "../TextEditorTab.h"
#include "MapEditorSceneSupport.h"

#include <QBrush>
#include <QColor>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QScopedValueRollback>

namespace TherionStudio
{
MapEditorInteractiveDrawController::MapEditorInteractiveDrawController(MapEditorTab *owner)
    : owner_(owner)
{
}

void MapEditorInteractiveDrawController::setInteractiveDrawMode(MapEditorTab::InteractiveDrawMode mode)
{
    const MapEditorTab::InteractiveDrawMode previousMode = owner_->interactiveDrawMode_;
    clearInteractiveDrawSession(false);
    owner_->interactiveDrawMode_ = mode;
    owner_->selectModeActive_ = mode == MapEditorTab::InteractiveDrawMode::None;
    if (previousMode != owner_->interactiveDrawMode_) {
        emit owner_->modeStatusChanged();
    }
    owner_->updateCommandSurfaceState();
}

bool MapEditorInteractiveDrawController::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::None) {
        return false;
    }

    if (owner_->textEditor_ == nullptr) {
        owner_->toolbarStatusNote_ = owner_->tr("Drawing failed: no active TH2 text editor.");
        owner_->refreshToolbarSummary();
        return true;
    }

    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Point) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QVector<QPointF> vertices{owner_->sourcePointFromScenePosition(scenePosition)};
        const QString beforeText = owner_->textEditor_->text();
        const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
        if (!owner_->textEditor_->insertDraftGeometry(QStringLiteral("point"), vertices, &insertedLineNumber, &errorMessage)) {
            owner_->toolbarStatusNote_ = errorMessage.isEmpty()
                ? owner_->tr("Point insert failed.")
                : owner_->tr("Point insert failed: %1").arg(errorMessage);
        } else {
            owner_->recordSourceTextSnapshot(owner_->tr("Insert Point"), beforeText, owner_->textEditor_->text(), insertedLineNumber);
            owner_->toolbarStatusNote_ = insertedLineNumber > 0
                ? owner_->tr("Inserted point at source line %1.").arg(insertedLineNumber)
                : owner_->tr("Inserted point.");
        }
        owner_->refreshToolbarSummary();
        return true;
    }

    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area) {
        owner_->captureInteractiveLineAnchor(scenePosition, std::nullopt);
        owner_->interactiveDrawHoverActive_ = false;
        owner_->toolbarStatusNote_ = owner_->tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                                 .arg(owner_->interactiveDrawLineVertices_.size());
        owner_->refreshToolbarSummary();
        owner_->updateCommandSurfaceState();
        return true;
    }

    return false;
}

bool MapEditorInteractiveDrawController::commitInteractiveDrawSession()
{
    const MapEditorTab::InteractiveDrawMode modeAtCommit = owner_->interactiveDrawMode_;
    if (modeAtCommit != MapEditorTab::InteractiveDrawMode::Line
        && modeAtCommit != MapEditorTab::InteractiveDrawMode::Area) {
        return false;
    }

    if (owner_->textEditor_ == nullptr) {
        owner_->toolbarStatusNote_ = owner_->tr("Complete Draft failed: no active TH2 text editor.");
        owner_->refreshToolbarSummary();
        return true;
    }

    const int minVertexCount = modeAtCommit == MapEditorTab::InteractiveDrawMode::Line ? 2 : 3;
    const int capturedVertices = owner_->interactiveDrawLineVertices_.size();
    if (capturedVertices < minVertexCount) {
        owner_->toolbarStatusNote_ = modeAtCommit == MapEditorTab::InteractiveDrawMode::Line
            ? owner_->tr("Line mode needs at least 2 vertices before completion.")
            : owner_->tr("Area mode needs at least 3 vertices before completion.");
        owner_->refreshToolbarSummary();
        return true;
    }

    if (modeAtCommit == MapEditorTab::InteractiveDrawMode::Line) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QStringList coordinateRows = owner_->lineCoordinateRowsForInteractiveDraft();
        const QString beforeText = owner_->textEditor_->text();
        const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
        if (!owner_->textEditor_->insertDraftLineGeometry(coordinateRows, &insertedLineNumber, &errorMessage)) {
            owner_->toolbarStatusNote_ = errorMessage.isEmpty()
                ? owner_->tr("Complete Draft failed.")
                : owner_->tr("Complete Draft failed: %1").arg(errorMessage);
            owner_->refreshToolbarSummary();
            return true;
        }
        owner_->recordSourceTextSnapshot(owner_->tr("Insert Line"), beforeText, owner_->textEditor_->text(), insertedLineNumber);
        owner_->toolbarStatusNote_ = insertedLineNumber > 0
            ? owner_->tr("Complete Draft wrote line geometry at source line %1.").arg(insertedLineNumber)
            : owner_->tr("Complete Draft wrote line geometry to source.");
    } else {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QString beforeText = owner_->textEditor_->text();
        const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
        if (!owner_->textEditor_->insertDraftAreaGeometry(owner_->areaCoordinateRowsForInteractiveDraft(),
                                                  &insertedLineNumber,
                                                  &errorMessage)) {
            owner_->toolbarStatusNote_ = errorMessage.isEmpty()
                ? owner_->tr("Complete Draft failed.")
                : owner_->tr("Complete Draft failed: %1").arg(errorMessage);
            owner_->refreshToolbarSummary();
            return true;
        }
        owner_->recordSourceTextSnapshot(owner_->tr("Insert Area"), beforeText, owner_->textEditor_->text(), insertedLineNumber);
        owner_->toolbarStatusNote_ = insertedLineNumber > 0
            ? owner_->tr("Complete Draft wrote area geometry at source line %1.").arg(insertedLineNumber)
            : owner_->tr("Complete Draft wrote area geometry to source.");
    }

    clearInteractiveDrawSession(false);
    owner_->interactiveDrawMode_ = modeAtCommit;
    owner_->selectModeActive_ = false;
    owner_->toolbarStatusNote_ = modeAtCommit == MapEditorTab::InteractiveDrawMode::Line
        ? owner_->tr("Line committed. Line mode is still active for the next object.")
        : owner_->tr("Area committed. Area mode is still active for the next object.");
    owner_->refreshToolbarSummary();
    owner_->updateHelpPanel();
    owner_->updateCommandSurfaceState();
    return true;
}

void MapEditorInteractiveDrawController::clearInteractiveDrawSession(bool clearMode)
{
    owner_->interactiveDrawSourceVertices_.clear();
    owner_->interactiveDrawSceneVertices_.clear();
    owner_->interactiveDrawLineVertices_.clear();
    owner_->interactiveDrawStrokeActive_ = false;
    owner_->interactiveDrawAnchorPressActive_ = false;
    owner_->interactiveDrawAnchorDragActive_ = false;
    owner_->interactiveDrawControlDragActive_ = false;
    owner_->interactiveDrawHoverActive_ = false;
    if (owner_->mapView_ != nullptr && owner_->mapView_->viewport() != nullptr && !owner_->mapPanActive_) {
        owner_->mapView_->viewport()->unsetCursor();
    }

    if (owner_->mapScene_ != nullptr) {
        if (owner_->interactiveDrawPreviewPath_ != nullptr) {
            owner_->mapScene_->removeItem(owner_->interactiveDrawPreviewPath_);
            delete owner_->interactiveDrawPreviewPath_;
            owner_->interactiveDrawPreviewPath_ = nullptr;
        }
        for (QGraphicsItem *marker : std::as_const(owner_->interactiveDrawPreviewMarkers_)) {
            if (marker != nullptr) {
                owner_->mapScene_->removeItem(marker);
                delete marker;
            }
        }
    }
    owner_->interactiveDrawPreviewMarkers_.clear();
    owner_->interactiveDrawPreviewPath_ = nullptr;

    if (clearMode) {
        const bool modeChanged = owner_->interactiveDrawMode_ != MapEditorTab::InteractiveDrawMode::None;
        owner_->interactiveDrawMode_ = MapEditorTab::InteractiveDrawMode::None;
        owner_->selectModeActive_ = true;
        if (modeChanged) {
            emit owner_->modeStatusChanged();
        }
    }
}

void MapEditorInteractiveDrawController::updateInteractiveDrawPreview()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    if (owner_->interactiveDrawPreviewPath_ != nullptr) {
        owner_->mapScene_->removeItem(owner_->interactiveDrawPreviewPath_);
        delete owner_->interactiveDrawPreviewPath_;
        owner_->interactiveDrawPreviewPath_ = nullptr;
    }
    for (QGraphicsItem *marker : std::as_const(owner_->interactiveDrawPreviewMarkers_)) {
        if (marker != nullptr) {
            owner_->mapScene_->removeItem(marker);
            delete marker;
        }
    }
    owner_->interactiveDrawPreviewMarkers_.clear();

    if (owner_->interactiveDrawMode_ != MapEditorTab::InteractiveDrawMode::Line
        && owner_->interactiveDrawMode_ != MapEditorTab::InteractiveDrawMode::Area
        && owner_->interactiveDrawMode_ != MapEditorTab::InteractiveDrawMode::Freehand) {
        return;
    }

    QColor accent;
    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line) {
        accent = QColor(QStringLiteral("#ffb15a"));
    } else if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area) {
        accent = QColor(QStringLiteral("#ff7f8f"));
    } else {
        accent = QColor(QStringLiteral("#66d38f"));
    }
    accent.setAlpha(220);
    QColor controlColor = QColor(QStringLiteral("#33a8ff"));
    controlColor.setAlpha(225);

    QPainterPath path;
    QVector<QPointF> anchorMarkers;
    QVector<QPointF> controlMarkers;
    QVector<QLineF> controlConnectors;
    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
        || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area) {
        struct DraftPreviewVertex
        {
            QPointF anchorScene;
            std::optional<QPointF> incomingControlScene;
            std::optional<QPointF> outgoingControlScene;
        };

        QVector<DraftPreviewVertex> previewVertices;
        previewVertices.reserve(owner_->interactiveDrawLineVertices_.size() + 1);
        for (const MapEditorInteractiveLineDraftVertex &vertex : std::as_const(owner_->interactiveDrawLineVertices_)) {
            DraftPreviewVertex previewVertex;
            previewVertex.anchorScene = vertex.anchorScene;
            previewVertex.incomingControlScene = vertex.incomingControlScene;
            previewVertex.outgoingControlScene = vertex.outgoingControlScene;
            previewVertices.append(previewVertex);
            anchorMarkers.append(vertex.anchorScene);
        }

        auto appendBezierControlsForLastSegment = [&previewVertices](const QPointF &dragScenePoint) {
            if (previewVertices.size() < 2) {
                return;
            }
            DraftPreviewVertex &previous = previewVertices[previewVertices.size() - 2];
            DraftPreviewVertex &current = previewVertices[previewVertices.size() - 1];
            // Treat drag location as a quadratic control point, then elevate to cubic.
            // This avoids midpoint-coupled handles that feel artificially parallel.
            constexpr qreal quadraticToCubicFactor = 2.0 / 3.0;
            const QPointF quadraticControlScene = dragScenePoint;
            previous.outgoingControlScene = previous.anchorScene
                + ((quadraticControlScene - previous.anchorScene) * quadraticToCubicFactor);
            current.incomingControlScene = current.anchorScene
                + ((quadraticControlScene - current.anchorScene) * quadraticToCubicFactor);
            if (previous.incomingControlScene.has_value()) {
                const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(previous.anchorScene,
                                                                                    previous.outgoingControlScene.value(),
                                                                                    previous.incomingControlScene);
                if (mirrored.has_value()) {
                    previous.incomingControlScene = mirrored.value();
                }
            }
        };

        if (owner_->interactiveDrawAnchorPressActive_) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = owner_->interactiveDrawAnchorPressScenePoint_;
            previewVertices.append(candidate);
            anchorMarkers.append(candidate.anchorScene);
            if (owner_->interactiveDrawAnchorDragActive_) {
                appendBezierControlsForLastSegment(owner_->interactiveDrawAnchorDragScenePoint_);
            }
        } else if (owner_->interactiveDrawHoverActive_ && !previewVertices.isEmpty()) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = owner_->interactiveDrawHoverScenePoint_;
            previewVertices.append(candidate);
        }

        if (!previewVertices.isEmpty()) {
            path.moveTo(previewVertices.first().anchorScene);
            for (int index = 1; index < previewVertices.size(); ++index) {
                const DraftPreviewVertex &previous = previewVertices.at(index - 1);
                const DraftPreviewVertex &current = previewVertices.at(index);
                const QPointF control1 = previous.outgoingControlScene.value_or(previous.anchorScene);
                const QPointF control2 = current.incomingControlScene.value_or(current.anchorScene);
                const bool curved = previous.outgoingControlScene.has_value() || current.incomingControlScene.has_value();
                if (curved) {
                    path.cubicTo(control1, control2, current.anchorScene);
                } else {
                    path.lineTo(current.anchorScene);
                }
            }

            for (const DraftPreviewVertex &vertex : std::as_const(previewVertices)) {
                if (vertex.incomingControlScene.has_value()) {
                    controlConnectors.append(QLineF(vertex.anchorScene, vertex.incomingControlScene.value()));
                    controlMarkers.append(vertex.incomingControlScene.value());
                }
                if (vertex.outgoingControlScene.has_value()) {
                    controlConnectors.append(QLineF(vertex.anchorScene, vertex.outgoingControlScene.value()));
                    controlMarkers.append(vertex.outgoingControlScene.value());
                }
            }
        }
        if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area && previewVertices.size() >= 3) {
            const DraftPreviewVertex &first = previewVertices.first();
            const DraftPreviewVertex &last = previewVertices.last();

            std::optional<QPointF> closingOutgoing = last.outgoingControlScene;
            if (!closingOutgoing.has_value() && last.incomingControlScene.has_value()) {
                closingOutgoing = mirroredSmoothControlPoint(last.anchorScene,
                                                             last.incomingControlScene.value(),
                                                             std::nullopt);
            }

            std::optional<QPointF> closingIncoming = first.incomingControlScene;
            if (!closingIncoming.has_value() && first.outgoingControlScene.has_value()) {
                closingIncoming = mirroredSmoothControlPoint(first.anchorScene,
                                                             first.outgoingControlScene.value(),
                                                             std::nullopt);
            }

            const bool curvedClose = closingOutgoing.has_value() || closingIncoming.has_value();
            if (curvedClose) {
                path.cubicTo(closingOutgoing.value_or(last.anchorScene),
                             closingIncoming.value_or(first.anchorScene),
                             first.anchorScene);
                if (closingOutgoing.has_value()) {
                    controlConnectors.append(QLineF(last.anchorScene, closingOutgoing.value()));
                    controlMarkers.append(closingOutgoing.value());
                }
                if (closingIncoming.has_value()) {
                    controlConnectors.append(QLineF(first.anchorScene, closingIncoming.value()));
                    controlMarkers.append(closingIncoming.value());
                }
            } else {
                path.lineTo(first.anchorScene);
            }
        }
    } else {
        if (owner_->interactiveDrawSceneVertices_.isEmpty()) {
            return;
        }
        QVector<QPointF> displayVertices = owner_->interactiveDrawSceneVertices_;
        if (owner_->interactiveDrawHoverActive_) {
            displayVertices.append(owner_->interactiveDrawHoverScenePoint_);
        }

        path = QPainterPath(displayVertices.first());
        for (int index = 1; index < displayVertices.size(); ++index) {
            path.lineTo(displayVertices.at(index));
        }
        if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area && displayVertices.size() >= 3) {
            path.closeSubpath();
        }
        anchorMarkers = owner_->interactiveDrawSceneVertices_;
    }

    owner_->interactiveDrawPreviewPath_ = new QGraphicsPathItem(path);
    owner_->interactiveDrawPreviewPath_->setPen(QPen(accent, 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    owner_->interactiveDrawPreviewPath_->setBrush(Qt::NoBrush);
    owner_->interactiveDrawPreviewPath_->setZValue(28.0);
    owner_->interactiveDrawPreviewPath_->setAcceptedMouseButtons(Qt::NoButton);
    owner_->mapScene_->addItem(owner_->interactiveDrawPreviewPath_);

    for (const QPointF &vertex : std::as_const(anchorMarkers)) {
        auto *marker = new QGraphicsEllipseItem(QRectF(vertex.x() - 3.2, vertex.y() - 3.2, 6.4, 6.4));
        marker->setPen(QPen(accent.darker(130), 1.0));
        marker->setBrush(QBrush(accent));
        marker->setZValue(28.5);
        marker->setAcceptedMouseButtons(Qt::NoButton);
        owner_->mapScene_->addItem(marker);
        owner_->interactiveDrawPreviewMarkers_.append(marker);
    }

    for (const QLineF &connector : std::as_const(controlConnectors)) {
        auto *connectorItem = new QGraphicsLineItem(connector);
        connectorItem->setPen(QPen(controlColor.lighter(115), 1.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        connectorItem->setZValue(28.4);
        connectorItem->setAcceptedMouseButtons(Qt::NoButton);
        owner_->mapScene_->addItem(connectorItem);
        owner_->interactiveDrawPreviewMarkers_.append(connectorItem);
    }

    for (const QPointF &control : std::as_const(controlMarkers)) {
        auto *marker = new QGraphicsEllipseItem(QRectF(control.x() - 4.0, control.y() - 4.0, 8.0, 8.0));
        marker->setPen(QPen(controlColor.darker(150), 1.2));
        marker->setBrush(QBrush(controlColor));
        marker->setZValue(28.6);
        marker->setAcceptedMouseButtons(Qt::NoButton);
        owner_->mapScene_->addItem(marker);
        owner_->interactiveDrawPreviewMarkers_.append(marker);
    }
}

bool MapEditorInteractiveDrawController::cancelInteractiveDrawingToSelectMode()
{
    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::None) {
        return false;
    }

    const MapEditorTab::InteractiveDrawMode modeAtCancel = owner_->interactiveDrawMode_;
    const bool isLineOrArea = modeAtCancel == MapEditorTab::InteractiveDrawMode::Line
        || modeAtCancel == MapEditorTab::InteractiveDrawMode::Area;
    bool committedLineOrAreaDraft = false;
    if (isLineOrArea && owner_->hasCompletableInteractiveDrawSession()) {
        if (modeAtCancel == MapEditorTab::InteractiveDrawMode::Line) {
            QString errorMessage;
            int insertedLineNumber = 0;
            const QString beforeText = owner_->textEditor_->text();
            const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
            if (!owner_->textEditor_->insertDraftLineGeometry(owner_->lineCoordinateRowsForInteractiveDraft(),
                                                      &insertedLineNumber,
                                                      &errorMessage)) {
                owner_->toolbarStatusNote_ = errorMessage.isEmpty()
                    ? owner_->tr("Complete Draft failed.")
                    : owner_->tr("Complete Draft failed: %1").arg(errorMessage);
                owner_->refreshToolbarSummary();
                owner_->updateCommandSurfaceState();
                owner_->updateHelpPanel();
                return false;
            }
            owner_->recordSourceTextSnapshot(owner_->tr("Insert Line"), beforeText, owner_->textEditor_->text(), insertedLineNumber);
        } else {
            QString errorMessage;
            int insertedLineNumber = 0;
            const QString beforeText = owner_->textEditor_->text();
            const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
            if (!owner_->textEditor_->insertDraftAreaGeometry(owner_->areaCoordinateRowsForInteractiveDraft(),
                                                      &insertedLineNumber,
                                                      &errorMessage)) {
                owner_->toolbarStatusNote_ = errorMessage.isEmpty()
                    ? owner_->tr("Complete Draft failed.")
                    : owner_->tr("Complete Draft failed: %1").arg(errorMessage);
                owner_->refreshToolbarSummary();
                owner_->updateCommandSurfaceState();
                owner_->updateHelpPanel();
                return false;
            }
            owner_->recordSourceTextSnapshot(owner_->tr("Insert Area"), beforeText, owner_->textEditor_->text(), insertedLineNumber);
        }
        committedLineOrAreaDraft = true;
    }

    clearInteractiveDrawSession(true);
    owner_->interactiveDrawStrokeActive_ = false;
    if (owner_->mapView_ != nullptr) {
        owner_->mapView_->setDragMode(QGraphicsView::NoDrag);
    }

    if (committedLineOrAreaDraft) {
        owner_->toolbarStatusNote_ = owner_->tr("Selection mode: draft committed.");
    } else if (isLineOrArea) {
        owner_->toolbarStatusNote_ = owner_->tr("Selection mode: draft canceled.");
    } else {
        owner_->toolbarStatusNote_ = owner_->tr("Selection mode: drawing canceled.");
    }
    owner_->refreshToolbarSummary();
    owner_->updateCommandSurfaceState();
    owner_->updateHelpPanel();
    return true;
}
}
