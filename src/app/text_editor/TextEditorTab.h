#pragma once

#include "TextEditorCommandMetadata.h"

#include <QString>
#include <QStringList>
#include <QWidget>
#include <QHash>
#include <QPointF>
#include <QVector>
#include <QColor>
#include <memory>

class QLabel;
class QCheckBox;
class QFrame;
class QListWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QPlainTextEdit;
class QGraphicsItem;
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
class TextEditorAppearanceController;
class TextEditorContextHelpController;
class TextEditorCursorController;
class TextEditorDocumentController;
class TextEditorEncodingController;
class TextEditorModeController;
class TextEditorSourceRewriteController;
class TextEditorStatusController;
class RawEditorFindController;
class RawEditorCompletionContextAnalyzer;
class RawEditorCompletionSuggestionBuilder;
class RawEditorCompletionController;
class BlockEditorOptionArgsController;
class BlockEditorDetailsHelpController;
class BlockEditorLineBuildService;
class BlockEditorApplyStateController;
class BlockEditorApplyExecutor;
class BlockEditorSelectionDetailsController;
class BlockEditorDetailsPaneController;
class BlockEditorToolboxController;
class BlockEditorToolboxDetailsController;
class BlockEditorCanvasSelectionController;
class BlockEditorMovePreviewController;
class BlockEditorCanvasBoundaryController;
class BlockEditorCanvasRebuildController;
class BlockEditorDocumentOutlineBuilder;
class BlockEditorDropTargetResolver;
class BlockEditorInsertionController;
class BlockEditorInsertionPlanner;
class BlockEditorMoveController;
class BlockEditorMovePlanner;
class BlockEditorSourceController;
struct BlockEditorCommandOptionsDialogContext;
struct BlockEditorConfigureContext;
struct BlockEditorDataBlockDialogContext;
struct BlockEditorDetailsHelpContext;
struct BlockEditorDeleteContext;
struct BlockEditorDocumentOutlineContext;
struct BlockEditorDropTargetContext;
struct BlockEditorInsertionContext;
struct BlockEditorInsertionPlannerContext;
struct BlockEditorMoveContext;
struct BlockEditorLineBuildContext;
struct BlockEditorMovePlannerContext;
struct BlockEditorMovePreviewContext;
struct BlockEditorOptionArgsContext;
struct BlockEditorSelectionDetailsContext;
struct BlockEditorApplyExecutorContext;
struct BlockEditorApplyStateContext;
struct BlockEditorCanvasBoundaryContext;
struct BlockEditorCanvasRebuildContext;
struct BlockEditorCanvasSelectionContext;
struct BlockEditorDetailsPaneContext;
struct BlockEditorSourceContext;
struct BlockEditorSingleValueCommandDialogContext;
struct BlockEditorToolboxContext;
struct BlockEditorToolboxDetailsContext;
struct TherionParsedLine;

class TextEditorTab final : public QWidget
{
    Q_OBJECT

public:
    enum class EditorMode
    {
        Raw,
        Blocks
    };

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
                          QString *errorMessage = nullptr,
                          const QString &options = QString());
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
    bool rewritePointOrientation(int lineNumber,
                                 bool enabled,
                                 qreal orientationDegrees,
                                 QString *errorMessage = nullptr);
    bool rewriteLinePointOrientation(int lineNumber,
                                     int sourceVertexIndex,
                                     bool enabled,
                                     qreal orientationDegrees,
                                     QString *errorMessage = nullptr);
    bool rewriteLinePointLeftSize(int lineNumber,
                                  int sourceVertexIndex,
                                  bool enabled,
                                  qreal sizeValue,
                                  QString *errorMessage = nullptr);
    bool rewriteLineCoordinateRows(int lineNumber,
                                   const QStringList &coordinateRows,
                                   QString *errorMessage = nullptr);
    bool configureCommandAtLine(const QString &kind, int lineNumber, bool showCommandHelpOnly = false);
    bool deleteCommandAtLine(int lineNumber);
    void replaceTextForCommand(const QString &contents);

    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    int currentLineNumber() const;
    int currentColumnNumber() const;
    int documentRevision() const;
    QString text() const;
    QColor sourceSurfaceColor() const;
    QString statusPathText() const;
    QString statusEncodingText() const;
    bool canUndo() const;
    bool canRedo() const;
    void triggerUndo();
    void triggerRedo();
    void setInlineStatusVisible(bool visible);
    void setModeSelectorVisible(bool visible);
    EditorMode editorMode() const;
    bool isBlocksModeAvailable() const;

