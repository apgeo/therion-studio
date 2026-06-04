#pragma once

#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <optional>

namespace TherionStudio
{

struct MapEditorRasterSourceImageLoadResult
{
    QString imagePath;
    QImage image;
};

bool isMapEditorXviBackgroundPath(const QString &layerPath);
std::optional<QImage> cachedMapEditorRasterSourceImage(const QString &layerPath);
void rememberMapEditorRasterSourceImage(const QString &layerPath, const QImage &image);
MapEditorRasterSourceImageLoadResult readMapEditorRasterSourceImageUncached(const QString &layerPath);
QImage readMapEditorRasterSourceImage(const QString &layerPath);
QSizeF mapEditorRasterModelSize(const QString &layerPath, qreal imageScale);
QImage gammaCorrectAndScaleMapEditorRasterSourceImage(const QString &layerPath,
                                                      QImage sourceImage,
                                                      const QSize &targetSize,
                                                      qreal gamma);
quint64 nextMapEditorRasterGammaRequestId();
quint64 nextMapEditorRasterLoadRequestId();

}
