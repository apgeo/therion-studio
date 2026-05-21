#pragma once

#include "BlockEditorInsertionPlanner.h"

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class BlockEditorInsertionTemplateBuilder final
{
public:
    QStringList buildLines(const QString &normalizedKind,
                           const QStringList &existingLines,
                           const BlockEditorInsertionPlan &insertionPlan) const;
};
}
