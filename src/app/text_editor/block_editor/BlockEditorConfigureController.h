#pragma once

#include "BlockEditorDataBlockDialog.h"
#include "BlockEditorSourceController.h"

#include <functional>
#include <QString>

class QWidget;

namespace TherionStudio
{
struct BlockEditorConfigureContext
{
    QWidget *dialogParent = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    BlockEditorDataBlockDialogContext dataBlockDialogContext;
    std::function<QString(const char *)> translate;
};

class BlockEditorConfigureController final
{
public:
    explicit BlockEditorConfigureController(BlockEditorConfigureContext context);

    void configureBlock(const QString &kind, int lineNumber, bool showCommandHelpOnly = false);

private:
    QString tr(const char *text) const;

    BlockEditorConfigureContext context_;
};
}
