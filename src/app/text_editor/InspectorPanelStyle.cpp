#include "InspectorPanelStyle.h"

#include "../ui/ApplicationSegmentedControlStyle.h"

namespace TherionStudio
{

QString inspectorPanelStyleSheet()
{
    return QStringLiteral(
        "TherionStudio--InspectorPanel {"
        " background-color: palette(base);"
        " border: none;"
        "}");
}

QString inspectorPanelSegmentedControlStyleSheet()
{
    return segmentedControlButtonStyleSheet(QStringLiteral("QPushButton#inspectorSegmentButton"));
}

QString inspectorPanelTabsStyleSheet()
{
    return QStringLiteral(
        "QTabWidget#inspectorPanelTabs {"
        " border: none;"
        "}"
        "QTabWidget#inspectorPanelTabs::pane {"
        " border: none;"
        " margin: 0;"
        " padding: 0;"
        "}");
}

} // namespace TherionStudio
