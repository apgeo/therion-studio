#include "ApplicationStyleTokens.h"

#include <algorithm>

namespace TherionStudio::UiStyleTokens
{

QString rgbaColorCss(const QColor &color, qreal alpha)
{
    const qreal clampedAlpha = std::clamp(alpha, 0.0, 1.0);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(clampedAlpha, 0, 'f', 3);
}

QColor mapModeSelectAccentColor()
{
    return QColor(QStringLiteral("#2e9f5c"));
}

QColor mapModeInsertAccentColor()
{
    return QColor(QStringLiteral("#d34a4a"));
}

} // namespace TherionStudio::UiStyleTokens
