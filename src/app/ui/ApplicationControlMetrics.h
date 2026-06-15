#pragma once

#include <QMargins>
#include <QSize>

namespace TherionStudio::UiMetrics
{

constexpr int compactControlRadius()
{
    return 2;
}

constexpr int standardControlRadius()
{
    return 4;
}

constexpr int toolbarButtonRadius()
{
    return 6;
}

constexpr int sidebarRailButtonRadius()
{
    return 9;
}

constexpr int segmentedControlMinimumHeight()
{
    return 28;
}

constexpr int segmentedControlVerticalPadding()
{
    return 4;
}

constexpr int segmentedControlHorizontalPadding()
{
    return 10;
}

constexpr int compactSpacing()
{
    return 4;
}

constexpr int standardSpacing()
{
    return 6;
}

constexpr int inspectorPanelMargin()
{
    return 6;
}

constexpr QMargins inspectorPanelMargins()
{
    return QMargins(inspectorPanelMargin(), inspectorPanelMargin(), inspectorPanelMargin(), inspectorPanelMargin());
}

constexpr QMargins inspectorSectionMargins()
{
    return inspectorPanelMargins();
}

constexpr int workspaceCommandButtonSize()
{
    return 22;
}

constexpr int workspaceCommandIconSize()
{
    return 14;
}

constexpr int sidebarActivityIconSize()
{
    return 20;
}

constexpr int sidebarActivityButtonSize()
{
    return 34;
}

inline QSize squareSize(int extent)
{
    return QSize(extent, extent);
}

} // namespace TherionStudio::UiMetrics
