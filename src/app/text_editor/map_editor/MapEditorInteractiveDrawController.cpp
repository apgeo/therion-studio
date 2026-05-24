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

#include <utility>

namespace TherionStudio
{
QString MapEditorInteractiveDrawController::tr(const char *text) const
{
    return context_.translate ? context_.translate(text) : QString::fromUtf8(text);
}

MapEditorInteractiveDrawMode MapEditorInteractiveDrawController::mode() const
{
    return context_.drawMode ? context_.drawMode() : MapEditorInteractiveDrawMode::None;
}

void MapEditorInteractiveDrawController::setMode(MapEditorInteractiveDrawMode mode)
{
    if (context_.setDrawMode) {
        context_.setDrawMode(mode);
    }
}

MapEditorInteractiveDrawController::MapEditorInteractiveDrawController(MapEditorInteractiveDrawContext context)
    : context_(std::move(context))
{
}

void MapEditorInteractiveDrawController::setInteractiveDrawMode(MapEditorInteractiveDrawMode mode)
{
    const MapEditorInteractiveDrawMode previousMode = this->mode();
    clearInteractiveDrawSession(false);
    setMode(mode);
    (*context_.selectModeActive) = mode == MapEditorInteractiveDrawMode::None;
    if (previousMode != this->mode()) {
        context_.emitModeStatusChanged();
    }
    context_.updateCommandSurfaceState();
}

bool MapEditorInteractiveDrawController::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    if (mode() == MapEditorInteractiveDrawMode::None) {
        return false;
    }

    if (context_.textEditor == nullptr) {
        (*context_.toolbarStatusNote) = tr("Drawing failed: no active TH2 text editor.");
        context_.refreshToolbarSummary();
        return true;
    }

    if (mode() == MapEditorInteractiveDrawMode::Point) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QVector<QPointF> vertices{context_.sourcePointFromScenePosition(scenePosition)};
        const QString beforeText = context_.textEditor->text();
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        if (!context_.textEditor->insertDraftGeometry(QStringLiteral("point"), vertices, &insertedLineNumber, &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("Point insert failed.")
                : tr("Point insert failed: %1").arg(errorMessage);
        } else {
            context_.recordSourceTextSnapshot(tr("Insert Point"), beforeText, context_.textEditor->text(), insertedLineNumber);
            (*context_.toolbarStatusNote) = insertedLineNumber > 0
                ? tr("Inserted point at source line %1.").arg(insertedLineNumber)
                : tr("Inserted point.");
        }
        context_.refreshToolbarSummary();
        return true;
    }

    if (mode() == MapEditorInteractiveDrawMode::Area) {
        context_.captureInteractiveLineAnchor(scenePosition, std::nullopt);
        (*context_.hoverActive) = false;
        (*context_.toolbarStatusNote) = tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                                 .arg((*context_.lineVertices).size());
        context_.refreshToolbarSummary();
        context_.updateCommandSurfaceState();
        return true;
    }

    return false;
}

