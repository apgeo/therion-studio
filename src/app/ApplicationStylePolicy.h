#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

#include <algorithm>

namespace TherionStudio
{

inline QString rgbaColorCss(const QColor &color, qreal alpha)
{
    const qreal clampedAlpha = std::clamp(alpha, 0.0, 1.0);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(clampedAlpha, 0, 'f', 3);
}

inline QString workspaceCommandBarStyleSheet(const QColor &backgroundColor,
                                             bool topBorderEnabled,
                                             bool bottomBorderEnabled)
{
    const QString topBorder = topBorderEnabled
        ? QStringLiteral("1px solid palette(midlight)")
        : QStringLiteral("none");
    const QString bottomBorder = bottomBorderEnabled
        ? QStringLiteral("1px solid palette(mid)")
        : QStringLiteral("none");

    return QStringLiteral(
               "QWidget#workspaceCommandBar {"
               " border-bottom: %2;"
               " border-top: %1;"
               " border-left: none;"
               " border-right: none;"
               " background-color: %3;"
               "}"
               "QFrame#workspaceToolbarSeparator {"
               " color: palette(mid);"
               " margin-left: 4px;"
               " margin-right: 4px;"
               "}"
               "QWidget#workspaceCommandBar QToolButton {"
               " min-width: 26px;"
               " max-width: 26px;"
               " min-height: 26px;"
               " max-height: 26px;"
               " border: 1px solid palette(mid);"
               " border-radius: 6px;"
               " padding: 0px;"
               " background-color: palette(button);"
               "}"
               "QWidget#workspaceCommandBar QPushButton {"
               " min-height: 26px;"
               " border: 1px solid palette(mid);"
               " border-radius: 6px;"
               " padding: 0 10px;"
               " background-color: palette(button);"
               "}"
               "QWidget#workspaceCommandBar QToolButton:hover,"
               "QWidget#workspaceCommandBar QPushButton:hover {"
               " background-color: palette(light);"
               "}"
               "QWidget#workspaceCommandBar QToolButton:pressed,"
               "QWidget#workspaceCommandBar QToolButton:checked,"
               "QWidget#workspaceCommandBar QPushButton:pressed,"
               "QWidget#workspaceCommandBar QPushButton:checked {"
               " background-color: palette(midlight);"
               "}"
               "QWidget#workspaceCommandBar QToolButton:disabled,"
               "QWidget#workspaceCommandBar QPushButton:disabled {"
               " color: palette(mid);"
               " border-color: palette(mid);"
               "}")
        .arg(topBorder, bottomBorder, backgroundColor.name(QColor::HexRgb));
}

inline QString mainEditorSurfaceStyleSheet()
{
    return QStringLiteral(
        "QWidget#mainEditorArea {"
        " background-color: palette(base);"
        " border: none;"
        "}"
        "QFrame#mainEditorAreaLeftDivider {"
        " background-color: palette(mid);"
        " border: none;"
        "}"
        "QWidget#mainEditorAreaColumn {"
        " background-color: palette(base);"
        " border: none;"
        "}");
}

inline QString sidebarActivityRailStyleSheet(const QPalette &palette)
{
    const QColor railBase = palette.color(QPalette::Window);
    const QColor railHover = palette.color(QPalette::Highlight);
    const QColor railChecked = palette.color(QPalette::Highlight);
    const QColor railWarning = QColor(217, 119, 6);
    return QStringLiteral(
               "#SidebarActivityRail {"
               "background-color: %1;"
               "}"
               "#SidebarActivityRail QToolButton {"
               "border: none;"
               "background: transparent;"
               "border-radius: 9px;"
               "padding: 0px;"
               "}"
               "#SidebarActivityRail QToolButton:hover {"
               "background-color: %2;"
               "}"
               "#SidebarActivityRail QToolButton:checked {"
               "background-color: %3;"
               "}"
               "#SidebarActivityRail QToolButton[validationProblems=\"true\"] {"
               "background-color: %5;"
               "border: 1px solid %6;"
               "}"
               "#SidebarActivityRail QToolButton[validationProblems=\"true\"]:checked {"
               "background-color: %7;"
               "}"
               "#SidebarActivityRail QFrame#SidebarActivitySeparator {"
               "background-color: %4;"
               "}")
        .arg(rgbaColorCss(railBase, 0.78))
        .arg(rgbaColorCss(railHover, 0.24))
        .arg(rgbaColorCss(railChecked, 0.34))
        .arg(rgbaColorCss(palette.color(QPalette::Mid), 0.7))
        .arg(rgbaColorCss(railWarning, 0.24))
        .arg(rgbaColorCss(railWarning, 0.92))
        .arg(rgbaColorCss(railWarning, 0.38));
}

inline QString sidebarContentPaneStyleSheet()
{
    return QStringLiteral(
        "QWidget#mainSidebarSplitterPane {"
        " background-color: palette(base);"
        " color: palette(windowText);"
        " border-left: none;"
        " border-right: none;"
        " border-top: none;"
        " border-bottom: none;"
        "}"
        "QWidget#mainSidebarSplitterPane QTreeView {"
        " background-color: palette(base);"
        " alternate-background-color: palette(window);"
        " border: 1px solid palette(mid);"
        "}"
        "QWidget#mainSidebarSplitterPane QHeaderView::section {"
        " background-color: palette(window);"
        " border: none;"
        " border-bottom: 1px solid palette(mid);"
        " padding: 4px 8px;"
        " font-weight: 600;"
        "}");
}

inline QColor mapModeStatusAccentColor(bool insertMode)
{
    return insertMode
        ? QColor(QStringLiteral("#d34a4a"))
        : QColor(QStringLiteral("#2e9f5c"));
}

inline QString statusBadgeStyleSheet(const QColor &accentColor)
{
    return QStringLiteral(
               "QLabel {"
               " color: white;"
               " font-weight: 700;"
               " background-color: %1;"
               " border-radius: 4px;"
               " padding: 1px 8px;"
               " min-height: 18px;"
               "}")
        .arg(accentColor.name(QColor::HexRgb));
}

inline QString compilerStatusBadgeStyleSheet(const QColor &accentColor)
{
    return QStringLiteral(
               "QToolButton {"
               " color: white;"
               " font-weight: 700;"
               " background-color: %1;"
               " border: none;"
               " border-radius: 4px;"
               " padding: 1px 8px;"
               " min-height: 18px;"
               "}")
        .arg(accentColor.name(QColor::HexRgb));
}

} // namespace TherionStudio
