#pragma once

#include <QModelIndex>
#include <QPointF>
#include <QString>

#include <functional>

class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QStandardItemModel;
class QTreeView;

namespace TherionStudio
{
struct MapEditorInspectorBackgroundContext
{
    QTreeView *backgroundLayersTree = nullptr;
    QStandardItemModel *backgroundLayersModel = nullptr;
    QPushButton *moveUpButton = nullptr;
    QPushButton *moveDownButton = nullptr;
    QDoubleSpinBox *positionXSpin = nullptr;
    QDoubleSpinBox *positionYSpin = nullptr;
    QSlider *opacitySlider = nullptr;
    QSlider *gammaSlider = nullptr;
    QPushButton *opacityResetButton = nullptr;
    QPushButton *gammaResetButton = nullptr;
    bool *updatingUi = nullptr;

    std::function<QString(const char *)> translate;
    std::function<int()> layerCount;
    std::function<QString(int)> layerLabel;
    std::function<bool(int)> layerVisible;
    std::function<qreal(int)> layerOpacity;
    std::function<qreal(int)> layerGamma;
    std::function<bool(int)> layerSupportsGamma;
    std::function<QPointF(int)> layerPosition;
    std::function<int()> selectedLayerIndex;
    std::function<void(int)> setSelectedLayerIndex;
    std::function<void()> removeSelectedLayer;
    std::function<void()> toggleSelectedLayerVisibility;
};

class MapEditorInspectorBackgroundController final
{
public:
    explicit MapEditorInspectorBackgroundController(MapEditorInspectorBackgroundContext context);

    void configureInspectorBackgroundLayerTreeColumns();
    void handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current);
    void handleInspectorBackgroundLayerClicked(const QModelIndex &index);
    void refreshInspectorBackgroundPanel();
    void refreshInspectorBackgroundSelectionControls();

private:
    QString tr(const char *text) const;

    MapEditorInspectorBackgroundContext context_;
};
}
