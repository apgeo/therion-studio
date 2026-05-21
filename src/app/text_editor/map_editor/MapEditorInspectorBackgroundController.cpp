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

MapEditorInspectorBackgroundController::MapEditorInspectorBackgroundController(MapEditorTab *owner)
    : owner_(owner)
{
}

void MapEditorInspectorBackgroundController::configureInspectorBackgroundLayerTreeColumns()
{
    if (owner_->mapBackgroundLayersTree_ == nullptr) {
        return;
    }

    if (owner_->mapBackgroundLayersModel_ != nullptr && owner_->mapBackgroundLayersModel_->columnCount() < kInspectorBackgroundColumnCount) {
        owner_->mapBackgroundLayersModel_->setColumnCount(kInspectorBackgroundColumnCount);
    }
    owner_->mapBackgroundLayersTree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (QHeaderView *header = owner_->mapBackgroundLayersTree_->header(); header != nullptr) {
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
    if (owner_->updatingMapInspectorBackgroundUi_ || !current.isValid()) {
        return;
    }
    if (current.column() == kInspectorBackgroundVisibilityColumn
        || current.column() == kInspectorBackgroundDeleteColumn) {
        return;
    }

    const QModelIndex layerIndex = current.sibling(current.row(), kInspectorBackgroundNameColumn);
    const int backgroundLayerIndex = layerIndex.data(kInspectorBackgroundLayerIndexRole).toInt();
    owner_->setSelectedBackgroundLayerIndex(backgroundLayerIndex);
}

void MapEditorInspectorBackgroundController::handleInspectorBackgroundLayerClicked(const QModelIndex &index)
{
    if (!index.isValid() || owner_->mapBackgroundLayersModel_ == nullptr) {
        return;
    }
    if (index.column() != kInspectorBackgroundVisibilityColumn && index.column() != kInspectorBackgroundDeleteColumn) {
        return;
    }

    const QModelIndex layerIndex = index.sibling(index.row(), kInspectorBackgroundNameColumn);
    const int backgroundLayerIndex = layerIndex.data(kInspectorBackgroundLayerIndexRole).toInt();
    if (backgroundLayerIndex < 0 || backgroundLayerIndex >= owner_->backgroundLayerCount()) {
        return;
    }

    owner_->setSelectedBackgroundLayerIndex(backgroundLayerIndex);
    if (index.column() == kInspectorBackgroundDeleteColumn) {
        owner_->removeSelectedBackgroundLayer();
        return;
    }
    owner_->toggleSelectedBackgroundLayerVisibility();
}

void MapEditorInspectorBackgroundController::refreshInspectorBackgroundPanel()
{
    if (owner_->mapBackgroundLayersModel_ == nullptr) {
        return;
    }

    const QScopedValueRollback<bool> guard(owner_->updatingMapInspectorBackgroundUi_, true);
    owner_->mapBackgroundLayersModel_->clear();
    owner_->mapBackgroundLayersModel_->setColumnCount(kInspectorBackgroundColumnCount);
    owner_->mapBackgroundLayersModel_->setHorizontalHeaderLabels({owner_->tr("Layers"), QString(), QString()});
    configureInspectorBackgroundLayerTreeColumns();

    const int layerCount = owner_->backgroundLayerCount();
    for (int index = 0; index < layerCount; ++index) {
        auto *layerItem = new QStandardItem(owner_->backgroundLayerLabel(index));
        layerItem->setEditable(false);
        layerItem->setData(index, kInspectorBackgroundLayerIndexRole);

        auto *visibilityItem = new QStandardItem;
        visibilityItem->setEditable(false);
        visibilityItem->setTextAlignment(Qt::AlignCenter);
        visibilityItem->setData(index, kInspectorBackgroundLayerIndexRole);
        const bool visible = owner_->isBackgroundLayerVisible(index);
        visibilityItem->setIcon(inspectorActionIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off")));
        visibilityItem->setToolTip(visible ? owner_->tr("Hide background layer") : owner_->tr("Show background layer"));

        auto *deleteItem = new QStandardItem;
        deleteItem->setEditable(false);
        deleteItem->setTextAlignment(Qt::AlignCenter);
        deleteItem->setData(index, kInspectorBackgroundLayerIndexRole);
        deleteItem->setIcon(inspectorActionIcon(QStringLiteral("trash-2")));
        deleteItem->setToolTip(owner_->tr("Remove background layer"));

        owner_->mapBackgroundLayersModel_->appendRow({layerItem, visibilityItem, deleteItem});
    }
    if (owner_->mapBackgroundLayersTree_ != nullptr && owner_->mapBackgroundLayersTree_->selectionModel() != nullptr) {
        const int selectedLayer = owner_->selectedBackgroundLayerIndex();
        const QModelIndex selectedIndex = selectedLayer >= 0
            ? owner_->mapBackgroundLayersModel_->index(selectedLayer, kInspectorBackgroundNameColumn)
            : QModelIndex();
        owner_->mapBackgroundLayersTree_->setCurrentIndex(selectedIndex);
        if (selectedIndex.isValid()) {
            owner_->mapBackgroundLayersTree_->selectionModel()->select(selectedIndex,
                                                               QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            owner_->mapBackgroundLayersTree_->selectionModel()->clearSelection();
        }
    }

    const int selectedIndex = owner_->selectedBackgroundLayerIndex();
    const bool hasLayer = selectedIndex >= 0 && selectedIndex < layerCount;
    owner_->mapBackgroundMoveUpButton_->setEnabled(hasLayer && selectedIndex > 0);
    owner_->mapBackgroundMoveDownButton_->setEnabled(hasLayer && selectedIndex >= 0 && selectedIndex < layerCount - 1);
    owner_->mapBackgroundPosXSpin_->setEnabled(hasLayer);
    owner_->mapBackgroundPosYSpin_->setEnabled(hasLayer);
    owner_->mapBackgroundOpacitySlider_->setEnabled(hasLayer);
    owner_->mapBackgroundGammaSlider_->setEnabled(hasLayer);
    owner_->mapBackgroundOpacityResetButton_->setEnabled(hasLayer);
    owner_->mapBackgroundGammaResetButton_->setEnabled(hasLayer);

    if (!hasLayer) {
        owner_->mapBackgroundPosXSpin_->setValue(0.0);
        owner_->mapBackgroundPosYSpin_->setValue(0.0);
        owner_->mapBackgroundOpacitySlider_->setValue(58);
        owner_->mapBackgroundGammaSlider_->setValue(100);
        return;
    }

    const QPointF position = owner_->backgroundLayerPosition(selectedIndex);
    owner_->mapBackgroundPosXSpin_->setValue(position.x());
    owner_->mapBackgroundPosYSpin_->setValue(position.y());
    owner_->mapBackgroundOpacitySlider_->setValue(qBound(5, qRound(owner_->backgroundLayerOpacity(selectedIndex) * 100.0), 100));
    owner_->mapBackgroundGammaSlider_->setValue(qBound(20, qRound(owner_->backgroundLayerGamma(selectedIndex) * 100.0), 250));
}
}
