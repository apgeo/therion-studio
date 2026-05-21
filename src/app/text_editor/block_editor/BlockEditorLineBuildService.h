#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorLineBuildService final
{
public:
    explicit BlockEditorLineBuildService(const TextEditorTab *owner);

    bool buildUpdatedLine(QString *updatedLine, QString *validationError) const;

private:
    const TextEditorTab *owner_ = nullptr;
};
}
