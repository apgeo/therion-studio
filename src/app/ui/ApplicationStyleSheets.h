#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

namespace TherionStudio
{

QString workspaceCommandBarStyleSheet(const QColor &backgroundColor,
                                      bool topBorderEnabled,
                                      bool bottomBorderEnabled);
QString mainEditorSurfaceStyleSheet();
QString sidebarActivityRailStyleSheet(const QPalette &palette);
QString sidebarContentPaneStyleSheet();
QColor mapModeStatusAccentColor(bool insertMode);
QString statusBadgeStyleSheet(const QColor &accentColor);
QString compilerStatusBadgeStyleSheet(const QColor &accentColor);

} // namespace TherionStudio
