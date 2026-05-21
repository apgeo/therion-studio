#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class RawEditorCompletionInsertionController final
{
public:
    explicit RawEditorCompletionInsertionController(TextEditorTab *owner);

    void insertCompletionToken(const QString &completion);

private:
    TextEditorTab *owner_ = nullptr;
};
}
