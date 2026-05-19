#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSet>
#include <QVector>
#include <QPointer>
#include <QPersistentModelIndex>
#include <optional>

class QLabel;
class QGraphicsScene;
class QGraphicsView;
class QLineEdit;
class QSplitter;
class QToolButton;
class QPushButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QSlider;
class QStandardItemModel;
class QTabWidget;
class QTreeView;
class QGraphicsRectItem;
class QGraphicsPixmapItem;
class QGraphicsPathItem;
class QUndoStack;
class QMainWindow;
class QGraphicsItem;
class QShortcut;
class QModelIndex;

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
        Visual,
        Raw
    };
    enum class InteractiveDrawMode
    {
        None,
        Point,
        Line,
        Freehand,
        Area
    };

    explicit MapEditorTab(QWidget *parent = nullptr);
    ~MapEditorTab() override;

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
    QString statusPathText() const;
    QString statusEncodingText() const;
    QString statusModeText() const;
    int zoomPercent() const;
    bool canUndo() const;
    bool canRedo() const;
    InteractiveDrawMode interactiveDrawMode() const;
    bool canCompleteDraftAction() const;
    bool isTouchFriendlyControlsEnabled() const;
    void triggerUndo();
    void triggerRedo();
    void triggerZoomIn();
    void triggerZoomOut();
    void triggerFit();
    void triggerFitWithBackground();
    void triggerSelectMode();
    void triggerInsertScrap();
    void triggerCompleteDraft();
    void triggerAddPoint();
    void triggerAddLine();
    void triggerAddFreehandLine();
    void triggerAddSmartTraceLine();
    void triggerAddArea();
    void setTouchControlsEnabled(bool enabled);
    bool isInsertModeActive() const;
    bool isMapPaneDetached() const;
    QString mapPaneWindowActionText() const;
    QString mapPaneWindowActionToolTip() const;
    WorkspaceMode workspaceMode() const;
    void setInlineWorkspaceModeSelectorVisible(bool visible);
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
    void setMapGridVisible(bool visible);
    void setMapGridSpacingMeters(qreal spacingMeters);

public slots:
    void setWorkspaceMode(WorkspaceMode mode);
    void toggleMapPaneWindow();

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void documentTextChanged();
    void backgroundLayersChanged();
    void modeStatusChanged();
    void workspaceModeChanged(TherionStudio::MapEditorTab::WorkspaceMode mode);
    void mapPaneDetachStateChanged(bool detached);
    void zoomStatusChanged(int zoomPercent);
    void commandSurfaceStateChanged();
    void openDedicatedWindowRequested(TherionStudio::MapEditorTab *tab);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void handleTextEditorCurrentLineChanged(int lineNumber);
    void handleTextEditorCursorPositionChanged(int lineNumber, int columnNumber);
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

