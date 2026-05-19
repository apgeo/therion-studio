#include "MapEditorTab.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QListWidget>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QScopedValueRollback>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <algorithm>
#include <functional>

#include "TextEditorTab.h"
#include "../core/SessionStore.h"

namespace TherionStudio
{
namespace
{
QPushButton *createDetachedToolbarButton(QWidget *parent, const QString &text, const QString &iconName)
{
    auto *button = new QPushButton(text, parent);
    button->setAutoDefault(false);
    button->setIcon(QIcon(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName)));
    button->setIconSize(QSize(14, 14));
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return button;
}

QToolButton *createDetachedIconButton(QWidget *parent, const QString &toolTip, const QString &iconName)
{
    auto *button = new QToolButton(parent);
    button->setAutoRaise(false);
    button->setIcon(QIcon(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName)));
    button->setIconSize(QSize(14, 14));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setFixedSize(QSize(26, 26));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QFrame *createDetachedToolbarSeparator(QWidget *parent)
{
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName(QStringLiteral("workspaceToolbarSeparator"));
    return separator;
}

void applyDetachedButtonMinimumWidth(QPushButton *button)
{
    if (button == nullptr) {
        return;
    }

    QString text = button->text();
    text.remove(QLatin1Char('&'));
    const QFontMetrics metrics(button->font());
    const int textWidth = metrics.horizontalAdvance(text);
    const int iconWidth = button->icon().isNull() ? 0 : (button->iconSize().width() + 8);
    const int framePadding = 28;
    button->setMinimumWidth(textWidth + iconWidth + framePadding);
}

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

class DetachedMapPaneWindow final : public QMainWindow
{
public:
    explicit DetachedMapPaneWindow(MapEditorTab *mapTab, QWidget *parent = nullptr)
        : QMainWindow(parent, Qt::Window)
        , mapTab_(mapTab)
    {
        setAttribute(Qt::WA_DeleteOnClose, true);
        auto *centralHost = new QWidget(this);
        auto *centralLayout = new QVBoxLayout(centralHost);
        centralLayout->setContentsMargins(0, 0, 0, 0);
        centralLayout->setSpacing(0);
        commandBar_ = new QWidget(centralHost);
        commandBar_->setObjectName(QStringLiteral("workspaceCommandBar"));
        commandBar_->setAttribute(Qt::WA_StyledBackground, true);
        commandBar_->setStyleSheet(QStringLiteral(
            "QWidget#workspaceCommandBar {"
            " border-bottom: none;"
            " border-top: 1px solid palette(mid);"
            " border-left: none;"
            " border-right: none;"
            " background-color: palette(base);"
            "}"
            "QFrame#workspaceToolbarSeparator {"
            " color: palette(mid);"
            " margin-left: 4px;"
            " margin-right: 4px;"
            "}"
            "QWidget#workspaceCommandBar QToolButton {"
            " min-width: 26px;"
            " max-width: 26px;"
            " min-height: 26px;"
            " max-height: 26px;"
            " border: 1px solid palette(mid);"
            " border-radius: 6px;"
            " padding: 0px;"
            " background-color: palette(button);"
            "}"
            "QWidget#workspaceCommandBar QPushButton {"
            " min-height: 26px;"
            " border: 1px solid palette(mid);"
            " border-radius: 6px;"
            " padding: 0 10px;"
            " background-color: palette(button);"
            "}"
            "QWidget#workspaceCommandBar QToolButton:hover,"
            "QWidget#workspaceCommandBar QPushButton:hover {"
            " background-color: palette(light);"
            "}"
            "QWidget#workspaceCommandBar QToolButton:pressed,"
            "QWidget#workspaceCommandBar QToolButton:checked,"
            "QWidget#workspaceCommandBar QPushButton:pressed,"
            "QWidget#workspaceCommandBar QPushButton:checked {"
            " background-color: palette(midlight);"
            "}"
            "QWidget#workspaceCommandBar QToolButton:disabled,"
            "QWidget#workspaceCommandBar QPushButton:disabled {"
            " color: palette(mid);"
            " border-color: palette(mid);"
            "}"));
        auto *commandLayout = new QHBoxLayout(commandBar_);
        commandLayout->setContentsMargins(4, 4, 8, 4);
        commandLayout->setSpacing(4);

        saveButton_ = createDetachedIconButton(commandBar_, QObject::tr("Save"), QStringLiteral("save"));
        undoButton_ = createDetachedIconButton(commandBar_, QObject::tr("Undo"), QStringLiteral("undo-2"));
        redoButton_ = createDetachedIconButton(commandBar_, QObject::tr("Redo"), QStringLiteral("redo-2"));
        commandLayout->addWidget(saveButton_);
        commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));
        commandLayout->addWidget(undoButton_);
        commandLayout->addWidget(redoButton_);
        commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));

        zoomInButton_ = createDetachedIconButton(commandBar_, QObject::tr("Zoom In"), QStringLiteral("zoom-in"));
        zoomOutButton_ = createDetachedIconButton(commandBar_, QObject::tr("Zoom Out"), QStringLiteral("zoom-out"));
        fitButton_ = createDetachedIconButton(commandBar_, QObject::tr("Fit"), QStringLiteral("scan"));
        fitBackgroundButton_ = createDetachedIconButton(commandBar_, QObject::tr("Fit With Background"), QStringLiteral("image"));
        commandLayout->addWidget(zoomInButton_);
        commandLayout->addWidget(zoomOutButton_);
        commandLayout->addWidget(fitButton_);
        commandLayout->addWidget(fitBackgroundButton_);
        commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));

        selectButton_ = createDetachedIconButton(commandBar_, QObject::tr("Select"), QStringLiteral("mouse-pointer-2"));
        completeDraftButton_ = createDetachedIconButton(commandBar_, QObject::tr("Complete Draft"), QStringLiteral("check"));
        insertScrapButton_ = createDetachedIconButton(commandBar_, QObject::tr("Insert Scrap"), QStringLiteral("puzzle"));
        pointButton_ = createDetachedIconButton(commandBar_, QObject::tr("Point"), QStringLiteral("locate-fixed"));
        lineButton_ = createDetachedIconButton(commandBar_, QObject::tr("Line"), QStringLiteral("spline"));
        freehandLineButton_ = createDetachedIconButton(commandBar_, QObject::tr("Freehand"), QStringLiteral("pencil-line"));
        smartTraceLineButton_ = createDetachedIconButton(commandBar_, QObject::tr("Smart Trace"), QStringLiteral("wand-sparkles"));
        areaButton_ = createDetachedIconButton(commandBar_, QObject::tr("Area"), QStringLiteral("pentagon"));
        touchControlsButton_ = createDetachedIconButton(commandBar_, QObject::tr("Touch Controls"), QStringLiteral("hand"));
        selectButton_->setCheckable(true);
        pointButton_->setCheckable(true);
        lineButton_->setCheckable(true);
        freehandLineButton_->setCheckable(true);
        smartTraceLineButton_->setCheckable(true);
        areaButton_->setCheckable(true);
        touchControlsButton_->setCheckable(true);
        commandLayout->addWidget(selectButton_);
        commandLayout->addWidget(completeDraftButton_);
        commandLayout->addWidget(insertScrapButton_);
        commandLayout->addWidget(pointButton_);
        commandLayout->addWidget(lineButton_);
        commandLayout->addWidget(freehandLineButton_);
        commandLayout->addWidget(smartTraceLineButton_);
        commandLayout->addWidget(areaButton_);
        commandLayout->addWidget(touchControlsButton_);
        commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));
        commandLayout->addStretch(1);

        mapWindowButton_ = createDetachedToolbarButton(commandBar_, QObject::tr("Return Map"), QStringLiteral("external-link"));
        applyDetachedButtonMinimumWidth(mapWindowButton_);
        commandLayout->addWidget(mapWindowButton_);

        centralLayout->addWidget(commandBar_);
        setCentralWidget(centralHost);

        if (mapTab_ != nullptr) {
            connect(saveButton_, &QToolButton::clicked, mapTab_, [this]() {
                if (mapTab_ != nullptr) {
                    mapTab_->save();
                }
            });
            connect(undoButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerUndo);
            connect(redoButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerRedo);
            connect(zoomInButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerZoomIn);
            connect(zoomOutButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerZoomOut);
            connect(fitButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerFit);
            connect(fitBackgroundButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerFitWithBackground);
            connect(selectButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerSelectMode);
            connect(completeDraftButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerCompleteDraft);
            connect(insertScrapButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerInsertScrap);
            connect(pointButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddPoint);
            connect(lineButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddLine);
            connect(freehandLineButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddFreehandLine);
            connect(smartTraceLineButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddSmartTraceLine);
            connect(areaButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddArea);
            connect(touchControlsButton_, &QToolButton::toggled, mapTab_, &MapEditorTab::setTouchControlsEnabled);
            connect(mapWindowButton_, &QPushButton::clicked, mapTab_, &MapEditorTab::toggleMapPaneWindow);
            connect(mapTab_, &MapEditorTab::mapPaneDetachStateChanged, this, [this](bool) {
                refreshCommandBarState();
            });
            connect(mapTab_, &MapEditorTab::zoomStatusChanged, this, [this](int) {
                refreshCommandBarState();
            });
            connect(mapTab_, &MapEditorTab::modeStatusChanged, this, [this]() {
                refreshCommandBarState();
            });
            connect(mapTab_, &MapEditorTab::backgroundLayersChanged, this, [this]() {
                refreshCommandBarState();
            });
            connect(mapTab_, &MapEditorTab::commandSurfaceStateChanged, this, [this]() {
                refreshCommandBarState();
            });
            refreshCommandBarState();
        }

        zoomLabel_ = new QLabel(statusBar());
        zoomLabel_->setAlignment(Qt::AlignCenter);
        zoomLabel_->setMinimumWidth(56);
        modeLabel_ = new QLabel(statusBar());
        modeLabel_->setAlignment(Qt::AlignCenter);
        modeLabel_->setMinimumWidth(78);
        statusBar()->addPermanentWidget(zoomLabel_, 0);
        statusBar()->addPermanentWidget(modeLabel_, 0);
    }

    void setMapPaneWidget(QWidget *mapPaneWidget)
    {
        if (mapPaneWidget == nullptr || centralWidget() == nullptr) {
            return;
        }

        auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
        if (layout == nullptr) {
            return;
        }

        if (layout->indexOf(mapPaneWidget) < 0) {
            layout->addWidget(mapPaneWidget, 1);
        }
    }

    void setMapStatus(int zoomPercent, bool insertMode, const QString &modeText)
    {
        if (zoomLabel_ != nullptr) {
            zoomLabel_->setText(QObject::tr("%1%").arg(zoomPercent));
            zoomLabel_->setToolTip(QObject::tr("Map zoom"));
        }
        if (modeLabel_ != nullptr) {
            const QString badgeText = insertMode ? QObject::tr("Insert") : QObject::tr("Select");
            const QString background = insertMode
                ? QStringLiteral("#d34a4a")
                : QStringLiteral("#2e9f5c");
            modeLabel_->setText(badgeText);
            modeLabel_->setToolTip(modeText);
            modeLabel_->setStyleSheet(QStringLiteral(
                                          "QLabel {"
                                          " color: white;"
                                          " font-weight: 700;"
                                          " background-color: %1;"
                                          " border-radius: 4px;"
                                          " padding: 1px 8px;"
                                          " min-height: 18px;"
                                          "}").arg(background));
        }
    }

    void setCloseCallback(std::function<void()> callback)
    {
        closeCallback_ = std::move(callback);
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (closeCallback_) {
            closeCallback_();
        }
        QMainWindow::closeEvent(event);
    }

