#pragma once

#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MapEditorObjectDeletePlan
{
    bool resolved = false;
    bool changed = false;
    QString updatedText;
    QString errorMessage;
    QVector<int> removedLineNumbers;
    int focusLineAfterDelete = 0;
};

MapEditorObjectDeletePlan planMapEditorObjectDelete(const QString &text, int lineNumber);
}
