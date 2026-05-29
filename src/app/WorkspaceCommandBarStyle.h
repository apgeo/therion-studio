#pragma once

#include <QColor>
#include <QString>

namespace TherionStudio
{

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

} // namespace TherionStudio
