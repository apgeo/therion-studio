#pragma once

#include "BlockEditorSourceController.h"

#include <functional>

#include <QString>
#include <QStringList>

class QWidget;

namespace TherionStudio
{
struct BlockEditorDeleteContext
{
    QWidget *dialogParent = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<QString(const QString &, const QStringList &, int)> resolveScopeForCommandAtLine;
    std::function<bool(const QString &, const QString &)> isCommandDirectiveInScope;
    std::function<QString(const char *)> translate;
};

class BlockEditorDeleteExecutor final
{
public:
    explicit BlockEditorDeleteExecutor(BlockEditorDeleteContext context);

    bool deleteCommandAtLine(int lineNumber);

private:
    QString tr(const char *text) const;

    BlockEditorDeleteContext context_;
};
}
