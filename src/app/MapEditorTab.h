#pragma once

#include <QColor>
#include <QWidget>
#include <QHash>
#include <QVector>

class QLabel;
class QComboBox;
class QFrame;
class QGraphicsScene;
class QGraphicsView;
class QSplitter;
class QPushButton;
class QGraphicsRectItem;
class QTextBrowser;
class QToolButton;
class QUndoStack;

namespace TherionStudio
{
struct TherionParsedLine;
enum class DraftGeometryKind;
}

namespace TherionStudio
{
class TextEditorTab;

class MapEditorTab final : public QWidget
{
    Q_OBJECT

public:
    enum class WorkspaceMode
    {
        TextOnly,
        MapOnly,
        Split
    };

    explicit MapEditorTab(QWidget *parent = nullptr);

    bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr);
    bool rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage = nullptr);
    void setProjectRootPath(const QString &projectRootPath);
    void showFindBar(bool replaceMode = false);
    void hideFindBar();
    void goToLine(int lineNumber);
    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    int currentLineNumber() const;
    QString text() const;
    WorkspaceMode workspaceMode() const;

public slots:
    void setWorkspaceMode(WorkspaceMode mode);

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void documentTextChanged();

private slots:
    void handleWorkspaceModeChanged(int index);
    void handleTextEditorCurrentLineChanged(int lineNumber);
    void handleMapSceneSelectionChanged();
    void handleAddPointTriggered();
    void handleAddLineTriggered();
    void handleAddAreaTriggered();
    void handleCompleteDraftTriggered();
    void handleUndoTriggered();
    void handleRedoTriggered();
    void handleZoomInTriggered();
    void handleZoomOutTriggered();
    void handleFitTriggered();
    void updateCommandSurfaceState();

private:
    void buildUi();
    void buildMapScene();
    void buildHelpPanel();
    void refreshMapScene();
    void clearMapScene();
    void clearDraftGeometryItems();
    void restoreDraftGeometryItems();
    void selectMapLine(int lineNumber);
    void fitMapToView();
    void adjustMapZoom(qreal factor);
    void recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition);
    void recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible);
    void recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition);
    void recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible);
    QGraphicsRectItem *selectedDraftGeometryItem() const;
    QGraphicsRectItem *createDraftGeometryItem(DraftGeometryKind kind);
    void addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position);
    void removeDraftGeometryItem(QGraphicsRectItem *item);
    void updateHelpPanel();
    void setHelpCollapsed(bool collapsed);
    void updateWorkspaceVisibility();
    void refreshTitle();
    void refreshStatus();
    QString displayPath() const;
    static WorkspaceMode workspaceModeFromIndex(int index);
    static int workspaceModeToIndex(WorkspaceMode mode);
    static WorkspaceMode workspaceModeFromSetting(const QString &value);
    static QString workspaceModeToSetting(WorkspaceMode mode);

    TextEditorTab *textEditor_ = nullptr;
    QGraphicsView *mapView_ = nullptr;
    QGraphicsScene *mapScene_ = nullptr;
    QSplitter *splitter_ = nullptr;
    QFrame *helpPanel_ = nullptr;
    QComboBox *workspaceModeCombo_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QTextBrowser *helpBrowser_ = nullptr;
    QToolButton *helpToggleButton_ = nullptr;
    QPushButton *detachButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
    QPushButton *redoButton_ = nullptr;
    QPushButton *pointButton_ = nullptr;
    QPushButton *lineButton_ = nullptr;
    QPushButton *areaButton_ = nullptr;
    QPushButton *completeDraftButton_ = nullptr;
    QPushButton *zoomOutButton_ = nullptr;
    QPushButton *zoomInButton_ = nullptr;
    QPushButton *fitButton_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QHash<int, QGraphicsRectItem *> mapItemsByLine_;
    QVector<QGraphicsRectItem *> draftGeometryItems_;
    QUndoStack *undoStack_ = nullptr;
    int nextDraftGeometryId_ = 1;
    WorkspaceMode workspaceMode_ = WorkspaceMode::Split;
    QString projectRootPath_;
    bool updatingSelection_ = false;
    qreal zoomFactor_ = 1.0;
    bool helpCollapsed_ = false;
    int helpPanelHeight_ = 180;
};
}