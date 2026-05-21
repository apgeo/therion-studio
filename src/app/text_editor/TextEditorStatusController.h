#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class TextEditorStatusController final
{
public:
    explicit TextEditorStatusController(TextEditorTab *owner);

    QString displayName() const;
    QString statusPathText() const;
    QString statusEncodingText() const;
    void setInlineStatusVisible(bool visible);
    void refreshTitle();
    void refreshStatus();
    bool isCurrentStateDirty() const;
    void applyDirtyStateFromCurrentState();
    QString displayPath() const;

private:
    TextEditorTab *owner_ = nullptr;
};
}
