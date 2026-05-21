#pragma once

#include <QPointF>
#include <QRectF>

namespace TherionStudio
{
class MapEditorTab;

class MapEditorSceneLifecycleController final
{
public:
    explicit MapEditorSceneLifecycleController(MapEditorTab *owner);
    explicit MapEditorSceneLifecycleController(const MapEditorTab *owner);

    void clearMapScene();
    void clearDraftGeometryItems();
    void clearBackgroundImageItems();
    void restoreDraftGeometryItems();
    void restoreBackgroundImageItems();
    void fitMapToView(bool includeBackgroundImages);
    void syncZoomFactorFromView();
    void applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition);
    void adjustMapZoom(qreal factor);
    QRectF mapGeometryFitBounds() const;
    QRectF mapPreviewBounds() const;

private:
    MapEditorTab *owner_ = nullptr;
};
}
