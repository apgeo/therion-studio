#pragma once

#include <functional>

namespace TherionStudio
{
class TextEditorTabInteractionController final
{
public:
    struct TextChangedActions
    {
        std::function<void()> rebuildBlocksCanvasFromText;
        std::function<void()> applyDirtyStateFromCurrentState;
        std::function<void()> emitDocumentTextChanged;
    };

    struct ModeRequestActions
    {
        std::function<void(bool)> setBlocksModeActive;
    };

    struct ModeSelectorActions
    {
        std::function<void(bool)> setModeRowVisible;
        std::function<void(int)> setModeRowMaximumHeight;
        std::function<void(int)> setModeRowMinimumHeight;
        std::function<void()> invalidateRootLayout;
        std::function<void()> activateRootLayout;
    };

    static bool handleTextChanged(bool loading,
                                  const TextChangedActions &actions);

    static void handleModeRequest(bool blocksModeRequested,
                                  const ModeRequestActions &actions);

    static void applyModeSelectorVisibility(bool visible,
                                            int modeRowSizeHintHeight,
                                            const ModeSelectorActions &actions);
};
}
