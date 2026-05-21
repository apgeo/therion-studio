#pragma once

#include <QString>
#include <QTextDocument>

namespace TherionStudio
{
class TextEditorTab;

class RawEditorFindController final
{
public:
    explicit RawEditorFindController(TextEditorTab *owner);

    void showFindBar(bool replaceMode);
    void hideFindBar();
    bool findNext();
    bool findPrevious();
    bool replaceCurrent();
    int replaceAll();

    void handleFindTextEdited();
    void handleReplaceTextEdited();
    void handleSearchOptionsChanged();
    void handleFindNextTriggered();
    void handleFindPreviousTriggered();
    void handleReplaceTriggered();
    void handleReplaceAllTriggered();
    void handleCloseSearchTriggered();

private:
    void updateSearchResults(const QString &message, bool error = false) const;
    void updateSearchVisibility(bool replaceMode) const;
    bool performFind(bool forward, bool wrapSearch);
    QTextDocument::FindFlags findFlags() const;
    QString currentFindText() const;
    QString currentReplaceText() const;

    TextEditorTab *owner_ = nullptr;
};
}
