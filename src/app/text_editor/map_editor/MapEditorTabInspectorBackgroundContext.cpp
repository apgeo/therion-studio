#include "MapEditorTab.h"

#include "MapEditorInspectorBackgroundController.h"

namespace TherionStudio
{
MapEditorInspectorBackgroundContext MapEditorTab::inspectorBackgroundContext()
{
    return MapEditorInspectorBackgroundContext{
        .backgroundLayersTree = mapBackgroundLayersTree_,
        .backgroundLayersModel = mapBackgroundLayersModel_,
        .moveUpButton = mapBackgroundMoveUpButton_,
        .moveDownButton = mapBackgroundMoveDownButton_,
        .positionXSpin = mapBackgroundPosXSpin_,
        .positionYSpin = mapBackgroundPosYSpin_,
        .scaleXSpin = mapBackgroundScaleXSpin_,
        .scaleYSpin = mapBackgroundScaleYSpin_,
        .rotationSpin = mapBackgroundRotationSpin_,
        .opacitySlider = mapBackgroundOpacitySlider_,
        .gammaSlider = mapBackgroundGammaSlider_,
        .setPivotButton = mapBackgroundSetPivotButton_,
        .resetPivotButton = mapBackgroundResetPivotButton_,
        .opacityResetButton = mapBackgroundOpacityResetButton_,
        .gammaResetButton = mapBackgroundGammaResetButton_,
        .updatingUi = &updatingMapInspectorBackgroundUi_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .layerCount = [this]() {
            return backgroundLayerCount();
        },
        .layerLabel = [this](int index) {
            return backgroundLayerLabel(index);
        },
        .layerVisible = [this](int index) {
            return isBackgroundLayerVisible(index);
        },
        .layerOpacity = [this](int index) {
            return backgroundLayerOpacity(index);
        },
        .layerGamma = [this](int index) {
            return backgroundLayerGamma(index);
        },
        .layerXScale = [this](int index) {
            return backgroundLayerXScale(index);
        },
        .layerYScale = [this](int index) {
            return backgroundLayerYScale(index);
        },
        .layerRotationDeg = [this](int index) {
            return backgroundLayerRotationDeg(index);
        },
        .layerSupportsGamma = [this](int index) {
            return backgroundLayerSupportsGamma(index);
        },
        .layerSupportsTransformEditing = [this](int index) {
            return backgroundLayerSupportsTransformEditing(index);
        },
        .layerSupportsPositionEditing = [this](int index) {
            return backgroundLayerSupportsPositionEditing(index);
        },
        .layerPosition = [this](int index) {
            return backgroundLayerPosition(index);
        },
        .selectedLayerIndex = [this]() {
            return selectedBackgroundLayerIndex();
        },
        .setSelectedLayerIndex = [this](int index) {
            setSelectedBackgroundLayerIndex(index);
        },
        .removeSelectedLayer = [this]() {
            removeSelectedBackgroundLayer();
        },
        .toggleSelectedLayerVisibility = [this]() {
            toggleSelectedBackgroundLayerVisibility();
        },
    };
}
}
