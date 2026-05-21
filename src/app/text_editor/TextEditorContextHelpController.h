#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TextEditorTab;

class TextEditorContextHelpController final
{
public:
    explicit TextEditorContextHelpController(TextEditorTab *owner);

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
};
}
