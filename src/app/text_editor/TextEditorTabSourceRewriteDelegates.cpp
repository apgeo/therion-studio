#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"

namespace TherionStudio
{
bool TextEditorTab::rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteStructureEntryName(lineNumber, category, newName, errorMessage);
}

bool TextEditorTab::configureCommandAtLine(const QString &kind, int lineNumber, bool showCommandHelpOnly)
{
    if (lineNumber <= 0 || editor_ == nullptr) {
        return false;
    }

    handleBlockConfigureRequest(kind, lineNumber, showCommandHelpOnly);
    return true;
}

bool TextEditorTab::deleteCommandAtLine(int lineNumber)
{
    return handleBlockDeleteRequest(lineNumber);
}

void TextEditorTab::replaceTextForCommand(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->replaceTextForCommand(contents);
    }
}

void TextEditorTab::replaceTextForSystemNormalization(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->replaceTextForSystemNormalization(contents);
    }
}
}
