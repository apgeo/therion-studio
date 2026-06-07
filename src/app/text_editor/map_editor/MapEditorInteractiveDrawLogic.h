#pragma once

#include <QPointF>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace TherionStudio
{
struct MapEditorInteractiveLineDraftVertex
{
    QPointF anchorSource;
    QPointF anchorScene;
    std::optional<QPointF> incomingControlSource;
    std::optional<QPointF> incomingControlScene;
    std::optional<QPointF> outgoingControlSource;
    std::optional<QPointF> outgoingControlScene;
};

struct MapEditorInteractiveLineControlHandleRef
{
    enum class Kind
    {
        Incoming,
        Outgoing
    };

    int vertexIndex = -1;
    Kind kind = Kind::Incoming;
};

QStringList lineCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices);
QStringList closedLineCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices);
QStringList areaCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices);
QStringList bezierLineCoordinateRowsForFreehandStroke(const QVector<QPointF> &sourceVertices);
void captureInteractiveLineAnchor(QVector<MapEditorInteractiveLineDraftVertex> *vertices,
                                  const QPointF &anchorScenePoint,
                                  const QPointF &anchorSourcePoint,
                                  const std::optional<QPointF> &dragScenePoint,
                                  const std::function<QPointF(const QPointF &)> &sceneToSource);
std::optional<MapEditorInteractiveLineControlHandleRef> interactiveLineControlAt(
    const QVector<MapEditorInteractiveLineDraftVertex> &vertices,
    const QPointF &scenePosition,
    qreal sceneRadius);
bool setInteractiveLineControlScenePoint(QVector<MapEditorInteractiveLineDraftVertex> *vertices,
                                         const MapEditorInteractiveLineControlHandleRef &handle,
                                         const QPointF &scenePoint,
                                         const std::function<QPointF(const QPointF &)> &sceneToSource);
}
