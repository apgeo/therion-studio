#include "MapEditorInspectorBackgroundController.h"

#include "MapEditorInspectorData.h"

#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSlider>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>

#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kInspectorBackgroundNameColumn = 0;
constexpr int kInspectorBackgroundVisibilityColumn = 1;
constexpr int kInspectorBackgroundDeleteColumn = 2;
constexpr int kInspectorBackgroundColumnCount = 3;
constexpr int kInspectorBackgroundLayerIndexRole = Qt::UserRole + 730;
}

MapEditorInspectorBackgroundController::MapEditorInspectorBackgroundController(MapEditorInspectorBackgroundContext context)
    : context_(std::move(context))
{
}

QString MapEditorInspectorBackgroundController::translate(const char *text) const
{
    return context_.translate ? context_.translate(text) : QString::fromUtf8(text);
}

void MapEditorInspectorBackgroundController::configureInspectorBackgroundLayerTreeColumns()
{
    if (context_.backgroundLayersTree == nullptr) {
        return;
    }

    if (context_.backgroundLayersModel != nullptr && context_.backgroundLayersModel->columnCount() < kInspectorBackgroundColumnCount) {
        context_.backgroundLayersModel->setColumnCount(kInspectorBackgroundColumnCount);
    }
    context_.backgroundLayersTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (QHeaderView *header = context_.backgroundLayersTree->header(); header != nullptr) {
        header->setStretchLastSection(false);
        header->setSectionsMovable(false);
        header->setMinimumSectionSize(22);
        header->setSectionResizeMode(kInspectorBackgroundNameColumn, QHeaderView::Stretch);
        header->setSectionResizeMode(kInspectorBackgroundVisibilityColumn, QHeaderView::Fixed);
        header->setSectionResizeMode(kInspectorBackgroundDeleteColumn, QHeaderView::Fixed);
        header->resizeSection(kInspectorBackgroundVisibilityColumn, 26);
        header->resizeSection(kInspectorBackgroundDeleteColumn, 26);
    }
}

void MapEditorInspectorBackgroundController::handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current)
{
    if (context_.updatingUi == nullptr || *context_.updatingUi || !current.isValid()) {
        return;
    }
    if (current.column() == kInspectorBackgroundVisibilityColumn
        || current.column() == kInspectorBackgroundDeleteColumn) {
        return;
    }

    const QModelIndex layerIndex = current.sibling(current.row(), kInspectorBackgroundNameColumn);
    const int backgroundLayerIndex = layerIndex.data(kInspectorBackgroundLayerIndexRole).toInt();
    if (context_.setSelectedLayerIndex) {
        context_.setSelectedLayerIndex(backgroundLayerIndex);
    }
}

void MapEditorInspectorBackgroundController::handleInspectorBackgroundLayerClicked(const QModelIndex &index)
{
    if (!index.isValid() || context_.backgroundLayersModel == nullptr) {
        return;
    }
    if (index.column() != kInspectorBackgroundVisibilityColumn && index.column() != kInspectorBackgroundDeleteColumn) {
        return;
    }

    const QModelIndex layerIndex = index.sibling(index.row(), kInspectorBackgroundNameColumn);
    const int backgroundLayerIndex = layerIndex.data(kInspectorBackgroundLayerIndexRole).toInt();
    const int layerCount = context_.layerCount ? context_.layerCount() : 0;
    if (backgroundLayerIndex < 0 || backgroundLayerIndex >= layerCount) {
        return;
    }

    if (context_.setSelectedLayerIndex) {
        context_.setSelectedLayerIndex(backgroundLayerIndex);
    }
    if (index.column() == kInspectorBackgroundDeleteColumn) {
        if (context_.removeSelectedLayer) {
            context_.removeSelectedLayer();
        }
        return;
    }
    if (context_.toggleSelectedLayerVisibility) {
        context_.toggleSelectedLayerVisibility();
    }
}

