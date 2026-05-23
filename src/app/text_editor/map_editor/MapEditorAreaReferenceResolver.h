#pragma once

#include <QSet>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MapEditorAreaReference
{
    int areaLineNumber = 0;
    QString areaLabel;
    QString borderLineId;
};

QSet<int> mapEditorBorderLineNumbersForArea(const QString &text, int areaLineNumber);
QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QString &text, int borderLineNumber);
}
