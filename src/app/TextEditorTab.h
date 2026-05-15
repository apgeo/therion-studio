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
class QListWidget;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QGraphicsScene;
class QGraphicsView;
class QStackedWidget;
class QSplitter;
class QSplitterHandle;
class QTextBrowser;
class QCompleter;
class QStringListModel;
class QEvent;

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
    bool insertDraftLineGeometry(const QStringList &coordinateRows,
                                 int *insertedLineNumber = nullptr,
                                 QString *errorMessage = nullptr);
    bool insertDraftAreaGeometry(const QStringList &coordinateRows,
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
    void handleRawModeRequested();
    void handleBlocksModeRequested();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void refreshCurrentLineHighlight();
    void refreshTitle();
    void refreshStatus();
    bool isCurrentStateDirty() const;
    void applyDirtyStateFromCurrentState();
    QString displayPath() const;
    void buildHelpPanel();
    void loadHelpMetadata();
    void loadHelpMetadataFromCommandCatalog();
    void mergeHelpEntry(const QString &token, const TherionHelpEntry &entry);
    void updateContextHelp();
    void updateValidationTooltipForCursor();
    QStringList helpCandidateTokens() const;
    QString currentHelpTokenForCursor() const;
    QString validationHelpHtmlForCursor(QString *tooltipText = nullptr,
                                        QString *tooltipKey = nullptr) const;
    void setHelpCollapsed(bool collapsed);
    QString renderHelpHtml(const QString &token, const TherionHelpEntry &entry) const;
    void updateSearchResults(const QString &message, bool error = false);
    void updateSearchVisibility(bool replaceMode);
    bool performFind(bool forward, bool wrapSearch);
    QTextDocument::FindFlags findFlags() const;
    QString currentFindText() const;
    QString currentReplaceText() const;
    void refreshEditorModeUi();
    bool isBlocksModeSupportedForCurrentFile() const;
    void refreshBlocksModeAvailability();
    void setBlocksModeActive(bool active);
    void populateBlockToolbox();
    void rebuildBlocksCanvasFromText();
    void handleCanvasDrop(const QString &kind, const QPointF &scenePos);
    void handleBlockMoveRequest(int lineNumber, const QPointF &scenePos);
    void handleBlockConfigureRequest(const QString &kind, int lineNumber);
    void handleBlockDeleteRequest(int lineNumber);
    bool insertLinesBefore(int lineNumber,
                           const QStringList &newLines,
                           QString *errorMessage = nullptr);
    void registerCompletionToken(const QString &token);
    void rebuildCompletionModel();
    QString currentCompletionPrefix() const;
    void triggerCompletionPopup();
    void insertCompletionToken(const QString &completion);
    QStringList buildCompletionSuggestionsForCursor(const QString &prefix) const;
    QStringList projectInputFileCompletionCandidates() const;
    bool isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const;
    QString currentCompletionCommand() const;
    QStringList activeCompletionScopeStack() const;
    QString normalizeCompletionContext(const QString &contextToken) const;
    QString currentCompletionScopeLabel() const;

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
    QWidget *modeRow_ = nullptr;
    QPushButton *rawModeButton_ = nullptr;
    QPushButton *blocksModeButton_ = nullptr;
    QStackedWidget *editorModeStack_ = nullptr;
    QWidget *blocksPanel_ = nullptr;
    QLineEdit *blockToolboxFilterEdit_ = nullptr;
    QListWidget *blockToolboxList_ = nullptr;
    QGraphicsView *blockCanvasView_ = nullptr;
    QGraphicsScene *blockCanvasScene_ = nullptr;
    QPlainTextEdit *editor_ = nullptr;
    TherionSyntaxHighlighter *highlighter_ = nullptr;
    QString filePath_;
    QString projectRootPath_;
    int currentLineNumber_ = 0;
    int currentColumnNumber_ = 1;
    int highlightedLineNumber_ = 0;
    QHash<QString, TherionHelpEntry> helpEntries_;
    QStringList completionTokens_;
    QStringList commandCompletionTokens_;
    QHash<QString, QStringList> commandOptionTokens_;
    QHash<QString, QStringList> commandValueTokens_;
    QHash<QString, QStringList> commandArgumentValueTokens_;
    QHash<QString, QStringList> commandOptionValueTokens_;
    QHash<QString, QString> commandOptionValueArityTokens_;
    QHash<QString, QStringList> commandTypeValueTokens_;
    QHash<QString, QHash<QString, QStringList>> commandSubtypeByTypeTokens_;
    QHash<QString, int> commandRequiredPositionalCount_;
    QHash<QString, QStringList> contextCommandTokens_;
    QHash<QString, QStringList> blockCommandContextsByKind_;
    QCompleter *completionCompleter_ = nullptr;
    QStringListModel *completionModel_ = nullptr;
    QString lastValidationTooltipKey_;
    int helpPanelHeight_ = 200;
    bool helpCollapsed_ = false;
    bool dirty_ = false;
    bool loading_ = false;
    bool replaceMode_ = false;
    bool inlineStatusRequestedVisible_ = true;
    bool blocksModeActive_ = false;
    QString fileEncodingName_ = QStringLiteral("UTF-8");
    QString fileEncodingLabel_ = QStringLiteral("UTF-8");
    QString cleanEncodingNameSnapshot_ = QStringLiteral("UTF-8");
    QString cleanTextSnapshot_;
    QString encodingStatusNote_;
};
}
