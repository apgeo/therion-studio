#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QDateTime>
#include <QVector>
#include <QPointer>

class QLabel;
class QFrame;
class QGraphicsScene;
class QGraphicsView;
class QSplitter;
class QSplitterHandle;
class QPushButton;
class QGraphicsRectItem;
class QGraphicsPixmapItem;
class QTextBrowser;
class QToolButton;
class QUndoStack;
class QMainWindow;

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
    bool rewriteLineOptionToggle(int lineNumber,
                                 const QString &optionName,
                                 bool enabled,
                                 QString *errorMessage = nullptr);
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
    int backgroundLayerCount() const;
    QString backgroundLayerLabel(int index) const;
    bool isBackgroundLayerVisible(int index) const;
    qreal backgroundLayerOpacity(int index) const;
    qreal backgroundLayerGamma(int index) const;
    QPointF backgroundLayerPosition(int index) const;
    int selectedBackgroundLayerIndex() const;
    void setSelectedBackgroundLayerIndex(int index);
    void browseAndAddBackgroundImages();
    void removeSelectedBackgroundLayer();
    void moveSelectedBackgroundLayerUp();
    void moveSelectedBackgroundLayerDown();
    void toggleSelectedBackgroundLayerVisibility();
    void setSelectedBackgroundLayerOpacity(qreal opacity);
    void resetSelectedBackgroundLayerOpacity();
    void setSelectedBackgroundLayerGamma(qreal gamma);
    void resetSelectedBackgroundLayerGamma();
    void setSelectedBackgroundLayerPosition(const QPointF &position);
    void nudgeSelectedBackgroundLayer(const QPointF &delta);

public slots:
    void setWorkspaceMode(WorkspaceMode mode);

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void documentTextChanged();
    void backgroundLayersChanged();
    void openDedicatedWindowRequested(TherionStudio::MapEditorTab *tab);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void handleWorkspaceModeChanged(int index);
    void handleTextEditorCurrentLineChanged(int lineNumber);
    void handleMapSceneSelectionChanged();
    void handleAddPointTriggered();
    void handleAddLineTriggered();
    void handleAddFreehandLineTriggered();
    void handleAddSmartTraceLineTriggered();
    void handleAddAreaTriggered();
    void handleSelectModeTriggered();
    void handleInsertScrapTriggered();
    void handleCompleteDraftTriggered();
    void handleUndoTriggered();
    void handleRedoTriggered();
    void handleZoomInTriggered();
    void handleZoomOutTriggered();
    void handleFitTriggered();
    void handleFitWithBackgroundTriggered();
    void handleTouchFriendlyControlsToggled(bool checked);
    void updateCommandSurfaceState();
    void handleDetachPaneTriggered();