private:
    void refreshCommandBarState()
    {
        if (mapTab_ == nullptr) {
            return;
        }

        const QSignalBlocker selectBlocker(selectButton_);
        const QSignalBlocker pointBlocker(pointButton_);
        const QSignalBlocker lineBlocker(lineButton_);
        const QSignalBlocker freehandBlocker(freehandLineButton_);
        const QSignalBlocker smartTraceBlocker(smartTraceLineButton_);
        const QSignalBlocker areaBlocker(areaButton_);
        const QSignalBlocker touchBlocker(touchControlsButton_);
        undoButton_->setEnabled(mapTab_->canUndo());
        redoButton_->setEnabled(mapTab_->canRedo());
        fitBackgroundButton_->setEnabled(mapTab_->backgroundLayerCount() > 0);
        completeDraftButton_->setEnabled(mapTab_->canCompleteDraftAction());
        mapWindowButton_->setText(mapTab_->mapPaneWindowActionText());
        mapWindowButton_->setToolTip(mapTab_->mapPaneWindowActionToolTip());
        applyDetachedButtonMinimumWidth(mapWindowButton_);

        const MapEditorTab::InteractiveDrawMode drawMode = mapTab_->interactiveDrawMode();
        selectButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::None);
        pointButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Point);
        lineButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Line);
        freehandLineButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Freehand);
        smartTraceLineButton_->setChecked(false);
        areaButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Area);
        touchControlsButton_->setChecked(mapTab_->isTouchFriendlyControlsEnabled());
    }

    QPointer<MapEditorTab> mapTab_;
    QWidget *commandBar_ = nullptr;
    QToolButton *saveButton_ = nullptr;
    QToolButton *undoButton_ = nullptr;
    QToolButton *redoButton_ = nullptr;
    QToolButton *zoomInButton_ = nullptr;
    QToolButton *zoomOutButton_ = nullptr;
    QToolButton *fitButton_ = nullptr;
    QToolButton *fitBackgroundButton_ = nullptr;
    QToolButton *selectButton_ = nullptr;
    QToolButton *completeDraftButton_ = nullptr;
    QToolButton *insertScrapButton_ = nullptr;
    QToolButton *pointButton_ = nullptr;
    QToolButton *lineButton_ = nullptr;
    QToolButton *freehandLineButton_ = nullptr;
    QToolButton *smartTraceLineButton_ = nullptr;
    QToolButton *areaButton_ = nullptr;
    QToolButton *touchControlsButton_ = nullptr;
    QPushButton *mapWindowButton_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QLabel *modeLabel_ = nullptr;
    std::function<void()> closeCallback_;
};
}

MapEditorTab::MapEditorTab(QWidget *parent)
    : QWidget(parent)
{
    undoStack_ = new QUndoStack(this);
    workspaceMode_ = WorkspaceMode::Visual;
    touchFriendlyControlsEnabled_ = SessionStore::therionMapTouchFriendlyControlsEnabled();
    buildUi();
    connect(this, &MapEditorTab::zoomStatusChanged, this, [this](int) {
        refreshStatus();
    });
    connect(this, &MapEditorTab::modeStatusChanged, this, [this]() {
        refreshStatus();
    });
    updateWorkspaceVisibility();
    setTouchFriendlyControlsEnabled(touchFriendlyControlsEnabled_);

    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            handleApplicationAppearanceChanged();
        });
    }
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }
    handleApplicationAppearanceChanged();
}

MapEditorTab::~MapEditorTab()
{
    if (mapScene_ != nullptr) {
        disconnect(mapScene_, nullptr, this, nullptr);
    }
    mapScene_ = nullptr;
}

