#pragma once

#include <functional>

namespace TherionStudio
{
class TextEditorDocumentWorkflowController final
{
public:
    struct LoadActions
    {
        std::function<void()> refreshBlocksModeAvailability;
        std::function<void(bool)> setBlocksModeActive;
        std::function<void()> rebuildBlocksCanvasFromText;
        std::function<void()> clearBlockDetailsPane;
        std::function<void()> populateBlockToolbox;
        std::function<void()> refreshEditorModeUi;
        std::function<void()> refreshTitle;
        std::function<void()> refreshCurrentLineHighlight;
        std::function<void(bool)> dirtyStateChanged;
        std::function<void()> updateContextHelp;
    };

    struct SaveActions
    {
        std::function<void()> refreshTitle;
        std::function<void(bool)> dirtyStateChanged;
    };

    struct ProjectRootActions
    {
        std::function<void()> refreshStatus;
    };

    static void runPostLoadWorkflow(bool disableBlocksMode,
                                    const LoadActions &actions);

    static void runPostSaveWorkflow(const SaveActions &actions);

    static void runPostProjectRootSetWorkflow(const ProjectRootActions &actions);
};
}
