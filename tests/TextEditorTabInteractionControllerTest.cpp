#include "../src/app/text_editor/TextEditorTabInteractionController.h"

#include <QCoreApplication>
#include <QWidget>
#include <QStringList>

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

int runHandleTextChangedSkipsWhenLoadingTest()
{
    QStringList calls;

    TextEditorTabInteractionController::TextChangedActions actions;
    actions.rebuildBlocksCanvasFromText = [&calls]() { calls.append(QStringLiteral("rebuild")); };
    actions.applyDirtyStateFromCurrentState = [&calls]() { calls.append(QStringLiteral("dirty")); };
    actions.emitDocumentTextChanged = [&calls]() { calls.append(QStringLiteral("emit")); };

    const bool handled = TextEditorTabInteractionController::handleTextChanged(true, actions);
    if (!expect(!handled,
                "handleTextChanged should return false while loading is active.")) {
        return 1;
    }
    if (!expect(calls.isEmpty(),
                "handleTextChanged should skip all actions while loading.")) {
        return 1;
    }

    return 0;
}

int runHandleTextChangedOrderTest()
{
    QStringList calls;

    TextEditorTabInteractionController::TextChangedActions actions;
    actions.rebuildBlocksCanvasFromText = [&calls]() { calls.append(QStringLiteral("rebuild")); };
    actions.applyDirtyStateFromCurrentState = [&calls]() { calls.append(QStringLiteral("dirty")); };
    actions.emitDocumentTextChanged = [&calls]() { calls.append(QStringLiteral("emit")); };

    const bool handled = TextEditorTabInteractionController::handleTextChanged(false, actions);
    const QStringList expected = {
        QStringLiteral("rebuild"),
        QStringLiteral("dirty"),
        QStringLiteral("emit")};
    if (!expect(handled,
                "handleTextChanged should return true when loading is inactive.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "handleTextChanged action order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runHandleModeRequestTest()
{
    QStringList calls;

    TextEditorTabInteractionController::ModeRequestActions actions;
    actions.setBlocksModeActive = [&calls](bool active) {
        calls.append(QStringLiteral("set:%1").arg(active ? QStringLiteral("true") : QStringLiteral("false")));
    };

    TextEditorTabInteractionController::handleModeRequest(false, actions);
    TextEditorTabInteractionController::handleModeRequest(true, actions);

    const QStringList expected = {
        QStringLiteral("set:false"),
        QStringLiteral("set:true")};
    if (!expect(calls == expected,
                "handleModeRequest should pass through requested block-mode value.")) {
        return 1;
    }

    return 0;
}

int runApplyModeSelectorVisibilityTest()
{
    QStringList calls;

    TextEditorTabInteractionController::ModeSelectorActions actions;
    actions.setModeRowVisible = [&calls](bool visible) {
        calls.append(QStringLiteral("visible:%1").arg(visible ? QStringLiteral("true") : QStringLiteral("false")));
    };
    actions.setModeRowMaximumHeight = [&calls](int height) {
        calls.append(QStringLiteral("max:%1").arg(height));
    };
    actions.setModeRowMinimumHeight = [&calls](int height) {
        calls.append(QStringLiteral("min:%1").arg(height));
    };
    actions.invalidateRootLayout = [&calls]() { calls.append(QStringLiteral("invalidate")); };
    actions.activateRootLayout = [&calls]() { calls.append(QStringLiteral("activate")); };

    TextEditorTabInteractionController::applyModeSelectorVisibility(true, 27, actions);

    const QStringList expectedVisible = {
        QStringLiteral("visible:true"),
        QStringLiteral("max:%1").arg(QWIDGETSIZE_MAX),
        QStringLiteral("min:27"),
        QStringLiteral("invalidate"),
        QStringLiteral("activate")};
    if (!expect(calls == expectedVisible,
                "applyModeSelectorVisibility visible=true flow changed unexpectedly.")) {
        return 1;
    }

    calls.clear();
    TextEditorTabInteractionController::applyModeSelectorVisibility(false, 27, actions);
    const QStringList expectedHidden = {
        QStringLiteral("visible:false"),
        QStringLiteral("max:0"),
        QStringLiteral("min:0"),
        QStringLiteral("invalidate"),
        QStringLiteral("activate")};
    if (!expect(calls == expectedHidden,
                "applyModeSelectorVisibility visible=false flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runHandleTextChangedSkipsWhenLoadingTest() != 0) {
        return 1;
    }
    if (runHandleTextChangedOrderTest() != 0) {
        return 1;
    }
    if (runHandleModeRequestTest() != 0) {
        return 1;
    }
    return runApplyModeSelectorVisibilityTest();
}
