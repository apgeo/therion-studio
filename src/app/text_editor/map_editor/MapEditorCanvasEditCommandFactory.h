#pragma once

#include "MapEditorSceneSupport.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>

class QUndoCommand;

namespace TherionStudio
{
class TextEditorTab;

using MapCanvasEditStatusCallback = std::function<void(const QString &)>;

struct MapLineAreaVertexSecondaryMove
{
    int vertexIndex = -1;
    QPointF oldPoint;
    QPointF newPoint;
};

struct MapLineAreaVertexMoveSet
{
    QVector<MapLineAreaVertexSecondaryMove> secondaryMoves;
};

QUndoCommand *createMapPointGeometryMoveCommand(TextEditorTab *textEditor,
                                                int lineNumber,
                                                const QPointF &oldPoint,
                                                const QPointF &newPoint,
                                                MapCanvasEditStatusCallback statusCallback);
QUndoCommand *createMapLineAreaVertexMoveCommand(TextEditorTab *textEditor,
                                                 int lineNumber,
                                                 const QString &kind,
                                                 int vertexIndex,
                                                 const QPointF &oldPoint,
                                                 const QPointF &newPoint,
                                                 const QVector<MapLineAreaVertexSecondaryMove> &secondaryMoves,
                                                 MapCanvasEditStatusCallback statusCallback);
QUndoCommand *createMapSourceTextSnapshotCommand(TextEditorTab *textEditor,
                                                 const QString &label,
                                                 const QString &beforeText,
                                                 const QString &afterText,
                                                 int insertedLineNumber,
                                                 MapCanvasEditStatusCallback statusCallback);
MapLineAreaVertexMoveSet coupledLineVertexMoveSet(const MapGeometryFeature &lineFeature,
                                                  int sourceVertexIndex,
                                                  const QPointF &oldPoint,
                                                  const QPointF &newPoint);
}