void MapEditorTab::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    workspaceModeRow_ = new QWidget(this);
    auto *workspaceModeLayout = new QHBoxLayout(workspaceModeRow_);
    workspaceModeLayout->setContentsMargins(12, 10, 12, 8);
    workspaceModeLayout->setSpacing(8);
    workspaceModeLayout->addWidget(new QLabel(tr("Mode:"), workspaceModeRow_));
    visualModeButton_ = new QPushButton(tr("Visual"), workspaceModeRow_);
    visualModeButton_->setCheckable(true);
    rawModeButton_ = new QPushButton(tr("Raw"), workspaceModeRow_);
    rawModeButton_->setCheckable(true);
    workspaceModeLayout->addWidget(visualModeButton_);
    workspaceModeLayout->addWidget(rawModeButton_);
    workspaceModeLayout->addStretch(1);

    connect(visualModeButton_, &QPushButton::clicked, this, [this]() {
        setWorkspaceMode(WorkspaceMode::Visual);
    });
    connect(rawModeButton_, &QPushButton::clicked, this, [this]() {
        setWorkspaceMode(WorkspaceMode::Raw);
    });
    workspaceModeRow_->setVisible(inlineWorkspaceModeSelectorVisible_);
    workspaceModeRow_->setMaximumHeight(inlineWorkspaceModeSelectorVisible_ ? QWIDGETSIZE_MAX : 0);

    toolbarStatusNote_ = tr("Ready");

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(splitter_, QStringLiteral("mapEditorWorkspaceSplitter"));

    textEditor_ = new TextEditorTab(splitter_);
    textEditor_->setInlineStatusVisible(false);
    textEditor_->setModeSelectorVisible(false);
    connect(textEditor_, &TextEditorTab::titleChanged, this, &MapEditorTab::titleChanged);
    connect(textEditor_, &TextEditorTab::dirtyStateChanged, this, &MapEditorTab::dirtyStateChanged);
    connect(textEditor_, &TextEditorTab::currentLineChanged, this, &MapEditorTab::handleTextEditorCurrentLineChanged);
    connect(textEditor_, &TextEditorTab::cursorPositionChanged, this, &MapEditorTab::handleTextEditorCursorPositionChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::refreshMapScene);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::rebuildInspectorObjectsTree);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::documentTextChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(this, &MapEditorTab::backgroundLayersChanged, this, &MapEditorTab::refreshInspectorBackgroundPanel);

    connect(undoStack_, &QUndoStack::canUndoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::canRedoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::indexChanged, this, &MapEditorTab::updateCommandSurfaceState);

    mapPaneContainer_ = new QWidget(splitter_);
    mapPaneContainer_->setMinimumWidth(420);
    auto *mapPaneLayout = new QVBoxLayout(mapPaneContainer_);
    mapPaneLayout->setContentsMargins(0, 0, 0, 0);
    mapPaneLayout->setSpacing(0);

    mapPaneTopSeparator_ = new QFrame(mapPaneContainer_);
    mapPaneTopSeparator_->setObjectName(QStringLiteral("mapPaneTopSeparator"));
    mapPaneTopSeparator_->setFrameShape(QFrame::NoFrame);
    mapPaneTopSeparator_->setFixedHeight(1);
    mapPaneTopSeparator_->setStyleSheet(QStringLiteral(
        "QFrame#mapPaneTopSeparator {"
        " background-color: palette(mid);"
        " border: none;"
        "}"));

    mapDetailsSplitter_ = new QSplitter(Qt::Horizontal, mapPaneContainer_);
    mapDetailsSplitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(mapDetailsSplitter_, QStringLiteral("mapEditorDetailsSplitter"));

    mapView_ = new QGraphicsView(mapDetailsSplitter_);
    mapView_->setFrameShape(QFrame::NoFrame);
    mapView_->setObjectName(QStringLiteral("mapCanvasView"));
    mapView_->setStyleSheet(QStringLiteral(
        "QGraphicsView#mapCanvasView {"
        " border-left: none;"
        " border-right: 1px solid palette(mid);"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    mapView_->setDragMode(QGraphicsView::NoDrag);
    mapView_->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    mapView_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    mapView_->setRenderHint(QPainter::Antialiasing, true);
    mapView_->setBackgroundBrush(palette().color(QPalette::Window));
    mapView_->setFocusPolicy(Qt::StrongFocus);
    mapView_->installEventFilter(this);
    if (mapView_->viewport() != nullptr) {
        mapView_->viewport()->setFocusPolicy(Qt::StrongFocus);
        mapView_->viewport()->installEventFilter(this);
    }

    buildMapScene();

    cancelDrawShortcut_ = new QShortcut(this);
    cancelDrawShortcut_->setKey(QKeySequence(Qt::Key_Escape));
    cancelDrawShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cancelDrawShortcut_, &QShortcut::activated, this, [this]() {
        cancelInteractiveDrawingToSelectMode();
    });

    commitDrawShortcut_ = new QShortcut(this);
    commitDrawShortcut_->setKeys(QList<QKeySequence>{QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
    commitDrawShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(commitDrawShortcut_, &QShortcut::activated, this, [this]() {
        commitInteractiveDrawSession();
    });

    objectDetailsPanel_ = new QFrame(mapDetailsSplitter_);
    objectDetailsPanel_->setObjectName(QStringLiteral("mapObjectDetailsPanel"));
    objectDetailsPanel_->setFrameShape(QFrame::NoFrame);
    objectDetailsPanel_->setAttribute(Qt::WA_StyledBackground, true);
    objectDetailsPanel_->setStyleSheet(QStringLiteral(
        "QFrame#mapObjectDetailsPanel {"
        " background-color: palette(base);"
        " border: none;"
        "}"));
    objectDetailsPanel_->setMinimumWidth(280);
    auto *objectDetailsLayout = new QVBoxLayout(objectDetailsPanel_);
    objectDetailsLayout->setContentsMargins(8, 8, 8, 8);
    objectDetailsLayout->setSpacing(8);

    auto *objectDetailsHeader = new QLabel(tr("Inspector"), objectDetailsPanel_);
    QFont objectDetailsHeaderFont = objectDetailsHeader->font();
    objectDetailsHeaderFont.setBold(true);
    objectDetailsHeader->setFont(objectDetailsHeaderFont);
    objectDetailsLayout->addWidget(objectDetailsHeader);

    mapInspectorTabs_ = new QTabWidget(objectDetailsPanel_);
    objectDetailsLayout->addWidget(mapInspectorTabs_, 1);

    auto *objectsTab = new QWidget(mapInspectorTabs_);
    auto *objectsLayout = new QVBoxLayout(objectsTab);
    objectsLayout->setContentsMargins(4, 4, 4, 4);
    objectsLayout->setSpacing(8);

    mapObjectsTree_ = new QTreeView(objectsTab);
    mapObjectsTree_->setRootIsDecorated(true);
    mapObjectsTree_->setAnimated(true);
    mapObjectsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapObjectsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapObjectsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapObjectsTree_->setAlternatingRowColors(true);
    mapObjectsTree_->setHeaderHidden(true);
    mapObjectsTree_->setIconSize(QSize(16, 16));
    mapObjectsModel_ = new QStandardItemModel(mapObjectsTree_);
    mapObjectsTree_->setModel(mapObjectsModel_);
    configureInspectorObjectTreeColumns();
    if (mapObjectsTree_->viewport() != nullptr) {
        mapObjectsTree_->viewport()->installEventFilter(this);
    }
    connect(mapObjectsTree_, &QTreeView::clicked, this, &MapEditorTab::handleInspectorObjectClicked);
    objectsLayout->addWidget(mapObjectsTree_, 1);
    if (mapObjectsTree_->selectionModel() != nullptr) {
        connect(mapObjectsTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
            handleInspectorObjectSelectionChanged(current);
        });
    }

    QFont sectionFont = objectsTab->font();
    sectionFont.setBold(true);

    auto *selectionTab = new QWidget(mapInspectorTabs_);
    auto *selectionTabLayout = new QVBoxLayout(selectionTab);
    selectionTabLayout->setContentsMargins(4, 4, 4, 4);
    selectionTabLayout->setSpacing(8);

    auto *selectionPanel = new QWidget(selectionTab);
    auto *selectionLayout = new QVBoxLayout(selectionPanel);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->setSpacing(8);

    objectDetailsSelectionLabel_ = new QLabel(tr("No map object selected."), selectionPanel);
    objectDetailsSelectionLabel_->setWordWrap(true);
    selectionLayout->addWidget(objectDetailsSelectionLabel_);

    auto createSelectionSection = [selectionPanel](const QString &title, QVBoxLayout **contentLayout) {
        auto *section = new QFrame(selectionPanel);
        section->setFrameShape(QFrame::StyledPanel);
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(8, 8, 8, 8);
        sectionLayout->setSpacing(6);
        auto *titleLabel = new QLabel(title, section);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        sectionLayout->addWidget(titleLabel);
        if (contentLayout != nullptr) {
            *contentLayout = sectionLayout;
        }
        return section;
    };

    QVBoxLayout *objectSelectionLayout = nullptr;
    objectSelectionSection_ = createSelectionSection(tr("Object"), &objectSelectionLayout);

    auto *objectActionsLayout = new QHBoxLayout;
    objectActionsLayout->setContentsMargins(0, 0, 0, 0);
    objectActionsLayout->setSpacing(6);
    objectShowSourceButton_ = new QPushButton(tr("Show in Source"), objectSelectionSection_);
    objectShowSourceButton_->setAutoDefault(false);
    objectDeleteButton_ = new QPushButton(tr("Delete"), objectSelectionSection_);
    objectDeleteButton_->setAutoDefault(false);
    connect(objectShowSourceButton_, &QPushButton::clicked, this, &MapEditorTab::showSelectedObjectInSource);
    connect(objectDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteSelectedObjectFromSelection);
    objectActionsLayout->addWidget(objectShowSourceButton_);
    objectActionsLayout->addWidget(objectDeleteButton_);
    objectSelectionLayout->addLayout(objectActionsLayout);

    auto *objectDetailsForm = new QFormLayout;
    objectDetailsForm->setContentsMargins(0, 0, 0, 0);
    objectDetailsForm->setSpacing(6);
    objectDetailsLineLabel_ = new QLabel(QStringLiteral("—"), selectionPanel);
    objectDetailsKindLabel_ = new QLabel(QStringLiteral("—"), selectionPanel);
    objectDetailsForm->addRow(tr("Source Line"), objectDetailsLineLabel_);
    objectDetailsForm->addRow(tr("Kind"), objectDetailsKindLabel_);
    objectSelectionLayout->addLayout(objectDetailsForm);

    objectQuickFieldsEditor_ = new QWidget(objectSelectionSection_);
    auto *objectQuickForm = new QFormLayout(objectQuickFieldsEditor_);
    objectQuickForm->setContentsMargins(0, 0, 0, 0);
    objectQuickForm->setSpacing(6);
    objectQuickTypeEdit_ = new QLineEdit(objectQuickFieldsEditor_);
    objectQuickSubtypeEdit_ = new QLineEdit(objectQuickFieldsEditor_);
    objectQuickIdentifierEdit_ = new QLineEdit(objectQuickFieldsEditor_);
    objectQuickIdentifierLabel_ = new QLabel(tr("ID"), objectQuickFieldsEditor_);
    objectQuickApplyButton_ = new QPushButton(tr("Apply Object Fields"), objectQuickFieldsEditor_);
    objectQuickApplyButton_->setAutoDefault(false);
    connect(objectQuickApplyButton_, &QPushButton::clicked, this, &MapEditorTab::applyObjectQuickFieldEdits);
    objectQuickForm->addRow(tr("Type"), objectQuickTypeEdit_);
    objectQuickForm->addRow(tr("Subtype"), objectQuickSubtypeEdit_);
    objectQuickForm->addRow(objectQuickIdentifierLabel_, objectQuickIdentifierEdit_);
    objectQuickForm->addRow(QString(), objectQuickApplyButton_);
    objectSelectionLayout->addWidget(objectQuickFieldsEditor_);
    selectionLayout->addWidget(objectSelectionSection_);

    QVBoxLayout *vertexSelectionLayout = nullptr;
    vertexSelectionSection_ = createSelectionSection(tr("Point / Vertex"), &vertexSelectionLayout);

    objectOrientationEditor_ = new QWidget(vertexSelectionSection_);
    auto *orientationLayout = new QVBoxLayout(objectOrientationEditor_);
    orientationLayout->setContentsMargins(0, 0, 0, 0);
    orientationLayout->setSpacing(6);
    objectOrientationEnabledCheck_ = new QCheckBox(tr("Orientation override (-orientation)"), objectOrientationEditor_);
    objectOrientationSpin_ = new QDoubleSpinBox(objectOrientationEditor_);
    objectOrientationSpin_->setDecimals(3);
    objectOrientationSpin_->setRange(0.0, 359.999);
    objectOrientationSpin_->setSingleStep(1.0);
    objectOrientationSpin_->setSuffix(tr(" deg"));
    objectOrientationApplyButton_ = new QPushButton(tr("Apply Orientation"), objectOrientationEditor_);
    objectOrientationApplyButton_->setAutoDefault(false);
    connect(objectOrientationEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleObjectOrientationEnabledToggled);
    connect(objectOrientationApplyButton_, &QPushButton::clicked, this, &MapEditorTab::applyObjectOrientationEdits);
    orientationLayout->addWidget(objectOrientationEnabledCheck_);
    orientationLayout->addWidget(objectOrientationSpin_);
    orientationLayout->addWidget(objectOrientationApplyButton_);
    vertexSelectionLayout->addWidget(objectOrientationEditor_);

    vertexActionsEditor_ = new QWidget(vertexSelectionSection_);
    auto *vertexActionsLayout = new QHBoxLayout(vertexActionsEditor_);
    vertexActionsLayout->setContentsMargins(0, 0, 0, 0);
    vertexActionsLayout->setSpacing(6);
    vertexInsertButton_ = new QPushButton(tr("Insert Vertex"), vertexActionsEditor_);
    vertexDeleteButton_ = new QPushButton(tr("Delete Vertex"), vertexActionsEditor_);
    vertexToggleSmoothButton_ = new QPushButton(tr("Toggle Smooth"), vertexActionsEditor_);
    vertexInsertButton_->setAutoDefault(false);
    vertexDeleteButton_->setAutoDefault(false);
    vertexToggleSmoothButton_->setAutoDefault(false);
    connect(vertexInsertButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexFromSelectionPanel);
    connect(vertexDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteVertexFromSelectionPanel);
    connect(vertexToggleSmoothButton_, &QPushButton::clicked, this, &MapEditorTab::toggleVertexSmoothFromSelectionPanel);
    vertexActionsLayout->addWidget(vertexInsertButton_);
    vertexActionsLayout->addWidget(vertexDeleteButton_);
    vertexActionsLayout->addWidget(vertexToggleSmoothButton_);
    vertexSelectionLayout->addWidget(vertexActionsEditor_);
    selectionLayout->addWidget(vertexSelectionSection_);

    QVBoxLayout *geometrySelectionLayout = nullptr;
    geometrySelectionSection_ = createSelectionSection(tr("Geometry"), &geometrySelectionLayout);

    lineOptionsEditor_ = new QWidget(geometrySelectionSection_);
    auto *lineOptionsLayout = new QVBoxLayout(lineOptionsEditor_);
    lineOptionsLayout->setContentsMargins(0, 0, 0, 0);
    lineOptionsLayout->setSpacing(6);
    lineClosedCheck_ = new QCheckBox(tr("Closed (-close)"), lineOptionsEditor_);
    lineReversedCheck_ = new QCheckBox(tr("Reversed (-reverse)"), lineOptionsEditor_);
    connect(lineClosedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineClosedToggled);
    connect(lineReversedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineReversedToggled);
    lineOptionsLayout->addWidget(lineClosedCheck_);
    lineOptionsLayout->addWidget(lineReversedCheck_);
    geometrySelectionLayout->addWidget(lineOptionsEditor_);
    selectionLayout->addWidget(geometrySelectionSection_);

    scrapScaleEditor_ = new QFrame(objectSelectionSection_);
    static_cast<QFrame *>(scrapScaleEditor_)->setFrameShape(QFrame::StyledPanel);
    auto *scrapScaleLayout = new QVBoxLayout(scrapScaleEditor_);
    scrapScaleLayout->setContentsMargins(8, 8, 8, 8);
    scrapScaleLayout->setSpacing(6);
    auto *scrapScaleTitle = new QLabel(tr("Scrap Scale"), scrapScaleEditor_);
    QFont scrapScaleTitleFont = scrapScaleTitle->font();
    scrapScaleTitleFont.setBold(true);
    scrapScaleTitle->setFont(scrapScaleTitleFont);
    scrapScaleLayout->addWidget(scrapScaleTitle);

    auto createScrapScaleSpin = [](QWidget *parent, int decimals) {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setDecimals(decimals);
        spin->setRange(-10000000.0, 10000000.0);
        spin->setSingleStep(decimals == 0 ? 1.0 : 0.1);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setMinimumWidth(72);
        return spin;
    };
    auto createScrapScalePointBlock = [](QWidget *parent,
                                         const QString &title,
                                         QDoubleSpinBox *xSpin,
                                         QDoubleSpinBox *ySpin) {
        auto *block = new QWidget(parent);
        auto *blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(0, 0, 0, 0);
        blockLayout->setSpacing(3);

        auto *titleLabel = new QLabel(title, block);
        titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        blockLayout->addWidget(titleLabel);

        auto *row = new QWidget(block);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(new QLabel(QStringLiteral("X"), row));
        rowLayout->addWidget(xSpin, 1);
        rowLayout->addWidget(new QLabel(QStringLiteral("Y"), row));
        rowLayout->addWidget(ySpin, 1);
        blockLayout->addWidget(row);
        return block;
    };

    scrapScaleSourceX1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleSourceY1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleSourceX2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleSourceY2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleRealX1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleRealY1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleRealX2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleRealY2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Picture point 1 (px)"), scrapScaleSourceX1Spin_, scrapScaleSourceY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Picture point 2 (px)"), scrapScaleSourceX2Spin_, scrapScaleSourceY2Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Real point 1"), scrapScaleRealX1Spin_, scrapScaleRealY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Real point 2"), scrapScaleRealX2Spin_, scrapScaleRealY2Spin_));

    auto *scrapScaleUnitRow = new QWidget(scrapScaleEditor_);
    auto *scrapScaleUnitLayout = new QHBoxLayout(scrapScaleUnitRow);
    scrapScaleUnitLayout->setContentsMargins(0, 0, 0, 0);
    scrapScaleUnitLayout->setSpacing(6);
    scrapScaleUnitLayout->addWidget(new QLabel(tr("Unit"), scrapScaleUnitRow));
    scrapScaleUnitCombo_ = new QComboBox(scrapScaleEditor_);
    scrapScaleUnitCombo_->addItems({QStringLiteral("m"), QStringLiteral("cm"), QStringLiteral("mm"), QStringLiteral("ft"), QStringLiteral("in")});
    scrapScaleUnitLayout->addWidget(scrapScaleUnitCombo_, 1);
    scrapScaleLayout->addWidget(scrapScaleUnitRow);

    auto *scrapScaleButtons = new QHBoxLayout;
    scrapScaleButtons->setContentsMargins(0, 0, 0, 0);
    scrapScaleUseBoundsButton_ = new QPushButton(tr("Use Bounds"), scrapScaleEditor_);
    scrapScaleUseBoundsButton_->setAutoDefault(false);
    scrapScaleUseBoundsButton_->setToolTip(tr("Use current source bounds as the XTherion default scrap scale."));
    scrapScaleApplyButton_ = new QPushButton(tr("Apply Scale"), scrapScaleEditor_);
    scrapScaleApplyButton_->setAutoDefault(false);
    connect(scrapScaleUseBoundsButton_, &QPushButton::clicked, this, &MapEditorTab::populateScrapScaleFromSourceBounds);
    connect(scrapScaleApplyButton_, &QPushButton::clicked, this, &MapEditorTab::applyScrapScaleEdits);
    scrapScaleButtons->addWidget(scrapScaleUseBoundsButton_);
    scrapScaleButtons->addWidget(scrapScaleApplyButton_);
    scrapScaleLayout->addLayout(scrapScaleButtons);
    objectSelectionLayout->addWidget(scrapScaleEditor_);

    QVBoxLayout *advancedSelectionLayout = nullptr;
    advancedSelectionSection_ = createSelectionSection(tr("Advanced"), &advancedSelectionLayout);
    objectConfigureButton_ = new QPushButton(tr("Edit Object Settings..."), advancedSelectionSection_);
    objectConfigureButton_->setAutoDefault(false);
    connect(objectConfigureButton_, &QPushButton::clicked, this, &MapEditorTab::handleConfigureObjectSettingsTriggered);
    advancedSelectionLayout->addWidget(objectConfigureButton_);
    selectionLayout->addWidget(advancedSelectionSection_);
    selectionTabLayout->addWidget(selectionPanel);
    selectionTabLayout->addStretch(1);
    mapInspectorTabs_->addTab(selectionTab, tr("Selection"));
    mapInspectorTabs_->addTab(objectsTab, tr("Objects"));

    auto *backgroundTab = new QWidget(mapInspectorTabs_);
    auto *backgroundLayout = new QVBoxLayout(backgroundTab);
    backgroundLayout->setContentsMargins(4, 4, 4, 4);
    backgroundLayout->setSpacing(10);

    auto *gridLabel = new QLabel(tr("Grid"), backgroundTab);
    gridLabel->setFont(sectionFont);
    backgroundLayout->addWidget(gridLabel);

    auto *gridFrame = new QFrame(backgroundTab);
    gridFrame->setFrameShape(QFrame::StyledPanel);
    auto *gridLayout = new QVBoxLayout(gridFrame);
    gridLayout->setContentsMargins(12, 12, 12, 12);
    gridLayout->setSpacing(8);

    mapGridVisibleCheck_ = new QCheckBox(tr("Show grid"), gridFrame);
    mapGridVisibleCheck_->setChecked(mapGridVisible_);
    gridLayout->addWidget(mapGridVisibleCheck_);

    auto *gridSpacingRow = new QHBoxLayout;
    gridSpacingRow->addWidget(new QLabel(tr("Spacing (m)"), gridFrame));
    mapGridSpacingSpin_ = new QDoubleSpinBox(gridFrame);
    mapGridSpacingSpin_->setRange(0.1, 10000.0);
    mapGridSpacingSpin_->setDecimals(1);
    mapGridSpacingSpin_->setSingleStep(1.0);
    mapGridSpacingSpin_->setSuffix(tr(" m"));
    mapGridSpacingSpin_->setValue(mapGridSpacingMeters_);
    gridSpacingRow->addWidget(mapGridSpacingSpin_, 1);
    gridLayout->addLayout(gridSpacingRow);
    backgroundLayout->addWidget(gridFrame);

    auto *layersRow = new QHBoxLayout;
    auto *layersLabel = new QLabel(tr("Layers"), backgroundTab);
    sectionFont = layersLabel->font();
    sectionFont.setBold(true);
    layersLabel->setFont(sectionFont);
    layersRow->addWidget(layersLabel);
    layersRow->addStretch(1);
    mapBackgroundAddButton_ = new QToolButton(backgroundTab);
    mapBackgroundAddButton_->setText(QStringLiteral("+"));
    mapBackgroundAddButton_->setToolTip(tr("Add background images"));
    layersRow->addWidget(mapBackgroundAddButton_);
    backgroundLayout->addLayout(layersRow);

    mapBackgroundLayersTree_ = new QTreeView(backgroundTab);
    mapBackgroundLayersTree_->setRootIsDecorated(false);
    mapBackgroundLayersTree_->setAnimated(false);
    mapBackgroundLayersTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapBackgroundLayersTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapBackgroundLayersTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapBackgroundLayersTree_->setAlternatingRowColors(true);
    mapBackgroundLayersTree_->setHeaderHidden(true);
    mapBackgroundLayersTree_->setIconSize(QSize(16, 16));
    mapBackgroundLayersTree_->setMinimumHeight(88);
    mapBackgroundLayersModel_ = new QStandardItemModel(mapBackgroundLayersTree_);
    mapBackgroundLayersTree_->setModel(mapBackgroundLayersModel_);
    configureInspectorBackgroundLayerTreeColumns();
    connect(mapBackgroundLayersTree_, &QTreeView::clicked, this, &MapEditorTab::handleInspectorBackgroundLayerClicked);
    backgroundLayout->addWidget(mapBackgroundLayersTree_);

    auto *layerActionsRow = new QHBoxLayout;
    mapBackgroundMoveUpButton_ = new QPushButton(tr("Up"), backgroundTab);
    mapBackgroundMoveDownButton_ = new QPushButton(tr("Down"), backgroundTab);
    mapBackgroundMoveUpButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mapBackgroundMoveDownButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layerActionsRow->addWidget(mapBackgroundMoveUpButton_);
    layerActionsRow->addWidget(mapBackgroundMoveDownButton_);
    backgroundLayout->addLayout(layerActionsRow);

    auto *positionLabel = new QLabel(tr("Position"), backgroundTab);
    positionLabel->setFont(sectionFont);
    backgroundLayout->addWidget(positionLabel);

    auto *positionFrame = new QFrame(backgroundTab);
    positionFrame->setFrameShape(QFrame::StyledPanel);
    auto *positionLayout = new QVBoxLayout(positionFrame);
    positionLayout->setContentsMargins(12, 12, 12, 12);
    positionLayout->setSpacing(8);

    auto *xRow = new QHBoxLayout;
    xRow->addWidget(new QLabel(tr("X"), positionFrame));
    mapBackgroundPosXSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosXSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosXSpin_->setDecimals(1);
    xRow->addWidget(mapBackgroundPosXSpin_, 1);
    positionLayout->addLayout(xRow);

    auto *yRow = new QHBoxLayout;
    yRow->addWidget(new QLabel(tr("Y"), positionFrame));
    mapBackgroundPosYSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosYSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosYSpin_->setDecimals(1);
    yRow->addWidget(mapBackgroundPosYSpin_, 1);
    positionLayout->addLayout(yRow);

    backgroundLayout->addWidget(positionFrame);

    auto *adjustmentsLabel = new QLabel(tr("Adjustments"), backgroundTab);
    adjustmentsLabel->setFont(sectionFont);
    backgroundLayout->addWidget(adjustmentsLabel);

    auto *adjustmentsFrame = new QFrame(backgroundTab);
    adjustmentsFrame->setFrameShape(QFrame::StyledPanel);
    auto *adjustmentsLayout = new QVBoxLayout(adjustmentsFrame);
    adjustmentsLayout->setContentsMargins(12, 12, 12, 12);
    adjustmentsLayout->setSpacing(8);

    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(tr("Opacity"), adjustmentsFrame));
    opacityRow->addStretch(1);
    mapBackgroundOpacityResetButton_ = new QPushButton(tr("Reset"), adjustmentsFrame);
    opacityRow->addWidget(mapBackgroundOpacityResetButton_);
    adjustmentsLayout->addLayout(opacityRow);

    mapBackgroundOpacitySlider_ = new QSlider(Qt::Horizontal, adjustmentsFrame);
    mapBackgroundOpacitySlider_->setRange(5, 100);
    adjustmentsLayout->addWidget(mapBackgroundOpacitySlider_);

    auto *gammaRow = new QHBoxLayout;
    gammaRow->addWidget(new QLabel(tr("Gamma"), adjustmentsFrame));
    gammaRow->addStretch(1);
    mapBackgroundGammaResetButton_ = new QPushButton(tr("Reset"), adjustmentsFrame);
    gammaRow->addWidget(mapBackgroundGammaResetButton_);
    adjustmentsLayout->addLayout(gammaRow);

    mapBackgroundGammaSlider_ = new QSlider(Qt::Horizontal, adjustmentsFrame);
    mapBackgroundGammaSlider_->setRange(20, 250);
    adjustmentsLayout->addWidget(mapBackgroundGammaSlider_);
    backgroundLayout->addWidget(adjustmentsFrame);

    backgroundLayout->addStretch(1);
    mapInspectorTabs_->addTab(backgroundTab, tr("Backgrounds"));

    connect(mapBackgroundAddButton_, &QToolButton::clicked, this, [this]() {
        browseAndAddBackgroundImages();
    });
    connect(mapBackgroundMoveUpButton_, &QPushButton::clicked, this, [this]() {
        moveSelectedBackgroundLayerUp();
    });
    connect(mapBackgroundMoveDownButton_, &QPushButton::clicked, this, [this]() {
        moveSelectedBackgroundLayerDown();
    });
    if (mapBackgroundLayersTree_->selectionModel() != nullptr) {
        connect(mapBackgroundLayersTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
            handleInspectorBackgroundLayerSelectionChanged(current);
        });
    }
    connect(mapBackgroundPosXSpin_, &QDoubleSpinBox::valueChanged, this, [this](double x) {
        if (updatingMapInspectorBackgroundUi_ || mapBackgroundPosYSpin_ == nullptr) {
            return;
        }
        setSelectedBackgroundLayerPosition(QPointF(x, mapBackgroundPosYSpin_->value()));
    });
    connect(mapBackgroundPosYSpin_, &QDoubleSpinBox::valueChanged, this, [this](double y) {
        if (updatingMapInspectorBackgroundUi_ || mapBackgroundPosXSpin_ == nullptr) {
            return;
        }
        setSelectedBackgroundLayerPosition(QPointF(mapBackgroundPosXSpin_->value(), y));
    });
    connect(mapBackgroundOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updatingMapInspectorBackgroundUi_) {
            return;
        }
        setSelectedBackgroundLayerOpacity(static_cast<qreal>(value) / 100.0);
    });
    connect(mapBackgroundGammaSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updatingMapInspectorBackgroundUi_) {
            return;
        }
        setSelectedBackgroundLayerGamma(static_cast<qreal>(value) / 100.0);
    });
    connect(mapBackgroundOpacityResetButton_, &QPushButton::clicked, this, [this]() {
        resetSelectedBackgroundLayerOpacity();
    });
    connect(mapBackgroundGammaResetButton_, &QPushButton::clicked, this, [this]() {
        resetSelectedBackgroundLayerGamma();
    });
    connect(mapGridVisibleCheck_, &QCheckBox::toggled, this, &MapEditorTab::setMapGridVisible);
    connect(mapGridSpacingSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::setMapGridSpacingMeters);

    mapDetailsSplitter_->addWidget(mapView_);
    mapDetailsSplitter_->addWidget(objectDetailsPanel_);
    mapDetailsSplitter_->setStretchFactor(0, 1);
    mapDetailsSplitter_->setStretchFactor(1, 0);
    mapDetailsSplitter_->setSizes(QList<int>{980, 320});
    mapPaneLayout->addWidget(mapPaneTopSeparator_);
    mapPaneLayout->addWidget(mapDetailsSplitter_, 1);
    splitter_->addWidget(mapPaneContainer_);
    splitter_->addWidget(textEditor_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes(QList<int>{700, 900});

    layout->addWidget(workspaceModeRow_);
    layout->addWidget(splitter_, 1);

    refreshMapScene();
    refreshWorkspaceModeUi();
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshObjectDetailsPanel();
    updateCommandSurfaceState();
}