bool MapEditorInteractiveDrawController::commitInteractiveDrawSession()
{
    const MapEditorInteractiveDrawMode modeAtCommit = mode();
    if (modeAtCommit != MapEditorInteractiveDrawMode::Line
        && modeAtCommit != MapEditorInteractiveDrawMode::Area) {
        return false;
    }

    if (context_.textEditor == nullptr) {
        (*context_.toolbarStatusNote) = tr("Complete Draft failed: no active TH2 text editor.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int minVertexCount = modeAtCommit == MapEditorInteractiveDrawMode::Line ? 2 : 3;
    const int capturedVertices = (*context_.lineVertices).size();
    if (capturedVertices < minVertexCount) {
        (*context_.toolbarStatusNote) = modeAtCommit == MapEditorInteractiveDrawMode::Line
            ? tr("Line mode needs at least 2 vertices before completion.")
            : tr("Area mode needs at least 3 vertices before completion.");
        context_.refreshToolbarSummary();
        return true;
    }

    if (modeAtCommit == MapEditorInteractiveDrawMode::Line) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QStringList coordinateRows = context_.lineCoordinateRowsForInteractiveDraft();
        const QString beforeText = context_.textEditor->text();
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        if (!context_.textEditor->insertDraftLineGeometry(coordinateRows, &insertedLineNumber, &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("Complete Draft failed.")
                : tr("Complete Draft failed: %1").arg(errorMessage);
            context_.refreshToolbarSummary();
            return true;
        }
        context_.recordSourceTextSnapshot(tr("Insert Line"), beforeText, context_.textEditor->text(), insertedLineNumber);
        (*context_.toolbarStatusNote) = insertedLineNumber > 0
            ? tr("Complete Draft wrote line geometry at source line %1.").arg(insertedLineNumber)
            : tr("Complete Draft wrote line geometry to source.");
    } else {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QString beforeText = context_.textEditor->text();
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        if (!context_.textEditor->insertDraftAreaGeometry(context_.areaCoordinateRowsForInteractiveDraft(),
                                                  &insertedLineNumber,
                                                  &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("Complete Draft failed.")
                : tr("Complete Draft failed: %1").arg(errorMessage);
            context_.refreshToolbarSummary();
            return true;
        }
        context_.recordSourceTextSnapshot(tr("Insert Area"), beforeText, context_.textEditor->text(), insertedLineNumber);
        (*context_.toolbarStatusNote) = insertedLineNumber > 0
            ? tr("Complete Draft wrote area geometry at source line %1.").arg(insertedLineNumber)
            : tr("Complete Draft wrote area geometry to source.");
    }

    clearInteractiveDrawSession(false);
    setMode(modeAtCommit);
    (*context_.selectModeActive) = false;
    (*context_.toolbarStatusNote) = modeAtCommit == MapEditorInteractiveDrawMode::Line
        ? tr("Line committed. Line mode is still active for the next object.")
        : tr("Area committed. Area mode is still active for the next object.");
    context_.refreshToolbarSummary();
    context_.updateHelpPanel();
    context_.updateCommandSurfaceState();
    return true;
}

void MapEditorInteractiveDrawController::clearInteractiveDrawSession(bool clearMode)
{
    (*context_.sourceVertices).clear();
    (*context_.sceneVertices).clear();
    (*context_.lineVertices).clear();
    (*context_.strokeActive) = false;
    (*context_.anchorPressActive) = false;
    (*context_.anchorDragActive) = false;
    (*context_.controlDragActive) = false;
    (*context_.hoverActive) = false;
    if (context_.view != nullptr && context_.view->viewport() != nullptr && !(*context_.panActive)) {
        context_.view->viewport()->unsetCursor();
    }

    if (context_.scene != nullptr) {
        if ((*context_.previewPath) != nullptr) {
            context_.scene->removeItem((*context_.previewPath));
            delete (*context_.previewPath);
            (*context_.previewPath) = nullptr;
        }
        for (QGraphicsItem *marker : std::as_const((*context_.previewMarkers))) {
            if (marker != nullptr) {
                context_.scene->removeItem(marker);
                delete marker;
            }
        }
    }
    (*context_.previewMarkers).clear();
    (*context_.previewPath) = nullptr;

    if (clearMode) {
        const bool modeChanged = mode() != MapEditorInteractiveDrawMode::None;
        setMode(MapEditorInteractiveDrawMode::None);
        (*context_.selectModeActive) = true;
        if (modeChanged) {
            context_.emitModeStatusChanged();
        }
    }
}

void MapEditorInteractiveDrawController::updateInteractiveDrawPreview()
{
    if (context_.scene == nullptr) {
        return;
    }

    auto clearPreviewMarkers = [this]() {
        for (QGraphicsItem *marker : std::as_const((*context_.previewMarkers))) {
            if (marker != nullptr) {
                if (marker->scene() != nullptr) {
                    marker->scene()->removeItem(marker);
                }
                delete marker;
            }
        }
        (*context_.previewMarkers).clear();
    };
    auto hidePreviewPath = [this]() {
        if ((*context_.previewPath) != nullptr) {
            (*context_.previewPath)->setPath(QPainterPath());
            (*context_.previewPath)->setVisible(false);
        }
    };

    auto removePreviewMarkerAt = [this](int markerIndex) {
        if (markerIndex < 0 || markerIndex >= (*context_.previewMarkers).size()) {
            return;
        }
        QGraphicsItem *marker = (*context_.previewMarkers).at(markerIndex);
        if (marker != nullptr) {
            if (marker->scene() != nullptr) {
                marker->scene()->removeItem(marker);
            }
            delete marker;
        }
        (*context_.previewMarkers)[markerIndex] = nullptr;
    };
    auto ensurePreviewMarkerSlot = [this](int markerIndex) {
        while ((*context_.previewMarkers).size() <= markerIndex) {
            (*context_.previewMarkers).append(nullptr);
        }
    };
    auto ensurePreviewEllipseMarker = [this, &ensurePreviewMarkerSlot, &removePreviewMarkerAt](
                                          int markerIndex,
                                          const QRectF &rect) {
        ensurePreviewMarkerSlot(markerIndex);
        auto *marker = dynamic_cast<QGraphicsEllipseItem *>((*context_.previewMarkers).at(markerIndex));
        if (marker == nullptr) {
            removePreviewMarkerAt(markerIndex);
            marker = new QGraphicsEllipseItem(rect);
            marker->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            marker->setAcceptedMouseButtons(Qt::NoButton);
            (*context_.previewMarkers)[markerIndex] = marker;
        } else {
            marker->setRect(rect);
        }
        if (marker->scene() != nullptr && marker->scene() != context_.scene) {
            marker->scene()->removeItem(marker);
        }
        if (marker->scene() != context_.scene) {
            context_.scene->addItem(marker);
        }
        return marker;
    };
    auto ensurePreviewLineMarker = [this, &ensurePreviewMarkerSlot, &removePreviewMarkerAt](int markerIndex) {
        ensurePreviewMarkerSlot(markerIndex);
        auto *marker = dynamic_cast<QGraphicsLineItem *>((*context_.previewMarkers).at(markerIndex));
        if (marker == nullptr) {
            removePreviewMarkerAt(markerIndex);
            marker = new QGraphicsLineItem();
            marker->setAcceptedMouseButtons(Qt::NoButton);
            (*context_.previewMarkers)[markerIndex] = marker;
        }
        if (marker->scene() != nullptr && marker->scene() != context_.scene) {
            marker->scene()->removeItem(marker);
        }
        if (marker->scene() != context_.scene) {
            context_.scene->addItem(marker);
        }
        return marker;
    };
    auto trimPreviewMarkers = [this, &removePreviewMarkerAt](int usedMarkerCount) {
        for (int markerIndex = (*context_.previewMarkers).size() - 1; markerIndex >= usedMarkerCount; --markerIndex) {
            removePreviewMarkerAt(markerIndex);
        }
        (*context_.previewMarkers).resize(usedMarkerCount);
    };

    if (mode() != MapEditorInteractiveDrawMode::Line
        && mode() != MapEditorInteractiveDrawMode::Area
        && mode() != MapEditorInteractiveDrawMode::Freehand) {
        clearPreviewMarkers();
        hidePreviewPath();
        return;
    }

    QColor accent;
    if (mode() == MapEditorInteractiveDrawMode::Line) {
        accent = QColor(QStringLiteral("#ffb15a"));
    } else if (mode() == MapEditorInteractiveDrawMode::Area) {
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
    if (mode() == MapEditorInteractiveDrawMode::Line
        || mode() == MapEditorInteractiveDrawMode::Area) {
        struct DraftPreviewVertex
        {
            QPointF anchorScene;
            std::optional<QPointF> incomingControlScene;
            std::optional<QPointF> outgoingControlScene;
        };

        QVector<DraftPreviewVertex> previewVertices;
        previewVertices.reserve((*context_.lineVertices).size() + 1);
        for (const MapEditorInteractiveLineDraftVertex &vertex : std::as_const((*context_.lineVertices))) {
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

        if ((*context_.anchorPressActive)) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = (*context_.anchorPressScenePoint);
            previewVertices.append(candidate);
            anchorMarkers.append(candidate.anchorScene);
            if ((*context_.anchorDragActive)) {
                appendBezierControlsForLastSegment((*context_.anchorDragScenePoint));
            }
        } else if ((*context_.hoverActive) && !previewVertices.isEmpty()) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = (*context_.hoverScenePoint);
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
        if (mode() == MapEditorInteractiveDrawMode::Area && previewVertices.size() >= 3) {
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
        if ((*context_.sceneVertices).isEmpty()) {
            clearPreviewMarkers();
            hidePreviewPath();
            return;
        }
        QVector<QPointF> displayVertices = (*context_.sceneVertices);
        if ((*context_.hoverActive)) {
            displayVertices.append((*context_.hoverScenePoint));
        }

        path = QPainterPath(displayVertices.first());
        for (int index = 1; index < displayVertices.size(); ++index) {
            path.lineTo(displayVertices.at(index));
        }
        if (mode() == MapEditorInteractiveDrawMode::Area && displayVertices.size() >= 3) {
            path.closeSubpath();
        }
    }

    if ((*context_.previewPath) == nullptr) {
        (*context_.previewPath) = new QGraphicsPathItem();
        (*context_.previewPath)->setBrush(Qt::NoBrush);
        (*context_.previewPath)->setZValue(28.0);
        (*context_.previewPath)->setAcceptedMouseButtons(Qt::NoButton);
        context_.scene->addItem((*context_.previewPath));
    } else if ((*context_.previewPath)->scene() != context_.scene) {
        context_.scene->addItem((*context_.previewPath));
    }

    const Qt::PenStyle previewPenStyle = mode() == MapEditorInteractiveDrawMode::Freehand
        ? Qt::SolidLine
        : Qt::DashLine;
    QPen previewPen(accent, 3.2, previewPenStyle, Qt::RoundCap, Qt::RoundJoin);
    previewPen.setCosmetic(true);
    (*context_.previewPath)->setPen(previewPen);
    (*context_.previewPath)->setPath(path);
    (*context_.previewPath)->setVisible(true);

    int previewMarkerIndex = 0;
    for (const QPointF &vertex : std::as_const(anchorMarkers)) {
        QGraphicsEllipseItem *marker = ensurePreviewEllipseMarker(previewMarkerIndex++, QRectF(-5.0, -5.0, 10.0, 10.0));
        marker->setPos(vertex);
        QPen markerPen(accent.darker(130), 1.4);
        markerPen.setCosmetic(true);
        marker->setPen(markerPen);
        marker->setBrush(QBrush(accent));
        marker->setZValue(28.5);
    }

    for (const QLineF &connector : std::as_const(controlConnectors)) {
        QGraphicsLineItem *connectorItem = ensurePreviewLineMarker(previewMarkerIndex++);
        connectorItem->setLine(connector);
        QPen connectorPen(controlColor.lighter(115), 1.6, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        connectorPen.setCosmetic(true);
        connectorItem->setPen(connectorPen);
        connectorItem->setZValue(28.4);
    }

    for (const QPointF &control : std::as_const(controlMarkers)) {
        QGraphicsEllipseItem *marker = ensurePreviewEllipseMarker(previewMarkerIndex++, QRectF(-5.2, -5.2, 10.4, 10.4));
        marker->setPos(control);
        QPen markerPen(controlColor.darker(150), 1.5);
        markerPen.setCosmetic(true);
        marker->setPen(markerPen);
        marker->setBrush(QBrush(controlColor));
        marker->setZValue(28.6);
    }
    trimPreviewMarkers(previewMarkerIndex);
}

bool MapEditorInteractiveDrawController::cancelInteractiveDrawingToSelectMode()
{
    if (mode() == MapEditorInteractiveDrawMode::None) {
        return false;
    }

    const MapEditorInteractiveDrawMode modeAtCancel = mode();
    const bool isLineOrArea = modeAtCancel == MapEditorInteractiveDrawMode::Line
        || modeAtCancel == MapEditorInteractiveDrawMode::Area;
    bool committedLineOrAreaDraft = false;
    if (isLineOrArea && context_.hasCompletableInteractiveDrawSession()) {
        if (modeAtCancel == MapEditorInteractiveDrawMode::Line) {
            QString errorMessage;
            int insertedLineNumber = 0;
            const QString beforeText = context_.textEditor->text();
            const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
            if (!context_.textEditor->insertDraftLineGeometry(context_.lineCoordinateRowsForInteractiveDraft(),
                                                      &insertedLineNumber,
                                                      &errorMessage)) {
                (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                    ? tr("Complete Draft failed.")
                    : tr("Complete Draft failed: %1").arg(errorMessage);
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            context_.recordSourceTextSnapshot(tr("Insert Line"), beforeText, context_.textEditor->text(), insertedLineNumber);
        } else {
            QString errorMessage;
            int insertedLineNumber = 0;
            const QString beforeText = context_.textEditor->text();
            const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
            if (!context_.textEditor->insertDraftAreaGeometry(context_.areaCoordinateRowsForInteractiveDraft(),
                                                      &insertedLineNumber,
                                                      &errorMessage)) {
                (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                    ? tr("Complete Draft failed.")
                    : tr("Complete Draft failed: %1").arg(errorMessage);
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            context_.recordSourceTextSnapshot(tr("Insert Area"), beforeText, context_.textEditor->text(), insertedLineNumber);
        }
        committedLineOrAreaDraft = true;
    }

    clearInteractiveDrawSession(true);
    (*context_.strokeActive) = false;
    if (context_.view != nullptr) {
        context_.view->setDragMode(QGraphicsView::NoDrag);
    }

    if (committedLineOrAreaDraft) {
        (*context_.toolbarStatusNote) = tr("Selection mode: draft committed.");
    } else if (isLineOrArea) {
        (*context_.toolbarStatusNote) = tr("Selection mode: draft canceled.");
    } else {
        (*context_.toolbarStatusNote) = tr("Selection mode: drawing canceled.");
    }
    context_.refreshToolbarSummary();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    return true;
}
}
