#pragma once

#include "BlockEditorCommandOptionsDialog.h"
#include "BlockEditorDataBlockDialog.h"
#include "BlockEditorSingleValueCommandDialog.h"
#include "BlockEditorSourceController.h"
#include "../TextEditorCommandMetadata.h"

#include <functional>
#include <QString>

class QWidget;

namespace TherionStudio
{
struct BlockEditorConfigureContext
{
    QWidget *dialogParent = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<bool(const QString &)> commandSupportsInlineIdField;
    std::function<bool(const QString &)> commandHasRequiredIdArgument;
    BlockEditorCommandOptionsDialogContext commandOptionsDialogContext;
    BlockEditorDataBlockDialogContext dataBlockDialogContext;
    BlockEditorSingleValueCommandDialogContext singleValueCommandDialogContext;
    std::function<QString(const char *)> translate;
};

class BlockEditorConfigureController final
{
public:
    explicit BlockEditorConfigureController(BlockEditorConfigureContext context);

    void configureBlock(const QString &kind, int lineNumber);

private:
    QString tr(const char *text) const;

    BlockEditorConfigureContext context_;
};
}
