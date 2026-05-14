#include "../src/app/MapEditorInputPolicy.h"

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runWheelPolicyTest()
{
    if (!expect(resolveMapEditorWheelAction(false, false, false) == MapEditorWheelAction::Zoom,
                "Standard non-precise wheel should zoom in default mode.")) {
        return 1;
    }

    if (!expect(resolveMapEditorWheelAction(false, true, false) == MapEditorWheelAction::Pan,
                "Precise scrolling should pan in default mode.")) {
        return 1;
    }

    if (!expect(resolveMapEditorWheelAction(false, true, true) == MapEditorWheelAction::Zoom,
                "Zoom modifier should force zoom in default mode.")) {
        return 1;
    }

    if (!expect(resolveMapEditorWheelAction(true, false, false) == MapEditorWheelAction::Pan,
                "Touch-friendly mode should pan for non-modified non-precise wheel input.")) {
        return 1;
    }

    if (!expect(resolveMapEditorWheelAction(true, true, false) == MapEditorWheelAction::Pan,
                "Touch-friendly mode should pan for precise scrolling input.")) {
        return 1;
    }

    if (!expect(resolveMapEditorWheelAction(true, false, true) == MapEditorWheelAction::Zoom,
                "Zoom modifier should still force zoom in touch-friendly mode.")) {
        return 1;
    }

    return 0;
}

int runTouchPanCandidatePolicyTest()
{
    if (!expect(shouldEnableTouchPanCandidate(true, false, false),
                "Touch-friendly mode should enable touch-pan candidate outside select mode.")) {
        return 1;
    }

    if (!expect(shouldEnableTouchPanCandidate(false, true, false),
                "Select mode should enable touch-pan candidate in default mode.")) {
        return 1;
    }

    if (!expect(!shouldEnableTouchPanCandidate(false, false, false),
                "Default mode outside select mode should not enable touch-pan candidate.")) {
        return 1;
    }

    if (!expect(!shouldEnableTouchPanCandidate(true, true, true),
                "Primary pointer interaction should suppress touch-pan candidate in all modes.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int wheelPolicyResult = runWheelPolicyTest(); wheelPolicyResult != 0) {
        return wheelPolicyResult;
    }

    return runTouchPanCandidatePolicyTest();
}
