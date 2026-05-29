#pragma once

#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace TherionStudio
{

struct MapEditorSceneThemeColors
{
    QColor canvasBorder;
    QColor canvasFill;
    QColor geometryStroke;
    QColor areaFill;
    QColor labelText;
    QColor mutedText;
    QColor pointHandleStroke;
    QColor pointHandleFill;
    QColor controlConnector;
    QColor controlHandleStroke;
    QColor controlHandleFill;
};

inline bool mapEditorUsesLightAppearance()
{
    const QPalette appPalette = QGuiApplication::palette();
    const QColor appWindow = appPalette.color(QPalette::Window);
    const QColor appBase = appPalette.color(QPalette::Base);
    const qreal appReferenceLightness = appWindow.isValid()
        ? appWindow.lightnessF()
        : (appBase.isValid() ? appBase.lightnessF() : 1.0);
    const bool appLooksLight = appReferenceLightness > 0.58;
    const bool appLooksDark = appReferenceLightness < 0.42;

    bool lightMode = appLooksLight;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints() != nullptr) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            lightMode = false;
        } else if (scheme == Qt::ColorScheme::Light) {
            lightMode = true;
        }
    }
#endif

    // On some platform/style combinations the color-scheme hint can disagree
    // with the effective application palette; prefer the palette in that case.
    if ((lightMode && appLooksDark) || (!lightMode && appLooksLight)) {
        lightMode = appLooksLight;
    }

    return lightMode;
}

inline MapEditorSceneThemeColors mapEditorSceneThemeColors(bool lightMode)
{
    if (lightMode) {
        return {
            QColor(QStringLiteral("#bec9d8")),
            QColor(QStringLiteral("#f4f8fd")),
            QColor(QStringLiteral("#1d2837")),
            QColor(48, 73, 105, 28),
            QColor(QStringLiteral("#344a67")),
            QColor(QStringLiteral("#556b84")),
            QColor(18, 26, 37, 220),
            QColor(24, 30, 42, 190),
            QColor(52, 110, 186, 190),
            QColor(20, 73, 148, 230),
            QColor(96, 176, 248, 220),
        };
    }

    return {
        QColor(QStringLiteral("#596477")),
        QColor(QStringLiteral("#232833")),
        QColor(QStringLiteral("#e1e9f5")),
        QColor(220, 227, 238, 24),
        QColor(QStringLiteral("#e6eefb")),
        QColor(QStringLiteral("#a6b4c8")),
        QColor(228, 236, 246, 220),
        QColor(206, 219, 235, 190),
        QColor(118, 178, 242, 190),
        QColor(76, 150, 229, 230),
        QColor(130, 201, 255, 220),
    };
}

inline QColor mapEditorDirectionTickColor()
{
    return QColor(QStringLiteral("#ffda00"));
}

} // namespace TherionStudio