public slots:
    void setEditorMode(EditorMode mode);

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void cursorPositionChanged(int lineNumber, int columnNumber);
    void documentTextChanged();
    void editorModeChanged(TherionStudio::TextEditorTab::EditorMode mode);

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
    const TextEditorCommandMetadata &commandMetadata() const { return commandMetadata_; }
    TextEditorCommandMetadata &mutableCommandMetadata() { return commandMetadata_; }
    QString displayPath() const;
    void buildHelpPanel();
    void buildContextHelpController();
    void buildRawEditorPanel();
    void buildAppearanceController();
    void buildCursorController();
    void buildDocumentController();
    void buildEncodingController();
    void buildModeController();
    void buildSourceRewriteController();
    void buildStatusController();
    void buildBlockEditorPanel();
    void buildBlockToolboxController();
    BlockEditorToolboxContext blockEditorToolboxContext();
    void buildBlockDetailsHelpController();
    BlockEditorDetailsHelpContext blockEditorDetailsHelpContext();
    void buildBlockDetailsLineBuildService();
    BlockEditorLineBuildContext blockEditorLineBuildContext();
    BlockEditorDocumentOutlineContext blockEditorDocumentOutlineContext() const;
    BlockEditorDropTargetContext blockEditorDropTargetContext() const;
    BlockEditorInsertionPlannerContext blockEditorInsertionPlannerContext() const;
    BlockEditorMovePlannerContext blockEditorMovePlannerContext() const;
    BlockEditorInsertionContext blockEditorInsertionContext();
    BlockEditorMoveContext blockEditorMoveContext();
    void buildBlockDetailsMovePreviewController();
    BlockEditorMovePreviewContext blockEditorMovePreviewContext();
    void buildBlockDetailsOptionArgsController();
    BlockEditorOptionArgsContext blockEditorOptionArgsContext();
    void buildBlockDetailsApplyExecutor();
    BlockEditorApplyExecutorContext blockEditorApplyExecutorContext();
    void buildBlockDetailsApplyStateController();
    BlockEditorApplyStateContext blockEditorApplyStateContext();
    BlockEditorCanvasBoundaryContext blockEditorCanvasBoundaryContext() const;
    BlockEditorCanvasRebuildContext blockEditorCanvasRebuildContext();
    void buildBlockDetailsSelectionDetailsController();
    BlockEditorSelectionDetailsContext blockEditorSelectionDetailsContext();
    void buildBlockDetailsPaneController();
    BlockEditorDetailsPaneContext blockEditorDetailsPaneContext();
    void buildBlockDetailsToolboxDetailsController();
    BlockEditorToolboxDetailsContext blockEditorToolboxDetailsContext();
    void buildBlockDetailsCanvasSelectionController();
    BlockEditorCanvasSelectionContext blockEditorCanvasSelectionContext();
    BlockEditorSourceContext blockEditorSourceContext();
    BlockEditorSourceContext blockEditorSourceContext() const;
    BlockEditorDeleteContext blockEditorDeleteContext();
    BlockEditorConfigureContext blockEditorConfigureContext();
    BlockEditorCommandOptionsDialogContext blockEditorCommandOptionsDialogContext();
    BlockEditorDataBlockDialogContext blockEditorDataBlockDialogContext();
    BlockEditorSingleValueCommandDialogContext blockEditorSingleValueCommandDialogContext();
    void loadHelpMetadata();
    void loadHelpMetadataFromCommandCatalog();
    void updateContextHelp();
    void updateValidationTooltipForCursor();
    QString normalizedDirectiveToken(const QString &directive) const;
    QString openingDirectiveForClosingToken(const QString &directive) const;
    QString closingDirectiveForOpeningToken(const QString &directive) const;
    bool isContainerDirectiveInstanceForParsedLine(const QString &directive,
                                                   const TherionParsedLine &parsedLine) const;
    void setHelpCollapsed(bool collapsed);
    void handleApplicationAppearanceChanged();
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
    void handleBlockConfigureRequest(const QString &kind, int lineNumber, bool showCommandHelpOnly = false);
    void selectBlockInCanvasAndDetails(int lineNumber);
    void clearBlockDetailsPane();
    void showBlockDetailsForToolboxCommand(const QString &commandToken);
    void refreshBlockDetailsSelectionFromScene();
    bool loadBlockDetailsForSelection(const QString &kind, int lineNumber);
    void refreshBlockDetailsOptionArgumentEditors();
    void updateBlockDetailsHelpForCurrentFocus();
    QGraphicsItem *resolveBlockCanvasItem(QGraphicsItem *item) const;
    int blockCanvasItemLineNumber(const QGraphicsItem *item) const;
    QString blockCanvasItemKind(const QGraphicsItem *item) const;
    void selectBlockCanvasItem(QGraphicsItem *item, bool centerView);
    bool blockDetailsReadingsTagEditorHasEditorFocus() const;
    QString blockDetailsReadingsOrderTextForBuild() const;
    void setBlockDetailsReadingsTagEditor(const QString &placeholderText,
                                          const QStringList &suggestions,
                                          const QStringList &tokens);
    void installBlockDetailsLineEditCompleter(QLineEdit *lineEdit,
                                              const QStringList &values) const;
    bool isContainerBlockDirectiveForBlocks(const QString &directive) const;
    bool isRequiredArgumentSignatureForBlocks(const QString &signature) const;
    void refreshBlockDetailsApplyState();
    bool buildUpdatedLineFromBlockDetails(QString *updatedLine, QString *validationError = nullptr) const;
    void applyBlockDetailsChanges();
    bool supportsDetailsPaneForKind(const QString &kind) const;
    bool handleBlockDeleteRequest(int lineNumber);
    void insertCompletionToken(const QString &completion);
    bool isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const;
    bool isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const;
    QStringList commandArgumentSignaturesFor(const QString &commandToken) const;
    bool commandHasRequiredIdArgument(const QString &commandToken) const;
    bool commandSupportsInlineIdField(const QString &commandToken) const;
    QString resolveScopeForCommandAtLine(const QString &commandToken,
                                         const QStringList &lines,
                                         int lineNumber) const;
    QString currentCompletionCommand() const;
    QString normalizeCompletionContext(const QString &contextToken) const;

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
    QWidget *rawEditorPanel_ = nullptr;
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
    TextEditorCommandMetadata commandMetadata_;
    QCompleter *completionCompleter_ = nullptr;
    QStringListModel *completionModel_ = nullptr;
    QString lastValidationTooltipKey_;
    int helpPanelExtent_ = 380;
    bool helpCollapsed_ = false;
    bool dirty_ = false;
    bool loading_ = false;
    bool inlineStatusRequestedVisible_ = true;
    bool modeSelectorRequestedVisible_ = true;
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
    std::unique_ptr<TextEditorAppearanceController> appearanceController_;
    std::unique_ptr<TextEditorContextHelpController> contextHelpController_;
    std::unique_ptr<TextEditorCursorController> cursorController_;
    std::unique_ptr<TextEditorDocumentController> documentController_;
    std::unique_ptr<TextEditorEncodingController> encodingController_;
    std::unique_ptr<TextEditorModeController> editorModeController_;
    std::unique_ptr<TextEditorSourceRewriteController> sourceRewriteController_;
    std::unique_ptr<TextEditorStatusController> statusController_;
    std::unique_ptr<RawEditorFindController> rawEditorFindController_;
    std::unique_ptr<RawEditorCompletionController> rawEditorCompletionController_;
    std::unique_ptr<BlockEditorOptionArgsController> blockDetailsOptionArgsController_;
    std::unique_ptr<BlockEditorDetailsHelpController> blockDetailsHelpController_;
    std::unique_ptr<BlockEditorLineBuildService> blockDetailsLineBuildService_;
    std::unique_ptr<BlockEditorApplyStateController> blockDetailsApplyStateController_;
    std::unique_ptr<BlockEditorApplyExecutor> blockDetailsApplyExecutor_;
    std::unique_ptr<BlockEditorSelectionDetailsController> blockDetailsSelectionDetailsController_;
    std::unique_ptr<BlockEditorDetailsPaneController> blockDetailsPaneController_;
    std::unique_ptr<BlockEditorToolboxController> blockToolboxController_;
    std::unique_ptr<BlockEditorToolboxDetailsController> blockDetailsToolboxDetailsController_;
    std::unique_ptr<BlockEditorCanvasSelectionController> blockDetailsCanvasSelectionController_;
    std::unique_ptr<BlockEditorMovePreviewController> blockDetailsMovePreviewController_;
    std::unique_ptr<BlockEditorCanvasBoundaryController> blockDetailsCanvasBoundaryController_;
    std::unique_ptr<BlockEditorCanvasRebuildController> blockDetailsCanvasRebuildController_;
};
}
