#pragma once

#include <QColor>
#include <QString>

namespace TherionStudio::UiStyleTokens
{

QString rgbaColorCss(const QColor &color, qreal alpha);

QColor mapModeSelectAccentColor();
QColor mapModeInsertAccentColor();

} // namespace TherionStudio::UiStyleTokens
