#pragma once

#include "../../core/CommandCatalogStore.h"

#include <functional>

#include <QJsonObject>
#include <QString>
#include <QStringList>

class QPoint;
class QPlainTextEdit;
class QSplitter;
class QTextCursor;
class QTextBrowser;
class QWidget;

namespace TherionStudio
{
class InspectorPanel;
class ContextHelpInspector;
struct TextEditorCommandMetadata;

struct TextEditorContextHelpContext
{
    QWidget *rootWidget = nullptr;
    QPlainTextEdit **editor = nullptr;
    QSplitter **editorHelpSplitter = nullptr;
    QWidget **helpPanel = nullptr;
    QTextBrowser **helpBrowser = nullptr;
    TextEditorCommandMetadata *metadata = nullptr;
    QString *lastValidationTooltipKey = nullptr;
    int *helpPanelExtent = nullptr;
    bool *helpCollapsed = nullptr;
    std::function<QString(const char *)> translate;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<QString(const QString &)> openingDirectiveForClosingToken;
    std::function<QString()> currentCompletionCommand;
    std::function<void()> rebuildCompletionModel;
    std::function<void(const QJsonObject &)> applyCatalogCommandsMetadata;
    std::function<void()> populateBlockToolboxScopeCombo;
    std::function<InspectorPanel *(QWidget *)> createInspectorPanel;
    std::function<void(InspectorPanel *)> configureInspectorPanel;
};

class TextEditorContextHelpController final
{
public:
    explicit TextEditorContextHelpController(TextEditorContextHelpContext context,
                                             CommandCatalogStore catalogStore);

    void buildHelpPanel();
    void loadHelpMetadata();
    void loadHelpMetadataFromCommandCatalog();
    void setHelpCollapsed(bool collapsed);
    void updateContextHelp();
    void updateValidationTooltipForCursor();
    bool showValidationTooltipForPosition(const QPoint &viewportPosition, const QPoint &globalPosition);

    QStringList helpCandidateTokens() const;
    QString currentHelpTokenForCursor() const;
    QString validationHelpHtmlForCursor(QString *tooltipText = nullptr,
                                        QString *tooltipKey = nullptr) const;

private:
    QString tr(const char *text) const;
    QPlainTextEdit *editor() const;
    QSplitter *editorHelpSplitter() const;
    QWidget *helpPanel() const;
    QTextBrowser *helpBrowser() const;
    QString validationHelpHtmlForTextCursor(const QTextCursor &cursor,
                                            QString *tooltipText = nullptr,
                                            QString *tooltipKey = nullptr) const;
    void setHelpPanel(QWidget *helpPanel);
    void setHelpBrowser(QTextBrowser *helpBrowser);
    void setHelpTitle(const QString &title);

    TextEditorContextHelpContext context_;
    CommandCatalogStore catalogStore_;
    ContextHelpInspector *helpInspector_ = nullptr;
};
}