bool MapEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    const QString currentFilePath = this->filePath();
    if (!currentFilePath.isEmpty() && QFileInfo(currentFilePath).canonicalFilePath() != QFileInfo(filePath).canonicalFilePath()) {
        clearDraftGeometryItems();
        clearBackgroundImageItems();
    }

    const bool loaded = textEditor_->loadFile(filePath, errorMessage);
    if (!loaded) {
        return false;
    }

    refreshMapScene();
    loadBackgroundLayersFromSession();
    loadBackgroundLayersFromDocumentMetadata();
    fitMapToView();
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshTitle();
    refreshStatus();
    return true;
}

bool MapEditorTab::save(QString *errorMessage)
{
    return textEditor_->save(errorMessage);
}

bool MapEditorTab::rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage)
{
    const bool rewritten = textEditor_->rewriteStructureEntryName(lineNumber, category, newName, errorMessage);
    if (rewritten) {
        refreshMapScene();
        refreshTitle();
        refreshStatus();
    }

    return rewritten;
}

bool MapEditorTab::rewriteLineOptionToggle(int lineNumber,
                                           const QString &optionName,
                                           bool enabled,
                                           QString *errorMessage)
{
    const bool rewritten = textEditor_->rewriteLineOptionToggle(lineNumber, optionName, enabled, errorMessage);
    if (rewritten) {
        refreshMapScene();
        refreshTitle();
        refreshStatus();
    }

    return rewritten;
}

void MapEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    textEditor_->setProjectRootPath(projectRootPath);
    refreshStatus();
}

void MapEditorTab::showFindBar(bool replaceMode)
{
    textEditor_->showFindBar(replaceMode);
}

void MapEditorTab::hideFindBar()
{
    textEditor_->hideFindBar();
}

void MapEditorTab::goToLine(int lineNumber)
{
    textEditor_->goToLine(lineNumber);
}

QString MapEditorTab::filePath() const
{
    return textEditor_->filePath();
}

QString MapEditorTab::displayName() const
{
    return textEditor_->displayName();
}

bool MapEditorTab::isDirty() const
{
    return textEditor_->isDirty();
}

int MapEditorTab::currentLineNumber() const
{
    return textEditor_->currentLineNumber();
}

QString MapEditorTab::text() const
{
    return textEditor_ != nullptr ? textEditor_->text() : QString();
}

QString MapEditorTab::statusPathText() const
{
    return textEditor_ != nullptr ? textEditor_->statusPathText() : tr("No file open");
}

QString MapEditorTab::statusEncodingText() const
{
    return textEditor_ != nullptr ? textEditor_->statusEncodingText() : tr("UTF-8");
}

QString MapEditorTab::statusModeText() const
{
    return interactiveDrawMode_ == InteractiveDrawMode::None
        ? tr("Map mode: Select")
        : tr("Map mode: Insert");
}

