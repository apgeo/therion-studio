#pragma once

#include <functional>

#include <QString>
#include <QTextDocument>

class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QWidget;

namespace TherionStudio
{
struct RawEditorFindContext
{
    QPlainTextEdit *editor = nullptr;
    QFrame *searchBar = nullptr;
    QWidget *replaceRow = nullptr;
    QLineEdit *findEdit = nullptr;
    QLineEdit *replaceEdit = nullptr;
    QLabel *searchStatusLabel = nullptr;
    QCheckBox *wholeWordCheck = nullptr;
    QCheckBox *caseSensitiveCheck = nullptr;
    QPushButton *replaceButton = nullptr;
    QPushButton *replaceAllButton = nullptr;
    bool *blocksModeActive = nullptr;
    std::function<void(bool)> setBlocksModeActive;
};

class RawEditorFindController final
{
public:
    explicit RawEditorFindController(RawEditorFindContext context);

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
    static QString trText(const char *sourceText);

    RawEditorFindContext context_;
};
}
