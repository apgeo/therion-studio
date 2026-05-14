#pragma once

#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <QWidget>
#include <QHash>
#include <QPointF>
#include <QVector>

class QLabel;
class QCheckBox;
class QFrame;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QSplitter;
class QSplitterHandle;
class QTextBrowser;

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
    void goToLineColumn(int lineNumber, int columnNumber);
    bool findNext();
    bool findPrevious();
    bool replaceCurrent();
    int replaceAll();
    bool rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage = nullptr);
    bool insertScrapBlock(const QString &preferredName = QString(),
                          int *insertedLineNumber = nullptr,
                          QString *errorMessage = nullptr);
    bool insertDraftGeometry(const QString &kind,
                             const QVector<QPointF> &vertices,
                             int *insertedLineNumber = nullptr,
                             QString *errorMessage = nullptr);
    bool rewritePointCoordinates(int lineNumber,
                                 const QPointF &point,
                                 QString *errorMessage = nullptr);
    bool rewriteLineAreaVertex(int lineNumber,
                               const QString &kind,
                               int vertexIndex,
                               const QPointF &point,
                               QString *errorMessage = nullptr);
    bool rewriteLineOptionToggle(int lineNumber,
                                 const QString &optionName,
                                 bool enabled,
                                 QString *errorMessage = nullptr);
    bool rewriteLineCoordinateRows(int lineNumber,
                                   const QStringList &coordinateRows,
                                   QString *errorMessage = nullptr);
    void replaceTextForCommand(const QString &contents);

    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    int currentLineNumber() const;
    int currentColumnNumber() const;
    QString text() const;
    QString statusPathText() const;
    QString statusEncodingText() const;
    void setInlineStatusVisible(bool visible);

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void cursorPositionChanged(int lineNumber, int columnNumber);
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
    void handleConvertToUtf8Triggered();
    void handleCursorPositionChanged();

private:
    void refreshCurrentLineHighlight();
    void refreshTitle();
    void refreshStatus();
    bool isCurrentStateDirty() const;
    void applyDirtyStateFromCurrentState();
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

    QLabel *encodingNoteLabel_ = nullptr;
    QPushButton *convertEncodingButton_ = nullptr;
    QWidget *statusRow_ = nullptr;
    QFrame *searchBar_ = nullptr;
    QSplitter *editorHelpSplitter_ = nullptr;
    QWidget *helpPanel_ = nullptr;
    QTextBrowser *helpBrowser_ = nullptr;
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
    int currentColumnNumber_ = 1;
    QHash<QString, TherionHelpEntry> helpEntries_;
    int helpPanelHeight_ = 200;
    bool helpCollapsed_ = false;
    bool dirty_ = false;
    bool loading_ = false;
    bool replaceMode_ = false;
    bool inlineStatusRequestedVisible_ = true;
    QString fileEncodingName_ = QStringLiteral("UTF-8");
    QString fileEncodingLabel_ = QStringLiteral("UTF-8");
    QString cleanEncodingNameSnapshot_ = QStringLiteral("UTF-8");
    QString cleanTextSnapshot_;
    QString encodingStatusNote_;
};
}
