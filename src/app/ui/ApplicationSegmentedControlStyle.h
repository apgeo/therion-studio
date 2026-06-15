#pragma once

#include "ApplicationControlMetrics.h"

#include <QString>

namespace TherionStudio
{

inline QString segmentedControlButtonStyleSheet(const QString &buttonSelector)
{
    return QStringLiteral(
               "%1 {"
               " background-color: palette(window);"
               " border: 1px solid palette(mid);"
               " border-left-width: 0;"
               " border-radius: 0;"
               " padding: %2px %3px;"
               "}"
               "%1[firstSegment=\"true\"] {"
               " border-left-width: 1px;"
               " border-top-left-radius: %4px;"
               " border-bottom-left-radius: %4px;"
               "}"
               "%1[lastSegment=\"true\"] {"
               " border-top-right-radius: %4px;"
               " border-bottom-right-radius: %4px;"
               "}"
               "%1:checked {"
               " background-color: palette(base);"
               " font-weight: 600;"
               "}")
        .arg(buttonSelector,
             QString::number(UiMetrics::segmentedControlVerticalPadding()),
             QString::number(UiMetrics::segmentedControlHorizontalPadding()),
             QString::number(UiMetrics::standardControlRadius()));
}

} // namespace TherionStudio