int MapEditorTab::zoomPercent() const
{
    return qRound(zoomFactor_ * 100.0);
}

bool MapEditorTab::canUndo() const
{
    return (undoStack_ != nullptr && undoStack_->canUndo())
        || (textEditor_ != nullptr && textEditor_->canUndo());
}

bool MapEditorTab::canRedo() const
{
    return (undoStack_ != nullptr && undoStack_->canRedo())
        || (textEditor_ != nullptr && textEditor_->canRedo());
}

MapEditorTab::InteractiveDrawMode MapEditorTab::interactiveDrawMode() const
{
    return interactiveDrawMode_;
}

bool MapEditorTab::canCompleteDraftAction() const
{
    const bool mapReady = mapScene_ != nullptr;
    return mapReady && (selectedDraftGeometryItem() != nullptr || hasCompletableInteractiveDrawSession());
}

bool MapEditorTab::isTouchFriendlyControlsEnabled() const
{
    return touchFriendlyControlsEnabled_;
}

void MapEditorTab::triggerUndo()
{
    handleUndoTriggered();
}

void MapEditorTab::triggerRedo()
{
    handleRedoTriggered();
}

void MapEditorTab::triggerZoomIn()
{
    handleZoomInTriggered();
}

void MapEditorTab::triggerZoomOut()
{
    handleZoomOutTriggered();
}

