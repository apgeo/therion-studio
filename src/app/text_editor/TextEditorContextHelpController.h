#pragma once

#include "../../core/CommandCatalogService.h"

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TextEditorTab;

class TextEditorContextHelpController final
{
public:
    explicit TextEditorContextHelpController(TextEditorTab *owner,
                                             CommandCatalogStore catalogStore = CommandCatalogStore());

    void buildHelpPanel();
    void loadHelpMetadata();
    void loadHelpMetadataFromCommandCatalog();
    void setHelpCollapsed(bool collapsed);
    void updateContextHelp();
    void updateValidationTooltipForCursor();

    QStringList helpCandidateTokens() const;
    QString currentHelpTokenForCursor() const;
    QString validationHelpHtmlForCursor(QString *tooltipText = nullptr,
                                        QString *tooltipKey = nullptr) const;

private:
    TextEditorTab *owner_ = nullptr;
    CommandCatalogStore catalogStore_;
};
}
