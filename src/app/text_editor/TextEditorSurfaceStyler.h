#pragma once

#include <QPalette>

class QWidget;

namespace TherionStudio
{
void syncPanelSurfaceToBaseTone(QWidget *panelWidget);
void syncPanelSurfaceToPalette(QWidget *panelWidget, const QPalette &sourcePalette);
void syncTextBrowserSurfaceToParent(QWidget *browserWidget);
void syncTextBrowserSurfaceToPalette(QWidget *browserWidget, const QPalette &sourcePalette);
}
