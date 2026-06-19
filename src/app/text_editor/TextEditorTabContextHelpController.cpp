#include "TextEditorTab.h"

#include "DocumentFileInspector.h"
#include "DocumentInspectorPanel.h"
#include "InspectorPanel.h"
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
    contextHelpContext.validateDocument = [this]() {
        TherionSourceValidationResult validation = validateDocument();
        validation.diagnostics += projectValidationDiagnostics_;
        return validation;
    };
    contextHelpContext.createInspectorPanel = [](QWidget *parent) {
        return new DocumentInspectorPanel(parent);
    };
    contextHelpContext.configureInspectorPanel = [this](InspectorPanel *inspectorPanel) {
        auto *documentInspectorPanel = qobject_cast<DocumentInspectorPanel *>(inspectorPanel);
        if (documentInspectorPanel == nullptr) {
            return;
        }
        DocumentFileInspectorContext fileContext;
        fileContext.filePath = [this]() {
            return filePath_;
        };
        fileContext.encodingName = [this]() {
            return fileEncodingName_;
        };
        fileContext.encodingLabel = [this]() {
            return fileEncodingLabel_;
        };
        fileContext.convertToUtf8 = [this]() {
            triggerConvertToUtf8();
        };
        rawFileInspector_ = documentInspectorPanel->addFileTab(std::move(fileContext));
    };

    contextHelpController_ =
        std::make_unique<TextEditorContextHelpController>(std::move(contextHelpContext), catalogStore_);
}

void TextEditorTab::buildHelpPanel()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->buildHelpPanel();
    }
}

void TextEditorTab::loadHelpMetadata()
{
    validationSourceSnapshotCache_.clear();
    if (contextHelpController_ != nullptr) {
        contextHelpController_->loadHelpMetadata();
    }
}

void TextEditorTab::loadHelpMetadataFromCommandCatalog()
{
    validationSourceSnapshotCache_.clear();
    if (contextHelpController_ != nullptr) {
        contextHelpController_->loadHelpMetadataFromCommandCatalog();
    }
}
}