void MapEditorTab::triggerFit()
{
    handleFitTriggered();
}

void MapEditorTab::triggerFitWithBackground()
{
    handleFitWithBackgroundTriggered();
}

void MapEditorTab::triggerSelectMode()
{
    handleSelectModeTriggered();
}

void MapEditorTab::triggerInsertScrap()
{
    handleInsertScrapTriggered();
}

void MapEditorTab::triggerCompleteDraft()
{
    handleCompleteDraftTriggered();
}

void MapEditorTab::triggerAddPoint()
{
    handleAddPointTriggered();
}

void MapEditorTab::triggerAddLine()
{
    handleAddLineTriggered();
}

void MapEditorTab::triggerAddFreehandLine()
{
    handleAddFreehandLineTriggered();
}

void MapEditorTab::triggerAddSmartTraceLine()
{
    handleAddSmartTraceLineTriggered();
}

void MapEditorTab::triggerAddArea()
{
    handleAddAreaTriggered();
}

void MapEditorTab::setTouchControlsEnabled(bool enabled)
{
    handleTouchFriendlyControlsToggled(enabled);
}

bool MapEditorTab::isInsertModeActive() const
{
    return interactiveDrawMode_ != InteractiveDrawMode::None;
}

bool MapEditorTab::isMapPaneDetached() const
{
    return mapPaneDetached_;
}

QString MapEditorTab::mapPaneWindowActionText() const
{
    return mapPaneDetached_ ? tr("Return Map") : tr("Separate Map");
}

QString MapEditorTab::mapPaneWindowActionToolTip() const
{
    return mapPaneDetached_
        ? tr("Return the map pane from the detached window into this tab.")
        : tr("Open the map pane in a separate window (for multi-monitor workflows).");
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceMode() const
{
    return workspaceMode_;
}

void MapEditorTab::setInlineWorkspaceModeSelectorVisible(bool visible)
{
    inlineWorkspaceModeSelectorVisible_ = visible;
    if (workspaceModeRow_ != nullptr) {
        workspaceModeRow_->setVisible(inlineWorkspaceModeSelectorVisible_);
        workspaceModeRow_->setMaximumHeight(inlineWorkspaceModeSelectorVisible_ ? QWIDGETSIZE_MAX : 0);
        if (!inlineWorkspaceModeSelectorVisible_) {
            workspaceModeRow_->setMinimumHeight(0);
        } else {
            workspaceModeRow_->setMinimumHeight(workspaceModeRow_->sizeHint().height());
        }
    }
    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        rootLayout->invalidate();
        rootLayout->activate();
    }
}

