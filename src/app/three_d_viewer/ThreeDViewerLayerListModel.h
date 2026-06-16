#pragma once

#include "../../core/ThreeDViewerSceneModel.h"

#include <QAbstractListModel>
#include <QIcon>

#include <array>

namespace TherionStudio
{

class ThreeDViewerLayerListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    static constexpr int kLayerCount = 5;

    enum class Layer
    {
        Centerline = 0,
        Stations = 1,
        Labels = 2,
        Meshes = 3,
        Surfaces = 4,
        Count = kLayerCount
    };

    enum Role
    {
        IconNameRole = Qt::UserRole + 1,
        LayerRole,
        VisibleRole,
        CountRole,
        LabelRole
    };

    explicit ThreeDViewerLayerListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);
    const ThreeDViewerSceneModel &sceneModel() const;

    bool layerVisible(Layer layer) const;
    Q_INVOKABLE bool layerVisible(int layerIndex) const;
    void setLayerVisible(Layer layer, bool visible);
    Q_INVOKABLE void setLayerVisible(int layerIndex, bool visible);
    std::array<bool, static_cast<int>(Layer::Count)> layerVisibility() const;

signals:
    void layerVisibilityChanged(int layerIndex, bool visible);

private:
    static constexpr int layerCount()
    {
        return kLayerCount;
    }

    static int layerIndex(Layer layer)
    {
        return static_cast<int>(layer);
    }

    QModelIndex layerModelIndex(Layer layer) const;

    QString layerLabel(Layer layer) const;
    QString layerIconName(Layer layer) const;
    int layerItemCount(Layer layer) const;

    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, kLayerCount> layerVisibility_ = {true, true, true, true, true};
};

} // namespace TherionStudio