private:
    void buildUi();
    void buildMapScene();
    void refreshMapScene();
    void refreshMapScenePreservingUndoStack();
    void flushPendingMapSceneRefreshAfterCommand();
    void clearMapScene();
    void clearDraftGeometryItems();
    void clearBackgroundImageItems();
    void restoreDraftGeometryItems();
    void restoreBackgroundImageItems();
    void selectMapLine(int lineNumber, bool centerOnSelection = true);
    void selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection = true);
    void setInteractiveDrawMode(InteractiveDrawMode mode);
    bool handleInteractiveDrawClick(const QPointF &scenePosition);
    bool commitInteractiveDrawSession();
    void clearInteractiveDrawSession(bool clearMode);
    void updateInteractiveDrawPreview();
    QRectF mapSourceBoundsForCurrentDocument() const;
    QPointF sourcePointFromScenePosition(const QPointF &scenePosition) const;
    bool hasCompletableInteractiveDrawSession() const;
    bool commitInteractiveDrawVertices(const QString &geometryKind,
                                       const QVector<QPointF> &vertices,
                                       const QString &successLabel);
    bool cancelInteractiveDrawingToSelectMode();
    QStringList lineCoordinateRowsForInteractiveDraft() const;
    QStringList areaCoordinateRowsForInteractiveDraft() const;
    void captureInteractiveLineAnchor(const QPointF &anchorScenePoint,
                                      const std::optional<QPointF> &dragScenePoint);
    struct InteractiveLineControlHandleRef
    {
        enum class Kind
        {
            Incoming,
            Outgoing
        };

        int vertexIndex = -1;
        Kind kind = Kind::Incoming;
    };
    std::optional<InteractiveLineControlHandleRef> interactiveLineControlAt(const QPointF &scenePosition,
                                                                            qreal sceneRadius) const;
    bool setInteractiveLineControlScenePoint(const InteractiveLineControlHandleRef &handle,
                                             const QPointF &scenePoint);
    void fitMapToView(bool includeBackgroundImages = false);
    void syncZoomFactorFromView();
    void applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition);
    void refreshToolbarSummary();
    void adjustMapZoom(qreal factor);
    QRectF mapGeometryFitBounds() const;
    QRectF mapBackgroundFitBounds() const;
    QRectF mapPreviewBounds() const;
    void addBackgroundImage(const QString &imagePath, bool writeXtherionMetadata = false);
    void refreshBackgroundLayerControls();
    void applyBackgroundLayerStackingOrder();
    void saveBackgroundLayersToSession() const;
    void loadBackgroundLayersFromSession();
    void loadBackgroundLayersFromDocumentMetadata();
    void syncAutoBackgroundLayersFromCurrentDocument();
    void reprojectMetadataBackgroundLayersForCurrentDocument();
    QRectF xtherionAutoAreaAdjustRect() const;
    void syncBackgroundLayerXtherionMetadata(QGraphicsPixmapItem *item, const QString &label);
    void removeBackgroundLayerXtherionMetadata(const QString &layerPath, const QString &label);
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
    void recordSourceTextSnapshot(const QString &label,
                                  const QString &beforeText,
                                  const QString &afterText,
                                  int insertedLineNumber);
    bool insertLineVertexFromSelection();
    bool removeLineVertexFromSelection();
    bool toggleLineVertexSmoothFromSelection();
    void recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition);
    void recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible);
    QGraphicsRectItem *selectedDraftGeometryItem() const;
    QGraphicsRectItem *createDraftGeometryItem(DraftGeometryKind kind);
    void addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position);
    void removeDraftGeometryItem(QGraphicsRectItem *item);
    void updateHelpPanel();
    void updateWorkspaceVisibility();
    void updateGeometrySelectionPresentation();
    void syncMapSelectionFromTextCursor(int lineNumber, int columnNumber);
    void detachMapPaneToWindow();
    void reattachMapPaneFromWindow();
    void focusDetachedMapPaneWindow();
    void refreshTitle();
    void refreshStatus();
    QString displayPath() const;
    void setTouchFriendlyControlsEnabled(bool enabled);
    void handleApplicationAppearanceChanged();
    void refreshWorkspaceModeUi();
    void rebuildInspectorObjectsTree();
    QModelIndex findInspectorObjectIndexForLine(int lineNumber) const;
    void syncInspectorObjectSelectionToLine(int lineNumber);
    void syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection);
    void setInspectorObjectCurrentIndex(const QModelIndex &index);
    void configureInspectorObjectTreeColumns();
    void clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers = {});
    void handleInspectorObjectSelectionChanged(const QModelIndex &current);
    void handleInspectorObjectClicked(const QModelIndex &index);
    void applyInspectorObjectVisibility();
    void configureInspectorBackgroundLayerTreeColumns();
    void handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current);
    void handleInspectorBackgroundLayerClicked(const QModelIndex &index);
    void refreshInspectorBackgroundPanel();
    void refreshObjectDetailsPanel();
    void applyObjectOrientationEdits();
    void showSelectedObjectInSource();
    void deleteSelectedObjectFromSelection();
    void applyObjectQuickFieldEdits();
    void insertVertexFromSelectionPanel();
    void deleteVertexFromSelectionPanel();
    void toggleVertexSmoothFromSelectionPanel();
    void populateScrapScaleFromSourceBounds();
    void applyScrapScaleEdits();
    void handleConfigureObjectSettingsTriggered();
    void handleObjectOrientationEnabledToggled(bool checked);
    void handleLineClosedToggled(bool checked);
    void handleLineReversedToggled(bool checked);

    QWidget *workspaceModeRow_ = nullptr;
    QPushButton *visualModeButton_ = nullptr;
    QPushButton *rawModeButton_ = nullptr;
    TextEditorTab *textEditor_ = nullptr;
    QGraphicsView *mapView_ = nullptr;
    QGraphicsScene *mapScene_ = nullptr;
    QWidget *mapPaneContainer_ = nullptr;
    QSplitter *mapDetailsSplitter_ = nullptr;
    QWidget *objectDetailsPanel_ = nullptr;
    QTabWidget *mapInspectorTabs_ = nullptr;
    QTreeView *mapObjectsTree_ = nullptr;
    QStandardItemModel *mapObjectsModel_ = nullptr;
    QTreeView *mapBackgroundLayersTree_ = nullptr;
    QStandardItemModel *mapBackgroundLayersModel_ = nullptr;
    QToolButton *mapBackgroundAddButton_ = nullptr;
    QPushButton *mapBackgroundMoveUpButton_ = nullptr;
    QPushButton *mapBackgroundMoveDownButton_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosXSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosYSpin_ = nullptr;
    QSlider *mapBackgroundOpacitySlider_ = nullptr;
    QSlider *mapBackgroundGammaSlider_ = nullptr;
    QPushButton *mapBackgroundOpacityResetButton_ = nullptr;
    QPushButton *mapBackgroundGammaResetButton_ = nullptr;
    QCheckBox *mapGridVisibleCheck_ = nullptr;
    QDoubleSpinBox *mapGridSpacingSpin_ = nullptr;
    bool updatingMapInspectorBackgroundUi_ = false;
    bool updatingMapInspectorObjectSelection_ = false;
    QSet<int> hiddenInspectorObjectLines_;
    QPersistentModelIndex inspectorObjectPressedNameIndex_;
    bool inspectorObjectPressedWasSelected_ = false;
    int lastInspectorClickedObjectLineNumber_ = 0;
    QSet<int> suppressedInspectorAutoReselectLineNumbers_;
    QLabel *objectDetailsSelectionLabel_ = nullptr;
    QWidget *objectSelectionSection_ = nullptr;
    QWidget *vertexSelectionSection_ = nullptr;
    QWidget *geometrySelectionSection_ = nullptr;
    QWidget *advancedSelectionSection_ = nullptr;
    QPushButton *objectShowSourceButton_ = nullptr;
    QPushButton *objectDeleteButton_ = nullptr;
    QWidget *objectQuickFieldsEditor_ = nullptr;
    QLabel *objectQuickIdentifierLabel_ = nullptr;
    QLineEdit *objectQuickTypeEdit_ = nullptr;
    QLineEdit *objectQuickSubtypeEdit_ = nullptr;
    QLineEdit *objectQuickIdentifierEdit_ = nullptr;
    QPushButton *objectQuickApplyButton_ = nullptr;
    QString objectQuickIdentifierOption_;
    QWidget *vertexActionsEditor_ = nullptr;
    QPushButton *vertexInsertButton_ = nullptr;
    QPushButton *vertexDeleteButton_ = nullptr;
    QPushButton *vertexToggleSmoothButton_ = nullptr;
    QLabel *objectDetailsLineLabel_ = nullptr;
    QLabel *objectDetailsKindLabel_ = nullptr;
    QWidget *lineOptionsEditor_ = nullptr;
    QCheckBox *lineClosedCheck_ = nullptr;
    QCheckBox *lineReversedCheck_ = nullptr;
    QWidget *scrapScaleEditor_ = nullptr;
    QDoubleSpinBox *scrapScaleSourceX1Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleSourceY1Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleSourceX2Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleSourceY2Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleRealX1Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleRealY1Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleRealX2Spin_ = nullptr;
    QDoubleSpinBox *scrapScaleRealY2Spin_ = nullptr;
    QComboBox *scrapScaleUnitCombo_ = nullptr;
    QPushButton *scrapScaleUseBoundsButton_ = nullptr;
    QPushButton *scrapScaleApplyButton_ = nullptr;
    QWidget *objectOrientationEditor_ = nullptr;
    QCheckBox *objectOrientationEnabledCheck_ = nullptr;
    QDoubleSpinBox *objectOrientationSpin_ = nullptr;
    QPushButton *objectOrientationApplyButton_ = nullptr;
    QPushButton *objectConfigureButton_ = nullptr;
    bool updatingObjectDetailsUi_ = false;
    int selectedObjectLineNumber_ = 0;
    int selectedObjectVertexIndex_ = -1;
    QString selectedObjectKind_;
    std::optional<QPointF> selectedObjectCoordinate_;
    QSplitter *splitter_ = nullptr;
    QShortcut *cancelDrawShortcut_ = nullptr;
    QShortcut *commitDrawShortcut_ = nullptr;
    QHash<int, QGraphicsItem *> mapItemsByLine_;
    QVector<QGraphicsRectItem *> draftGeometryItems_;
    QVector<QGraphicsPixmapItem *> backgroundImageItems_;
    QUndoStack *undoStack_ = nullptr;
    int nextDraftGeometryId_ = 1;
    WorkspaceMode workspaceMode_ = WorkspaceMode::Visual;
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
    bool mapGridVisible_ = true;
    qreal mapGridSpacingMeters_ = 10.0;
    QDateTime lastNativeZoomGestureUtc_;
    bool touchFriendlyControlsEnabled_ = false;
    int selectedBackgroundLayerIndex_ = -1;
    bool mapCommandApplyInProgress_ = false;
    bool mapSceneRefreshPending_ = false;
    QPointer<QMainWindow> detachedMapPaneWindow_;
    bool mapPaneDetached_ = false;
    bool inlineWorkspaceModeSelectorVisible_ = true;
    bool reattachingMapPane_ = false;
    bool mapSelectionDrivenTextNavigationInProgress_ = false;
    int lastCursorSyncedLine_ = -1;
    int lastCursorSyncedColumn_ = -1;
    bool pendingMapClickSelection_ = false;
    QPointF pendingMapClickScenePosition_;
    QElapsedTimer pendingMapClickElapsed_;
    int pendingMapClickLineNumber_ = 0;
    int pendingMapClickSourceVertexIndex_ = -1;
    QString pendingMapClickGeometryKind_;
    InteractiveDrawMode interactiveDrawMode_ = InteractiveDrawMode::None;
    QVector<QPointF> interactiveDrawSourceVertices_;
    QVector<QPointF> interactiveDrawSceneVertices_;
    struct InteractiveLineDraftVertex
    {
        QPointF anchorSource;
        QPointF anchorScene;
        std::optional<QPointF> incomingControlSource;
        std::optional<QPointF> incomingControlScene;
        std::optional<QPointF> outgoingControlSource;
        std::optional<QPointF> outgoingControlScene;
    };
    QVector<InteractiveLineDraftVertex> interactiveDrawLineVertices_;
    bool interactiveDrawStrokeActive_ = false;
    bool interactiveDrawAnchorPressActive_ = false;
    QPointF interactiveDrawAnchorPressScenePoint_;
    bool interactiveDrawAnchorDragActive_ = false;
    QPointF interactiveDrawAnchorDragScenePoint_;
    bool interactiveDrawControlDragActive_ = false;
    InteractiveLineControlHandleRef interactiveDrawControlDragHandle_;
    bool interactiveDrawHoverActive_ = false;
    QPointF interactiveDrawHoverScenePoint_;
    QGraphicsPathItem *interactiveDrawPreviewPath_ = nullptr;
    QVector<QGraphicsItem *> interactiveDrawPreviewMarkers_;
};
}
