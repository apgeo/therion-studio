#include "ThreeDViewerLayerListModel.h"

#include "../LucideIconFactory.h"

#include <QApplication>

namespace TherionStudio
{

ThreeDViewerLayerListModel::ThreeDViewerLayerListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ThreeDViewerLayerListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : layerCount();
}

QVariant ThreeDViewerLayerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= layerCount()) {
        return {};
    }

    const Layer layer = static_cast<Layer>(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return QStringLiteral("%1 (%2)").arg(layerLabel(layer)).arg(layerItemCount(layer));
    case Qt::DecorationRole:
        return themedLucideIcon(layerIconName(layer), QApplication::palette(), 16, 1.0);
    case Qt::CheckStateRole:
    case VisibleRole:
        return layerVisible(layer) ? Qt::Checked : Qt::Unchecked;
    case LayerRole:
        return static_cast<int>(layer);
    case CountRole:
        return layerItemCount(layer);
    case IconNameRole:
        return layerIconName(layer);
    default:
        return {};
    }
}

Qt::ItemFlags ThreeDViewerLayerListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

bool ThreeDViewerLayerListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= layerCount()) {
        return false;
    }

    if (role != Qt::CheckStateRole && role != VisibleRole) {
        return false;
    }

    const Layer layer = static_cast<Layer>(index.row());
    const bool visible = value.toInt() == Qt::Checked || value.toBool();
    if (layerVisible(layer) == visible) {
        return false;
    }

    layerVisibility_[index.row()] = visible;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::DecorationRole, Qt::CheckStateRole, VisibleRole});
    emit layerVisibilityChanged(index.row(), visible);
    return true;
}

QHash<int, QByteArray> ThreeDViewerLayerListModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {Qt::DecorationRole, "decoration"},
        {Qt::CheckStateRole, "layerChecked"},
        {IconNameRole, "iconName"},
        {LayerRole, "layerIndex"},
        {VisibleRole, "visible"},
        {CountRole, "count"},
        {LabelRole, "label"},
    };
}

void ThreeDViewerLayerListModel::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    sceneModel_ = sceneModel;
    if (rowCount() > 0) {
        emit dataChanged(index(0, 0), index(layerCount() - 1, 0), {Qt::DisplayRole, Qt::DecorationRole, CountRole, LabelRole});
    }
}

const ThreeDViewerSceneModel &ThreeDViewerLayerListModel::sceneModel() const
{
    return sceneModel_;
}

bool ThreeDViewerLayerListModel::layerVisible(Layer layer) const
{
    const int index = layerIndex(layer);
    return index >= 0 && index < layerCount() ? layerVisibility_[index] : false;
}

bool ThreeDViewerLayerListModel::layerVisible(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= layerCount()) {
        return false;
    }
    return layerVisible(static_cast<Layer>(layerIndex));
}

void ThreeDViewerLayerListModel::setLayerVisible(Layer layer, bool visible)
{
    const int index = layerIndex(layer);
    if (index < 0 || index >= layerCount() || layerVisible(layer) == visible) {
        return;
    }

    layerVisibility_[index] = visible;
    const QModelIndex modelIndex = layerModelIndex(layer);
    emit dataChanged(modelIndex, modelIndex, {Qt::DisplayRole, Qt::DecorationRole, Qt::CheckStateRole, VisibleRole});
    emit layerVisibilityChanged(index, visible);
}

void ThreeDViewerLayerListModel::setLayerVisible(int layerIndex, bool visible)
{
    if (layerIndex < 0 || layerIndex >= layerCount()) {
        return;
    }
    setLayerVisible(static_cast<Layer>(layerIndex), visible);
}

std::array<bool, static_cast<int>(ThreeDViewerLayerListModel::Layer::Count)> ThreeDViewerLayerListModel::layerVisibility() const
{
    return layerVisibility_;
}

QString ThreeDViewerLayerListModel::layerLabel(Layer layer) const
{
    switch (layer) {
    case Layer::Centerline:
        return tr("Centerline");
    case Layer::Stations:
        return tr("Stations");
    case Layer::Labels:
        return tr("Labels");
    case Layer::Meshes:
        return tr("Meshes");
    case Layer::Surfaces:
        return tr("Surfaces");
    case Layer::Count:
        break;
    }
    return {};
}

QString ThreeDViewerLayerListModel::layerIconName(Layer layer) const
{
    return layerVisible(layer) ? QStringLiteral("eye") : QStringLiteral("eye-off");
}

int ThreeDViewerLayerListModel::layerItemCount(Layer layer) const
{
    switch (layer) {
    case Layer::Centerline:
        return sceneModel_.shots.size();
    case Layer::Stations:
        return sceneModel_.stations.size();
    case Layer::Labels:
        return sceneModel_.stations.size();
    case Layer::Meshes:
        return sceneModel_.meshGroups.size();
    case Layer::Surfaces:
        return sceneModel_.surfaces.size();
    case Layer::Count:
        break;
    }
    return 0;
}

QModelIndex ThreeDViewerLayerListModel::layerModelIndex(Layer layer) const
{
    const int row = layerIndex(layer);
    return row >= 0 && row < layerCount() ? index(row, 0) : QModelIndex();
}

} // namespace TherionStudio
