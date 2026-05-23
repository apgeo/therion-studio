#include "TextEditorTab.h"

#include "TextEditorContextHelpController.h"
#include "raw_editor/RawEditorCompletionController.h"

#include <QJsonObject>

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildContextHelpController()
{
    TextEditorContextHelpContext contextHelpContext;
    contextHelpContext.rootWidget = this;
    contextHelpContext.editor = &editor_;
    contextHelpContext.editorHelpSplitter = &editorHelpSplitter_;
    contextHelpContext.helpPanel = &helpPanel_;
    contextHelpContext.helpBrowser = &helpBrowser_;
    contextHelpContext.metadata = &commandMetadata_;
    contextHelpContext.lastValidationTooltipKey = &lastValidationTooltipKey_;
    contextHelpContext.helpPanelExtent = &helpPanelExtent_;
    contextHelpContext.helpCollapsed = &helpCollapsed_;
    contextHelpContext.translate = [this](const char *text) {
        return tr(text);
    };
    contextHelpContext.normalizedDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    contextHelpContext.openingDirectiveForClosingToken = [this](const QString &directive) {
        return openingDirectiveForClosingToken(directive);
    };
    contextHelpContext.currentCompletionCommand = [this]() {
        return currentCompletionCommand();
    };
    contextHelpContext.rebuildCompletionModel = [this]() {
        if (rawEditorCompletionController_ != nullptr) {
            rawEditorCompletionController_->rebuildCompletionModel();
        }
    };
    contextHelpContext.applyCatalogCommandsMetadata = [this](const QJsonObject &catalogObject) {
        if (rawEditorCompletionController_ != nullptr) {
            rawEditorCompletionController_->applyCatalogCommandsMetadata(catalogObject);
        }
    };
    contextHelpContext.populateBlockToolboxScopeCombo = [this]() {
        populateBlockToolboxScopeCombo();
    };

    contextHelpController_ =
        std::make_unique<TextEditorContextHelpController>(std::move(contextHelpContext));
}
}
