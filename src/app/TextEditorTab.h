#pragma once

#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <QWidget>
#include <QHash>

class QLabel;
class QCheckBox;
class QFrame;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QSplitter;
class QTextBrowser;
class QToolButton;

namespace TherionStudio
{
class TherionSyntaxHighlighter;

struct TherionHelpEntry
{
    QString summary;
    QString syntax;
    QStringList arguments;
    QStringList acceptedValues;
    QStringList options;
    QStringList relatedKeywords;
};

class TextEditorTab final : public QWidget
{
    Q_OBJECT

public:
    explicit TextEditorTab(QWidget *parent = nullptr);

    bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr);
    void setProjectRootPath(const QString &projectRootPath);
    void showFindBar(bool replaceMode = false);
    void hideFindBar();
    void goToLine(int lineNumber);
    bool findNext();
    bool findPrevious();
    bool replaceCurrent();
    int replaceAll();
    bool rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage = nullptr);

    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    int currentLineNumber() const;
    QString text() const;

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void documentTextChanged();

private slots:
    void handleTextChanged();
    void handleFindTextEdited(const QString &text);
    void handleReplaceTextEdited(const QString &text);
    void handleSearchOptionsChanged(bool checked);
    void handleFindNextTriggered();
    void handleFindPreviousTriggered();
    void handleReplaceTriggered();
    void handleReplaceAllTriggered();
    void handleCloseSearchTriggered();
    void handleCursorPositionChanged();
    void handleHelpToggleTriggered(bool checked);

private:
    void refreshTitle();
    void refreshStatus();
    QString displayPath() const;
    void buildHelpPanel();
    void loadHelpMetadata();
    void updateContextHelp();
    QStringList helpCandidateTokens() const;
    QString currentHelpTokenForCursor() const;
    void setHelpCollapsed(bool collapsed);
    QString renderHelpHtml(const QString &token, const TherionHelpEntry &entry) const;
    void updateSearchResults(const QString &message, bool error = false);
    void updateSearchVisibility(bool replaceMode);
    bool performFind(bool forward, bool wrapSearch);
    QTextDocument::FindFlags findFlags() const;
    QString currentFindText() const;
    QString currentReplaceText() const;

    QLabel *pathLabel_ = nullptr;
    QLabel *encodingLabel_ = nullptr;
    QFrame *searchBar_ = nullptr;
    QSplitter *editorHelpSplitter_ = nullptr;
    QWidget *helpPanel_ = nullptr;
    QTextBrowser *helpBrowser_ = nullptr;
    QToolButton *helpToggleButton_ = nullptr;
    QWidget *replaceRow_ = nullptr;
    QLineEdit *findEdit_ = nullptr;
    QLineEdit *replaceEdit_ = nullptr;
    QLabel *searchStatusLabel_ = nullptr;
    QCheckBox *wholeWordCheck_ = nullptr;
    QCheckBox *caseSensitiveCheck_ = nullptr;
    QPushButton *findNextButton_ = nullptr;
    QPushButton *findPreviousButton_ = nullptr;
    QPushButton *replaceButton_ = nullptr;
    QPushButton *replaceAllButton_ = nullptr;
    QPushButton *closeSearchButton_ = nullptr;
    QPlainTextEdit *editor_ = nullptr;
    TherionSyntaxHighlighter *highlighter_ = nullptr;
    QString filePath_;
    QString projectRootPath_;
    int currentLineNumber_ = 0;
    QHash<QString, TherionHelpEntry> helpEntries_;
    int helpPanelHeight_ = 200;
    bool helpCollapsed_ = false;
    bool dirty_ = false;
    bool loading_ = false;
    bool replaceMode_ = false;
};
}
