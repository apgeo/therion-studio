#pragma once

#include <QStringList>

#include <optional>

namespace TherionStudio
{
class TextEditorTab;

struct BlockEditorDataBlockDialogResult
{
    QStringList rowLines;
    int dataBodyLastLine = 0;
    bool schemaMismatchDetected = false;
};

class BlockEditorDataBlockDialog final
{
public:
    explicit BlockEditorDataBlockDialog(TextEditorTab *owner);

    std::optional<BlockEditorDataBlockDialogResult> configureRows(const QStringList &lines,
                                                                  int lineNumber);

private:
    TextEditorTab *owner_ = nullptr;
};
}
