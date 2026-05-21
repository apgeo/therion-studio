#include "MapEditorInteractiveDrawLogic.h"

#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"

#include <QLineF>

namespace TherionStudio
{
QStringList lineCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    QStringList rows;
    if (vertices.isEmpty()) {
        return rows;
    }

    rows.reserve(vertices.size());
    const auto formatPoint = [](const QPointF &point) {
        return QStringLiteral("%1 %2")
            .arg(formatSourceCoordinate(point.x()), formatSourceCoordinate(point.y()));
    };

    rows.append(formatPoint(vertices.first().anchorSource));
    for (int index = 1; index < vertices.size(); ++index) {
        const MapEditorInteractiveLineDraftVertex &previous = vertices.at(index - 1);
        const MapEditorInteractiveLineDraftVertex &current = vertices.at(index);
        QStringList tokens;
        if (previous.outgoingControlSource.has_value() || current.incomingControlSource.has_value()) {
            const QPointF control1 = previous.outgoingControlSource.value_or(previous.anchorSource);
            const QPointF control2 = current.incomingControlSource.value_or(current.anchorSource);
            tokens << formatSourceCoordinate(control1.x())
                   << formatSourceCoordinate(control1.y())
                   << formatSourceCoordinate(control2.x())
                   << formatSourceCoordinate(control2.y());
        }
        tokens << formatSourceCoordinate(current.anchorSource.x())
               << formatSourceCoordinate(current.anchorSource.y());
        rows.append(tokens.join(QLatin1Char(' ')));
    }

    return rows;
}

QStringList areaCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    QStringList rows = lineCoordinateRowsForInteractiveDraft(vertices);
    if (vertices.size() < 3) {
        return rows;
    }

    const MapEditorInteractiveLineDraftVertex &first = vertices.first();
    const MapEditorInteractiveLineDraftVertex &last = vertices.last();

    std::optional<QPointF> closingOutgoing = last.outgoingControlSource;
    if (!closingOutgoing.has_value() && last.incomingControlSource.has_value()) {
        closingOutgoing = mirroredSmoothControlPoint(last.anchorSource,
                                                     last.incomingControlSource.value(),
                                                     std::nullopt);
    }

    std::optional<QPointF> closingIncoming = first.incomingControlSource;
    if (!closingIncoming.has_value() && first.outgoingControlSource.has_value()) {
        closingIncoming = mirroredSmoothControlPoint(first.anchorSource,
                                                     first.outgoingControlSource.value(),
                                                     std::nullopt);
    }

    if (!closingOutgoing.has_value() && !closingIncoming.has_value()) {
        return rows;
    }

    QStringList tokens;
    const QPointF control1 = closingOutgoing.value_or(last.anchorSource);
    const QPointF control2 = closingIncoming.value_or(first.anchorSource);
    tokens << formatSourceCoordinate(control1.x())
           << formatSourceCoordinate(control1.y())
           << formatSourceCoordinate(control2.x())
           << formatSourceCoordinate(control2.y())
           << formatSourceCoordinate(first.anchorSource.x())
           << formatSourceCoordinate(first.anchorSource.y());
    rows.append(tokens.join(QLatin1Char(' ')));
    return rows;
}

