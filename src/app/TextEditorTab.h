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
class QComboBox;
class QPushButton;
class QPlainTextEdit;
class QGraphicsScene;
class QGraphicsView;
class QGraphicsLineItem;
class QStackedWidget;
class QSplitter;
class QSplitterHandle;
class QTextBrowser;
class QCompleter;
class QStringListModel;
class QEvent;
class QTableWidget;
class QFormLayout;

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
    ~TextEditorTab() override;

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

protected:
    void changeEvent(QEvent *event) override;

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
    enum class BlockDetailsMode
    {
        None,
        StructuredOptions,
        SimpleValue,
        DataHeader,
        Unsupported,
    };

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
    void handleApplicationAppearanceChanged();
    QString renderHelpHtml(const QString &token, const TherionHelpEntry &entry, bool includeSyntax = true) const;
    QString renderHelpSummaryHtml(const QString &token, const TherionHelpEntry &entry) const;
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
    bool ensureEncodingRootDirectiveForBlocks();
    void populateBlockToolbox();
    void populateBlockToolboxScopeCombo();
    void updateBlockToolboxScopeLabel();
    QString selectedBlockInsertionContextToken() const;
    void rebuildBlocksCanvasFromText();
    void handleCanvasDrop(const QString &kind, const QPointF &scenePos);
    void handleBlockMoveRequest(int lineNumber, const QPointF &scenePos);
    void updateBlockMovePreview(int sourceLineNumber, const QPointF &scenePos);
    void clearBlockMovePreview();
    int resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const;
    void handleBlockConfigureRequest(const QString &kind, int lineNumber);
    void selectBlockInCanvasAndDetails(int lineNumber);
    void clearBlockDetailsPane();
    void showBlockDetailsForToolboxCommand(const QString &commandToken);
    void refreshBlockDetailsSelectionFromScene();
    bool loadBlockDetailsForSelection(const QString &kind, int lineNumber);
    void refreshBlockDetailsOptionArgumentEditors();
    void updateBlockDetailsHelpForCurrentFocus();
    void refreshBlockDetailsApplyState();
    bool buildUpdatedLineFromBlockDetails(QString *updatedLine, QString *validationError = nullptr) const;
    void applyBlockDetailsChanges();
    bool supportsDetailsPaneForKind(const QString &kind) const;
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
    bool isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const;
    QStringList commandArgumentSignaturesFor(const QString &commandToken) const;
    bool commandHasRequiredIdArgument(const QString &commandToken) const;
    bool commandSupportsInlineIdField(const QString &commandToken) const;
    QString primaryInsertionScopeForCommand(const QString &commandToken) const;
    QString resolveScopeForCommandAtLine(const QString &commandToken,
                                         const QStringList &lines,
                                         int lineNumber) const;
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
    QComboBox *blockToolboxScopeCombo_ = nullptr;
    QListWidget *blockToolboxList_ = nullptr;
    QGraphicsView *blockCanvasView_ = nullptr;
    QGraphicsScene *blockCanvasScene_ = nullptr;
    QGraphicsLineItem *blockMovePreviewLine_ = nullptr;
    QList<QGraphicsLineItem *> blockContainerBoundaryGuideItems_;
    QHash<int, qreal> blockContainerBoundaryEndYByLine_;
    QFrame *blockDetailsPanel_ = nullptr;
    QWidget *blockDetailsEditPanel_ = nullptr;
    QWidget *blockDetailsHelpPanel_ = nullptr;
    QLabel *blockDetailsStatusLabel_ = nullptr;
    QLabel *blockDetailsPrimaryFieldLabel_ = nullptr;
    QLabel *blockDetailsSecondaryFieldLabel_ = nullptr;
    QLabel *blockDetailsCommentFieldLabel_ = nullptr;
    QLabel *blockDetailsOptionsLabel_ = nullptr;
    QLabel *blockDetailsOptionArgsLabel_ = nullptr;
    QLineEdit *blockDetailsIdEdit_ = nullptr;
    QStackedWidget *blockDetailsSecondaryFieldStack_ = nullptr;
    QLineEdit *blockDetailsAdditionalPositionalEdit_ = nullptr;
    QWidget *blockDetailsReadingsTagEditor_ = nullptr;
    QLineEdit *blockDetailsCommentEdit_ = nullptr;
    QTableWidget *blockDetailsOptionsTable_ = nullptr;
    QWidget *blockDetailsOptionArgsPanel_ = nullptr;
    QFormLayout *blockDetailsOptionArgsFormLayout_ = nullptr;
    QList<QLineEdit *> blockDetailsOptionArgEditors_;
    QTextBrowser *blockDetailsHelpBrowser_ = nullptr;
    QPushButton *blockDetailsAddOptionButton_ = nullptr;
    QPushButton *blockDetailsRemoveOptionButton_ = nullptr;
    QPushButton *blockDetailsApplyButton_ = nullptr;
    QPushButton *blockDetailsLegacyConfigureButton_ = nullptr;
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
    QHash<QString, QStringList> commandOptionArgumentLabelsByKey_;
    QHash<QString, int> commandOptionFixedArityByKey_;
    QHash<QString, QString> commandOptionHelpHtmlByKey_;
    QHash<QString, QStringList> commandTypeValueTokens_;
    QHash<QString, QHash<QString, QStringList>> commandSubtypeByTypeTokens_;
    QHash<QString, int> commandRequiredPositionalCount_;
    QHash<QString, QStringList> commandArgumentSignaturesByToken_;
    QHash<QString, bool> commandPrimaryValueIsPerson_;
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
    bool enforcingEncodingRootDirective_ = false;
    bool tearingDown_ = false;
    bool blockDetailsPopulating_ = false;
    bool blockDetailsOptionArgsSyncing_ = false;
    BlockDetailsMode blockDetailsMode_ = BlockDetailsMode::None;
    int blockDetailsSelectedLineNumber_ = 0;
    QString blockDetailsSelectedKind_;
    QString blockDetailsBaseStatusText_;
    QChar blockDetailsCommentMarker_ = QLatin1Char('#');
    QString fileEncodingName_ = QStringLiteral("UTF-8");
    QString fileEncodingLabel_ = QStringLiteral("UTF-8");
    QString cleanEncodingNameSnapshot_ = QStringLiteral("UTF-8");
    QString cleanTextSnapshot_;
    QString encodingStatusNote_;
};
}