void MapEditorInspectorBackgroundController::refreshInspectorBackgroundPanel()
{
    if (context_.backgroundLayersModel == nullptr || context_.updatingUi == nullptr) {
        return;
    }

    const QScopedValueRollback<bool> guard(*context_.updatingUi, true);
    context_.backgroundLayersModel->clear();
    context_.backgroundLayersModel->setColumnCount(kInspectorBackgroundColumnCount);
    context_.backgroundLayersModel->setHorizontalHeaderLabels({translate("Layers"), QString(), QString()});
    configureInspectorBackgroundLayerTreeColumns();

    const int layerCount = context_.layerCount ? context_.layerCount() : 0;
    for (int index = 0; index < layerCount; ++index) {
        auto *layerItem = new QStandardItem(context_.layerLabel ? context_.layerLabel(index) : QString());
        layerItem->setEditable(false);
        layerItem->setData(index, kInspectorBackgroundLayerIndexRole);

        auto *visibilityItem = new QStandardItem;
        visibilityItem->setEditable(false);
        visibilityItem->setTextAlignment(Qt::AlignCenter);
        visibilityItem->setData(index, kInspectorBackgroundLayerIndexRole);
        const bool visible = context_.layerVisible ? context_.layerVisible(index) : false;
        visibilityItem->setIcon(inspectorActionIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off")));
        visibilityItem->setToolTip(visible ? translate("Hide background layer") : translate("Show background layer"));

        auto *deleteItem = new QStandardItem;
        deleteItem->setEditable(false);
        deleteItem->setTextAlignment(Qt::AlignCenter);
        deleteItem->setData(index, kInspectorBackgroundLayerIndexRole);
        deleteItem->setIcon(inspectorActionIcon(QStringLiteral("trash-2")));
        deleteItem->setToolTip(translate("Remove background layer"));

        context_.backgroundLayersModel->appendRow({layerItem, visibilityItem, deleteItem});
    }
    if (context_.backgroundLayersTree != nullptr && context_.backgroundLayersTree->selectionModel() != nullptr) {
        const int selectedLayer = context_.selectedLayerIndex ? context_.selectedLayerIndex() : -1;
        const QModelIndex selectedIndex = selectedLayer >= 0
            ? context_.backgroundLayersModel->index(selectedLayer, kInspectorBackgroundNameColumn)
            : QModelIndex();
        context_.backgroundLayersTree->setCurrentIndex(selectedIndex);
        if (selectedIndex.isValid()) {
            context_.backgroundLayersTree->selectionModel()->select(selectedIndex,
                                                               QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            context_.backgroundLayersTree->selectionModel()->clearSelection();
        }
    }

    const int selectedIndex = context_.selectedLayerIndex ? context_.selectedLayerIndex() : -1;
    const bool hasLayer = selectedIndex >= 0 && selectedIndex < layerCount;
    const bool supportsGamma = hasLayer
        && (!context_.layerSupportsGamma || context_.layerSupportsGamma(selectedIndex));
    context_.moveUpButton->setEnabled(hasLayer && selectedIndex > 0);
    context_.moveDownButton->setEnabled(hasLayer && selectedIndex >= 0 && selectedIndex < layerCount - 1);
    context_.positionXSpin->setEnabled(hasLayer);
    context_.positionYSpin->setEnabled(hasLayer);
    context_.opacitySlider->setEnabled(hasLayer);
    context_.gammaSlider->setEnabled(supportsGamma);
    context_.opacityResetButton->setEnabled(hasLayer);
    context_.gammaResetButton->setEnabled(supportsGamma);

    if (!hasLayer) {
        context_.positionXSpin->setValue(0.0);
        context_.positionYSpin->setValue(0.0);
        context_.opacitySlider->setValue(58);
        context_.gammaSlider->setValue(100);
        return;
    }

    const QPointF position = context_.layerPosition ? context_.layerPosition(selectedIndex) : QPointF();
    context_.positionXSpin->setValue(position.x());
    context_.positionYSpin->setValue(position.y());
    context_.opacitySlider->setValue(qBound(5, qRound((context_.layerOpacity ? context_.layerOpacity(selectedIndex) : 0.58) * 100.0), 100));
    context_.gammaSlider->setValue(supportsGamma
                                       ? qBound(20, qRound((context_.layerGamma ? context_.layerGamma(selectedIndex) : 1.0) * 100.0), 250)
                                       : 100);
}
}
