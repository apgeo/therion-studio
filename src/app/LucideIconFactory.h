#pragma once

#include <QColor>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{

inline QPixmap renderLucidePixmap(const QString &iconName, const QColor &color, int extent, qreal devicePixelRatio)
{
    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        return {};
    }

    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    const int pixelExtent = qMax(1, static_cast<int>(std::ceil(extent * dpr)));
    QPixmap pixmap(pixelExtent, pixelExtent);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0.0, 0.0, pixelExtent, pixelExtent));
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

inline QIcon themedLucideIcon(const QString &iconName, const QPalette &palette, int extent, qreal devicePixelRatio)
{
    QIcon icon;
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::ButtonText), extent, devicePixelRatio), QIcon::Normal);
    icon.addPixmap(renderLucidePixmap(iconName,
                                      palette.color(QPalette::Disabled, QPalette::ButtonText),
                                      extent,
                                      devicePixelRatio),
                   QIcon::Disabled);
    return icon;
}

} // namespace TherionStudio
