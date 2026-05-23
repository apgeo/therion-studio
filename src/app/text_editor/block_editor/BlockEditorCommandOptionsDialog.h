#pragma once

#include "../TextEditorCommandMetadata.h"

#include <functional>
#include <QString>

#include <optional>

class QWidget;

namespace TherionStudio
{
struct BlockEditorCommandOptionsDialogContext
{
    QWidget *dialogParent = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<bool(const QString &)> isRequiredArgumentSignatureForBlocks;
    std::function<QString(const char *)> translate;
};

enum class BlockEditorCommandIdFieldMode
{
    None,
    Optional,
    Required,
};

class BlockEditorCommandOptionsDialog final
{
public:
    explicit BlockEditorCommandOptionsDialog(BlockEditorCommandOptionsDialogContext context);

    std::optional<QString> configureLine(const QString &commandName,
                                         const QString &sourceLine,
                                         int lineNumber,
                                         BlockEditorCommandIdFieldMode idFieldMode);

private:
    QString tr(const char *text) const;

    BlockEditorCommandOptionsDialogContext context_;
};
}
