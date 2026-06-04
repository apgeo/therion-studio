#include "MapEditorRasterBackgroundImage.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QtMath>

#include <cmath>

namespace TherionStudio
{
namespace
{
constexpr qsizetype kRasterSourceImageCacheLimit = 8;
constexpr qsizetype kAdjustedRasterImageCacheLimit = 12;

struct CachedRasterSourceImageEntry
{
    QString cacheKey;
    QImage image;
};

struct CachedAdjustedRasterImageEntry
{
    QString cacheKey;
    QImage image;
};

QString normalizedRasterPathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    QString resolvedPath = fileInfo.canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = fileInfo.absoluteFilePath();
    }
    if (resolvedPath.isEmpty()) {
        resolvedPath = path;
    }

    return QDir::cleanPath(resolvedPath)
        .normalized(QString::NormalizationForm_C)
        .toCaseFolded();
}

QString rasterSourceImageCacheKey(const QString &layerPath)
{
    if (layerPath.isEmpty() || isMapEditorXviBackgroundPath(layerPath)) {
        return QString();
    }

    const QFileInfo fileInfo(layerPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return QString();
    }

    return QStringLiteral("raster-source-v1|%1|%2|%3")
        .arg(normalizedRasterPathKey(fileInfo.absoluteFilePath()),
             QString::number(fileInfo.lastModified().toMSecsSinceEpoch()),
             QString::number(fileInfo.size()));
}

QHash<QString, CachedRasterSourceImageEntry> &rasterSourceImageCache()
{
    static QHash<QString, CachedRasterSourceImageEntry> cache;
    return cache;
}

QHash<QString, CachedAdjustedRasterImageEntry> &adjustedRasterImageCache()
{
    static QHash<QString, CachedAdjustedRasterImageEntry> cache;
    return cache;
}

QStringList &rasterSourceImageCacheOrder()
{
    static QStringList order;
    return order;
}

QStringList &adjustedRasterImageCacheOrder()
{
    static QStringList order;
    return order;
}

QMutex &adjustedRasterImageCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

void touchRasterSourceImageCacheKey(const QString &cacheKey)
{
    if (cacheKey.isEmpty()) {
        return;
    }

    QStringList &order = rasterSourceImageCacheOrder();
    order.removeAll(cacheKey);
    order.append(cacheKey);

    QHash<QString, CachedRasterSourceImageEntry> &cache = rasterSourceImageCache();
    while (order.size() > kRasterSourceImageCacheLimit) {
        cache.remove(order.takeFirst());
    }
}

QString adjustedRasterImageCacheKey(const QString &layerPath, const QSize &targetSize, qreal gamma)
{
    const QString sourceKey = rasterSourceImageCacheKey(layerPath);
    if (sourceKey.isEmpty() || targetSize.width() <= 1 || targetSize.height() <= 1) {
        return QString();
    }

    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    return QStringLiteral("raster-adjusted-v1|%1|%2x%3|%4")
        .arg(sourceKey,
             QString::number(targetSize.width()),
             QString::number(targetSize.height()),
             QString::number(qRound(boundedGamma * 1000.0)));
}

void touchAdjustedRasterImageCacheKey(const QString &cacheKey)
{
    if (cacheKey.isEmpty()) {
        return;
    }

    QStringList &order = adjustedRasterImageCacheOrder();
    order.removeAll(cacheKey);
    order.append(cacheKey);

    QHash<QString, CachedAdjustedRasterImageEntry> &cache = adjustedRasterImageCache();
    while (order.size() > kAdjustedRasterImageCacheLimit) {
        cache.remove(order.takeFirst());
    }
}

std::optional<QImage> cachedAdjustedRasterImage(const QString &cacheKey)
{
    if (cacheKey.isEmpty()) {
        return std::nullopt;
    }

    QMutexLocker locker(&adjustedRasterImageCacheMutex());
    const auto cacheIt = adjustedRasterImageCache().constFind(cacheKey);
    if (cacheIt == adjustedRasterImageCache().constEnd() || cacheIt->image.isNull()) {
        return std::nullopt;
    }

    touchAdjustedRasterImageCacheKey(cacheKey);
    return cacheIt->image;
}

void rememberAdjustedRasterImage(const QString &cacheKey, const QImage &image)
{
    if (cacheKey.isEmpty() || image.isNull()) {
        return;
    }

    QMutexLocker locker(&adjustedRasterImageCacheMutex());
    adjustedRasterImageCache().insert(cacheKey, CachedAdjustedRasterImageEntry{cacheKey, image});
    touchAdjustedRasterImageCacheKey(cacheKey);
}
}