void MapEditorTab::setWorkspaceMode(WorkspaceMode mode)
{
    if (workspaceMode_ == mode) {
        refreshWorkspaceModeUi();
        return;
    }

    workspaceMode_ = mode;
    refreshWorkspaceModeUi();
    updateWorkspaceVisibility();
    emit workspaceModeChanged(workspaceMode_);
}

void MapEditorTab::refreshWorkspaceModeUi()
{
    if (visualModeButton_ == nullptr || rawModeButton_ == nullptr) {
        return;
    }

    visualModeButton_->setChecked(workspaceMode_ == WorkspaceMode::Visual);
    rawModeButton_->setChecked(workspaceMode_ == WorkspaceMode::Raw);
}

void MapEditorTab::handleTextEditorCurrentLineChanged(int lineNumber)
{
    if (!mapSelectionDrivenTextNavigationInProgress_) {
        syncMapSelectionFromTextCursor(lineNumber, textEditor_ != nullptr ? textEditor_->currentColumnNumber() : 1);
    }
    syncInspectorObjectSelectionToLine(lineNumber);
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleTextEditorCursorPositionChanged(int lineNumber, int columnNumber)
{
    if (mapSelectionDrivenTextNavigationInProgress_) {
        return;
    }

    if (lineNumber == lastCursorSyncedLine_ && columnNumber == lastCursorSyncedColumn_) {
        return;
    }

    syncMapSelectionFromTextCursor(lineNumber, columnNumber);
}

void MapEditorTab::handleUndoTriggered()
{
    if (undoStack_ != nullptr && undoStack_->canUndo()) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->undo();
        flushPendingMapSceneRefreshAfterCommand();
        return;
    }
    if (textEditor_ != nullptr && textEditor_->canUndo()) {
        textEditor_->triggerUndo();
    }
}

void MapEditorTab::handleRedoTriggered()
{
    if (undoStack_ != nullptr && undoStack_->canRedo()) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->redo();
        flushPendingMapSceneRefreshAfterCommand();
        return;
    }
    if (textEditor_ != nullptr && textEditor_->canRedo()) {
        textEditor_->triggerRedo();
    }
}

void MapEditorTab::handleZoomInTriggered()
{
    adjustMapZoom(1.2);
}

void MapEditorTab::handleZoomOutTriggered()
{
    adjustMapZoom(1.0 / 1.2);
}

void MapEditorTab::handleFitTriggered()
{
    fitBackgroundRequested_ = false;
    toolbarStatusNote_ = tr("Fit geometry: centered visible map content.");
    fitMapToView();
    refreshToolbarSummary();
}

void MapEditorTab::handleTouchFriendlyControlsToggled(bool checked)
{
    setTouchFriendlyControlsEnabled(checked);
}

void MapEditorTab::updateWorkspaceVisibility()
{
    if (textEditor_ == nullptr) {
        refreshStatus();
        return;
    }

    if (mapPaneDetached_) {
        if (workspaceModeRow_ != nullptr) {
            workspaceModeRow_->setEnabled(false);
            workspaceModeRow_->setToolTip(tr("Map pane is detached: raw editor remains in this tab while visual map stays in the detached window."));
        }
        textEditor_->show();
        if (mapPaneContainer_ != nullptr) {
            // Detached window always hosts the visual map pane, even if main-tab mode is Raw.
            mapPaneContainer_->show();
        }
        if (mapPaneTopSeparator_ != nullptr) {
            mapPaneTopSeparator_->hide();
        }
        refreshStatus();
        return;
    }

    if (workspaceModeRow_ != nullptr) {
        workspaceModeRow_->setEnabled(true);
        workspaceModeRow_->setToolTip(QString());
    }

    const bool visualMode = workspaceMode_ == WorkspaceMode::Visual;
    textEditor_->setVisible(!visualMode);
    if (mapPaneContainer_ != nullptr) {
        mapPaneContainer_->setVisible(visualMode);
    }
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->setVisible(visualMode);
    }

    if (splitter_ != nullptr) {
        if (visualMode) {
            splitter_->setSizes(QList<int>{1, 0});
        } else {
            splitter_->setSizes(QList<int>{0, 1});
        }
    }

    refreshStatus();
}

void MapEditorTab::refreshTitle()
{
    emit titleChanged();
}

void MapEditorTab::refreshStatus()
{
    if (detachedMapPaneWindow_ != nullptr) {
        detachedMapPaneWindow_->setWindowTitle(tr("%1 — Map").arg(displayName()));
        detachedMapPaneWindow_->setWindowFilePath(filePath());
        if (auto *window = dynamic_cast<DetachedMapPaneWindow *>(detachedMapPaneWindow_.data())) {
            window->setMapStatus(zoomPercent(), isInsertModeActive(), statusModeText());
        }
    }
}

QString MapEditorTab::displayPath() const
{
    return textEditor_->filePath();
}

void MapEditorTab::toggleMapPaneWindow()
{
    if (mapPaneDetached_) {
        if (detachedMapPaneWindow_ != nullptr) {
            detachedMapPaneWindow_->close();
            return;
        }

        reattachMapPaneFromWindow();
        return;
    }

    detachMapPaneToWindow();
}

void MapEditorTab::detachMapPaneToWindow()
{
    if (mapPaneDetached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr) {
        return;
    }

    mapPaneContainer_->setParent(nullptr);

    auto *window = new DetachedMapPaneWindow(this, this);
    window->setWindowTitle(tr("%1 — Map").arg(displayName()));
    window->setWindowFilePath(filePath());
    window->setMapPaneWidget(mapPaneContainer_);
    window->setCloseCallback([this]() { reattachMapPaneFromWindow(); });

    detachedMapPaneWindow_ = window;
    mapPaneDetached_ = true;
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->hide();
    }
    emit mapPaneDetachStateChanged(true);
    updateWorkspaceVisibility();

    const QSize mapSize = mapView_ != nullptr ? mapView_->size() : QSize();
    window->resize(mapSize.isValid() ? mapSize.expandedTo(QSize(900, 700)) : QSize(1200, 800));
    window->show();
    window->raise();
    window->activateWindow();
}

void MapEditorTab::reattachMapPaneFromWindow()
{
    if (!mapPaneDetached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr || reattachingMapPane_) {
        return;
    }

    reattachingMapPane_ = true;

    mapPaneContainer_->setParent(splitter_);
    splitter_->insertWidget(0, mapPaneContainer_);
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->setVisible(workspaceMode_ == WorkspaceMode::Visual);
    }

    mapPaneDetached_ = false;
    detachedMapPaneWindow_ = nullptr;
    emit mapPaneDetachStateChanged(false);
    updateWorkspaceVisibility();
    if (mapView_ != nullptr) {
        mapView_->setFocus(Qt::OtherFocusReason);
    }

    reattachingMapPane_ = false;
}

void MapEditorTab::focusDetachedMapPaneWindow()
{
    if (detachedMapPaneWindow_ == nullptr) {
        return;
    }

    detachedMapPaneWindow_->showNormal();
    detachedMapPaneWindow_->raise();
    detachedMapPaneWindow_->activateWindow();
}

void MapEditorTab::setTouchFriendlyControlsEnabled(bool enabled)
{
    touchFriendlyControlsEnabled_ = enabled;
    SessionStore::setTherionMapTouchFriendlyControlsEnabled(enabled);
    updateCommandSurfaceState();
}
}