void captureInteractiveLineAnchor(QVector<MapEditorInteractiveLineDraftVertex> *vertices,
                                  const QPointF &anchorScenePoint,
                                  const QPointF &anchorSourcePoint,
                                  const std::optional<QPointF> &dragScenePoint,
                                  const std::function<QPointF(const QPointF &)> &sceneToSource)
{
    if (vertices == nullptr || sceneToSource == nullptr) {
        return;
    }

    MapEditorInteractiveLineDraftVertex vertex;
    vertex.anchorScene = anchorScenePoint;
    vertex.anchorSource = anchorSourcePoint;
    vertices->append(vertex);

    if (dragScenePoint.has_value() && vertices->size() >= 2) {
        MapEditorInteractiveLineDraftVertex &previous = (*vertices)[vertices->size() - 2];
        MapEditorInteractiveLineDraftVertex &current = vertices->last();
        constexpr qreal quadraticToCubicFactor = 2.0 / 3.0;
        const QPointF quadraticControlScene = dragScenePoint.value();
        previous.outgoingControlScene = previous.anchorScene
            + ((quadraticControlScene - previous.anchorScene) * quadraticToCubicFactor);
        current.incomingControlScene = current.anchorScene
            + ((quadraticControlScene - current.anchorScene) * quadraticToCubicFactor);
        previous.outgoingControlSource = sceneToSource(previous.outgoingControlScene.value());
        current.incomingControlSource = sceneToSource(current.incomingControlScene.value());
        if (previous.incomingControlScene.has_value()) {
            const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(previous.anchorScene,
                                                                               previous.outgoingControlScene.value(),
                                                                               previous.incomingControlScene);
            if (mirrored.has_value()) {
                previous.incomingControlScene = mirrored.value();
                previous.incomingControlSource = sceneToSource(mirrored.value());
            }
        }
    }
}

std::optional<MapEditorInteractiveLineControlHandleRef> interactiveLineControlAt(
    const QVector<MapEditorInteractiveLineDraftVertex> &vertices,
    const QPointF &scenePosition,
    qreal sceneRadius)
{
    if (sceneRadius <= 0.0) {
        return std::nullopt;
    }

    qreal bestDistance = sceneRadius;
    std::optional<MapEditorInteractiveLineControlHandleRef> bestHandle;
    for (int index = 0; index < vertices.size(); ++index) {
        const MapEditorInteractiveLineDraftVertex &vertex = vertices.at(index);
        if (vertex.incomingControlScene.has_value()) {
            const qreal distance = QLineF(scenePosition, vertex.incomingControlScene.value()).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestHandle = MapEditorInteractiveLineControlHandleRef{index,
                                                                      MapEditorInteractiveLineControlHandleRef::Kind::Incoming};
            }
        }
        if (vertex.outgoingControlScene.has_value()) {
            const qreal distance = QLineF(scenePosition, vertex.outgoingControlScene.value()).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestHandle = MapEditorInteractiveLineControlHandleRef{index,
                                                                      MapEditorInteractiveLineControlHandleRef::Kind::Outgoing};
            }
        }
    }

    return bestHandle;
}

bool setInteractiveLineControlScenePoint(QVector<MapEditorInteractiveLineDraftVertex> *vertices,
                                         const MapEditorInteractiveLineControlHandleRef &handle,
                                         const QPointF &scenePoint,
                                         const std::function<QPointF(const QPointF &)> &sceneToSource)
{
    if (vertices == nullptr
        || sceneToSource == nullptr
        || handle.vertexIndex < 0
        || handle.vertexIndex >= vertices->size()) {
        return false;
    }

    MapEditorInteractiveLineDraftVertex &vertex = (*vertices)[handle.vertexIndex];
    if (handle.kind == MapEditorInteractiveLineControlHandleRef::Kind::Incoming) {
        vertex.incomingControlScene = scenePoint;
        vertex.incomingControlSource = sceneToSource(scenePoint);
        if (vertex.outgoingControlScene.has_value()) {
            const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(vertex.anchorScene,
                                                                               scenePoint,
                                                                               vertex.outgoingControlScene);
            if (mirrored.has_value()) {
                vertex.outgoingControlScene = mirrored.value();
                vertex.outgoingControlSource = sceneToSource(mirrored.value());
            }
        }
        return true;
    }

    vertex.outgoingControlScene = scenePoint;
    vertex.outgoingControlSource = sceneToSource(scenePoint);
    if (vertex.incomingControlScene.has_value()) {
        const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(vertex.anchorScene,
                                                                           scenePoint,
                                                                           vertex.incomingControlScene);
        if (mirrored.has_value()) {
            vertex.incomingControlScene = mirrored.value();
            vertex.incomingControlSource = sceneToSource(mirrored.value());
        }
    }
    return true;
}
}
