#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
struct MapEditorLineSplitPlan
{
    bool resolved = false;
    bool changed = false;
    bool areaReferencesUpdated = false;
    QString originalLineId;
    QString splitLineId;
    QString updatedText;
    QString errorMessage;
};

class MapEditorLineSplitPlanner final
{
public:
    [[nodiscard]] static MapEditorLineSplitPlan planSplit(const QString &text,
                                                          int lineNumber,
                                                          const QStringList &firstCoordinateRows,
                                                          const QStringList &secondCoordinateRows);
};
}