private:
    void buildUi();
    void buildMapScene();
    void buildHelpPanel();
    void refreshMapScene();
    void clearMapScene();
    void clearDraftGeometryItems();
    void clearBackgroundImageItems();
    void restoreDraftGeometryItems();
    void restoreBackgroundImageItems();
    void selectMapLine(int lineNumber);
    void fitMapToView(bool includeBackgroundImages = false);
    void syncZoomFactorFromView();
    void applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition);
    void refreshToolbarSummary();
    void adjustMapZoom(qreal factor);
    QRectF mapGeometryFitBounds() const;
    QRectF mapBackgroundFitBounds() const;
    QRectF mapPreviewBounds() const;
    void addBackgroundImage(const QString &imagePath);
    void refreshBackgroundLayerControls();
    void applyBackgroundLayerStackingOrder();
    void saveBackgroundLayersToSession() const;
    void loadBackgroundLayersFromSession();
    void loadBackgroundLayersFromDocumentMetadata();
    void syncAutoBackgroundLayersFromCurrentDocument();
    QGraphicsPixmapItem *backgroundLayerItemAt(int index) const;
    QGraphicsPixmapItem *selectedBackgroundLayerItem() const;
    qreal backgroundLayerGammaValue(const QGraphicsPixmapItem *item) const;
    void applyBackgroundLayerGamma(QGraphicsPixmapItem *item, qreal gamma);
    void setSelectedBackgroundLayerIndexInternal(int index);
    QVector<QPointF> sourceVerticesForDraft(const QGraphicsRectItem *item) const;
    QPointF previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const;
    QString canonicalDocumentSessionKey() const;
    void recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition);
    void recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible);
    void recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint);
    void recordLineAreaVertexMove(int lineNumber,
                                  const QString &kind,
                                  int vertexIndex,
                                  const QPointF &oldPoint,
                                  const QPointF &newPoint);
    bool insertLineVertexFromSelection();
    bool removeLineVertexFromSelection();
    void recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition);
    void recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible);
    QGraphicsRectItem *selectedDraftGeometryItem() const;
    QGraphicsRectItem *createDraftGeometryItem(DraftGeometryKind kind);
    void addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position);
    void removeDraftGeometryItem(QGraphicsRectItem *item);
    void updateHelpPanel();
    void installHelpBorderToggle();
    void refreshHelpBorderToggle();
    void setHelpCollapsed(bool collapsed);
    void updateWorkspaceVisibility();
    void detachMapPaneToWindow();
    void reattachMapPaneFromWindow();
    void focusDetachedMapPaneWindow();
    void refreshDetachButtonState();
    void refreshTitle();
    void refreshStatus();
    QString displayPath() const;
    static WorkspaceMode workspaceModeFromIndex(int index);
    static int workspaceModeToIndex(WorkspaceMode mode);
    static WorkspaceMode workspaceModeFromSetting(const QString &value);
    static QString workspaceModeToSetting(WorkspaceMode mode);
    void setTouchFriendlyControlsEnabled(bool enabled);

    TextEditorTab *textEditor_ = nullptr;
    QGraphicsView *mapView_ = nullptr;
    QGraphicsScene *mapScene_ = nullptr;
    QWidget *mapPaneContainer_ = nullptr;
    QSplitter *splitter_ = nullptr;
    QSplitter *mapHelpSplitter_ = nullptr;
    QFrame *helpPanel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QWidget *statusRow_ = nullptr;
    QLabel *statusPathLabel_ = nullptr;
    QLabel *statusEncodingLabel_ = nullptr;
    QTextBrowser *helpBrowser_ = nullptr;
    QToolButton *helpBorderToggleButton_ = nullptr;
    QPushButton *detachButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
    QPushButton *redoButton_ = nullptr;
    QPushButton *pointButton_ = nullptr;
    QPushButton *lineButton_ = nullptr;
    QPushButton *freehandLineButton_ = nullptr;
    QPushButton *smartTraceLineButton_ = nullptr;
    QPushButton *areaButton_ = nullptr;
    QPushButton *selectButton_ = nullptr;
    QPushButton *insertScrapButton_ = nullptr;
    QPushButton *completeDraftButton_ = nullptr;
    QPushButton *zoomOutButton_ = nullptr;
    QPushButton *zoomInButton_ = nullptr;
    QPushButton *fitButton_ = nullptr;
    QPushButton *fitBackgroundButton_ = nullptr;
    QPushButton *touchControlsButton_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QHash<int, QGraphicsRectItem *> mapItemsByLine_;
    QVector<QGraphicsRectItem *> draftGeometryItems_;
    QVector<QGraphicsPixmapItem *> backgroundImageItems_;
    QUndoStack *undoStack_ = nullptr;
    int nextDraftGeometryId_ = 1;
    WorkspaceMode workspaceMode_ = WorkspaceMode::Split;
    QString projectRootPath_;
    QString toolbarStatusNote_;
    bool updatingSelection_ = false;
    bool updatingBackgroundLayerControls_ = false;
    bool autoFitEnabled_ = true;
    qreal zoomFactor_ = 1.0;
    bool fitBackgroundRequested_ = false;
    bool mapPanActive_ = false;
    QPoint mapPanLastPosition_;
    bool primaryPointerInteractionActive_ = false;
    bool selectModeActive_ = true;
    bool touchPanCandidate_ = false;
    bool touchPanActive_ = false;
    QPointF touchPanStartPosition_;
    QPointF touchPanLastPosition_;
    QDateTime lastTabletInteractionUtc_;
    bool nativeZoomGestureActive_ = false;
    QDateTime lastNativeZoomGestureUtc_;
    bool touchFriendlyControlsEnabled_ = false;
    bool helpCollapsed_ = false;
    int helpPanelHeight_ = 180;
    int selectedBackgroundLayerIndex_ = -1;
    QPointer<QMainWindow> detachedMapPaneWindow_;
    bool mapPaneDetached_ = false;
    bool reattachingMapPane_ = false;
};
}