bool isMapEditorXviBackgroundPath(const QString &layerPath)
{
    return layerPath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive);
}

std::optional<QImage> cachedMapEditorRasterSourceImage(const QString &layerPath)
{
    const QString cacheKey = rasterSourceImageCacheKey(layerPath);
    if (cacheKey.isEmpty()) {
        return std::nullopt;
    }

    const auto cacheIt = rasterSourceImageCache().constFind(cacheKey);
    if (cacheIt == rasterSourceImageCache().constEnd() || cacheIt->image.isNull()) {
        return std::nullopt;
    }

    touchRasterSourceImageCacheKey(cacheKey);
    return cacheIt->image;
}

void rememberMapEditorRasterSourceImage(const QString &layerPath, const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    const QString cacheKey = rasterSourceImageCacheKey(layerPath);
    if (cacheKey.isEmpty()) {
        return;
    }

    rasterSourceImageCache().insert(cacheKey, CachedRasterSourceImageEntry{cacheKey, image});
    touchRasterSourceImageCacheKey(cacheKey);
}

MapEditorRasterSourceImageLoadResult readMapEditorRasterSourceImageUncached(const QString &layerPath)
{
    MapEditorRasterSourceImageLoadResult result;
    result.imagePath = QFileInfo(layerPath).absoluteFilePath();
    if (layerPath.isEmpty() || isMapEditorXviBackgroundPath(layerPath)) {
        return result;
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    result.image = imageReader.read();
    return result;
}

QImage readMapEditorRasterSourceImage(const QString &layerPath)
{
    if (layerPath.isEmpty() || isMapEditorXviBackgroundPath(layerPath)) {
        return QImage();
    }

    if (const std::optional<QImage> cachedImage = cachedMapEditorRasterSourceImage(layerPath); cachedImage.has_value()) {
        return cachedImage.value();
    }

    MapEditorRasterSourceImageLoadResult result = readMapEditorRasterSourceImageUncached(layerPath);
    rememberMapEditorRasterSourceImage(result.imagePath, result.image);
    return result.image;
}

QSizeF mapEditorRasterModelSize(const QString &layerPath, qreal imageScale)
{
    Q_UNUSED(imageScale);

    if (layerPath.isEmpty()) {
        return QSizeF();
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    const QSize imageSize = imageReader.size();
    if (!imageSize.isValid()) {
        return QSizeF();
    }

    return QSizeF(imageSize.width(), imageSize.height());
}

QImage gammaCorrectAndScaleMapEditorRasterSourceImage(const QString &layerPath,
                                                      QImage sourceImage,
                                                      const QSize &targetSize,
                                                      qreal gamma)
{
    if (sourceImage.isNull()) {
        return QImage();
    }

    const QString adjustedCacheKey = adjustedRasterImageCacheKey(layerPath, targetSize, gamma);
    if (const std::optional<QImage> cachedImage = cachedAdjustedRasterImage(adjustedCacheKey); cachedImage.has_value()) {
        return cachedImage.value();
    }

    sourceImage = sourceImage.convertToFormat(QImage::Format_RGBA8888);
    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    unsigned char lookupTable[256];
    for (int value = 0; value < 256; ++value) {
        const qreal normalized = static_cast<qreal>(value) / 255.0;
        const qreal corrected = std::pow(normalized, 1.0 / boundedGamma);
        lookupTable[value] = static_cast<unsigned char>(qBound(0, qRound(corrected * 255.0), 255));
    }

    for (int y = 0; y < sourceImage.height(); ++y) {
        uchar *scanLine = sourceImage.scanLine(y);
        for (int x = 0; x < sourceImage.width(); ++x) {
            uchar *pixel = scanLine + (x * 4);
            pixel[0] = lookupTable[pixel[0]];
            pixel[1] = lookupTable[pixel[1]];
            pixel[2] = lookupTable[pixel[2]];
        }
    }

    if (targetSize.width() > 1 && targetSize.height() > 1) {
        QImage scaledImage = sourceImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        rememberAdjustedRasterImage(adjustedCacheKey, scaledImage);
        return scaledImage;
    }
    return sourceImage;
}

quint64 nextMapEditorRasterGammaRequestId()
{
    static quint64 nextRequestId = 0;
    return ++nextRequestId;
}

quint64 nextMapEditorRasterLoadRequestId()
{
    static quint64 nextRequestId = 0;
    return ++nextRequestId;
}

}
