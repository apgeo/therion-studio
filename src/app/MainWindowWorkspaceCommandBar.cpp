#include "MainWindow.h"

#include "LucideIconFactory.h"
#include "MainWindowDocumentHelpers.h"
#include "WorkspaceCommandBarStyle.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QList>
#include <QMenu>
#include <QPalette>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>

namespace
{
QToolButton *createWorkspaceIconButton(QWidget *parent,
                                       const QString &toolTip,
                                       const QString &iconName)
{
    auto *button = new QToolButton(parent);
    button->setAutoRaise(false);
    button->setIconSize(QSize(14, 14));
    button->setIcon(TherionStudio::themedLucideIcon(iconName, button->palette(), 14, button->devicePixelRatioF()));
    button->setProperty("lucideIconName", iconName);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setFixedSize(QSize(26, 26));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QFrame *createWorkspaceToolbarSeparator(QWidget *parent)
{
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName(QStringLiteral("workspaceToolbarSeparator"));
    return separator;
}
}

void MainWindow::initializeWorkspaceModeSwitcher()
{
    if (editorTabs_ != nullptr) {
        editorTabs_->setObjectName(QStringLiteral("mainEditorTabs"));
        if (!editorTabs_->property("baseStyleSheet").isValid()) {
            editorTabs_->setProperty("baseStyleSheet", editorTabs_->styleSheet());
        }
    }

    QWidget *workspaceHost = editorAreaColumn_ != nullptr ? editorAreaColumn_ : editorAreaHost_;
    if (workspaceHost == nullptr) {
        workspaceHost = centralWidget();
    }
    if (workspaceHost == nullptr) {
        workspaceHost = editorTabs_;
    }
    workspaceModeSwitcher_ = new QWidget(workspaceHost);
    workspaceModeSwitcher_->setObjectName(QStringLiteral("workspaceCommandBar"));
    workspaceModeSwitcher_->setAttribute(Qt::WA_StyledBackground, true);
    workspaceModeSwitcher_->setStyleSheet(TherionStudio::workspaceCommandBarStyleSheet(palette().color(QPalette::Base),
                                                                                        false,
                                                                                        false));
    auto *hostLayout = new QHBoxLayout(workspaceModeSwitcher_);
    hostLayout->setContentsMargins(4, 4, 8, 4);
    hostLayout->setSpacing(4);

    workspaceOpenProjectButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Open Project"), QStringLiteral("folder-open"));
    workspaceNewDocumentButton_ =
        createWorkspaceIconButton(workspaceModeSwitcher_, tr("New Document"), QStringLiteral("file-plus"));
    workspaceCloseProjectButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Close Project"), QStringLiteral("folder-x"));
    auto *newDocumentMenu = new QMenu(workspaceNewDocumentButton_);
    if (newTherionSourceAction_ != nullptr) {
        newDocumentMenu->addAction(newTherionSourceAction_);
    }
    if (newTherionMapAction_ != nullptr) {
        newDocumentMenu->addAction(newTherionMapAction_);
    }
    if (newTherionConfigAction_ != nullptr) {
        newDocumentMenu->addAction(newTherionConfigAction_);
    }
    connect(workspaceNewDocumentButton_, &QToolButton::clicked, workspaceNewDocumentButton_, [newDocumentMenu, button = workspaceNewDocumentButton_]() {
        if (newDocumentMenu == nullptr || button == nullptr) {
            return;
        }
        newDocumentMenu->popup(button->mapToGlobal(QPoint(0, button->height())));
    });
    workspaceProjectSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    workspaceSaveButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Save"), QStringLiteral("save"));
    workspaceUndoButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Undo"), QStringLiteral("undo-2"));
    workspaceRedoButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Redo"), QStringLiteral("redo-2"));
    workspaceCompileCurrentConfigButton_ =
        createWorkspaceIconButton(workspaceModeSwitcher_, tr("Compile Current Config"), QStringLiteral("play"));
    hostLayout->addWidget(workspaceOpenProjectButton_);
    hostLayout->addWidget(workspaceCloseProjectButton_);
    hostLayout->addWidget(workspaceProjectSeparator_);
    hostLayout->addWidget(workspaceNewDocumentButton_);
    hostLayout->addWidget(workspaceSaveButton_);
    hostLayout->addWidget(createWorkspaceToolbarSeparator(workspaceModeSwitcher_));
    hostLayout->addWidget(workspaceUndoButton_);
    hostLayout->addWidget(workspaceRedoButton_);
    workspaceCompileSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceCompileSeparator_);
    hostLayout->addWidget(workspaceCompileCurrentConfigButton_);
    workspaceHistorySeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceHistorySeparator_);
    workspaceZoomGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *zoomLayout = new QHBoxLayout(workspaceZoomGroup_);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->setSpacing(4);
    workspaceZoomInButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Zoom In"), QStringLiteral("zoom-in"));
    workspaceZoomOutButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Zoom Out"), QStringLiteral("zoom-out"));
    workspaceFitButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Fit"), QStringLiteral("scan"));
    workspaceFitBackgroundButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Fit With Background"), QStringLiteral("image"));
    zoomLayout->addWidget(workspaceZoomInButton_);
    zoomLayout->addWidget(workspaceZoomOutButton_);
    zoomLayout->addWidget(workspaceFitButton_);
    zoomLayout->addWidget(workspaceFitBackgroundButton_);
    hostLayout->addWidget(workspaceZoomGroup_);
    workspaceZoomSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceZoomSeparator_);
    workspaceMapToolsGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *mapToolsLayout = new QHBoxLayout(workspaceMapToolsGroup_);
    mapToolsLayout->setContentsMargins(0, 0, 0, 0);
    mapToolsLayout->setSpacing(4);
    workspaceSelectButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Select"), QStringLiteral("mouse-pointer-2"));
    workspaceCompleteDraftButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Complete Draft"), QStringLiteral("check"));
    workspaceInsertScrapButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Insert Scrap"), QStringLiteral("puzzle"));
    workspacePointButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Point"), QStringLiteral("locate-fixed"));
    workspaceLineButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Line"), QStringLiteral("spline"));
    workspaceFreehandLineButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Freehand"), QStringLiteral("pencil-line"));
    workspaceAreaButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Area"), QStringLiteral("pentagon"));
    workspaceSelectButton_->setCheckable(true);
    workspacePointButton_->setCheckable(true);
    workspaceLineButton_->setCheckable(true);
    workspaceFreehandLineButton_->setCheckable(true);
    workspaceAreaButton_->setCheckable(true);
    mapToolsLayout->addWidget(workspaceSelectButton_);
    mapToolsLayout->addWidget(workspaceCompleteDraftButton_);
    mapToolsLayout->addWidget(createWorkspaceToolbarSeparator(workspaceMapToolsGroup_));
    mapToolsLayout->addWidget(workspaceInsertScrapButton_);
    mapToolsLayout->addWidget(workspacePointButton_);
    mapToolsLayout->addWidget(workspaceLineButton_);
    mapToolsLayout->addWidget(workspaceFreehandLineButton_);
    mapToolsLayout->addWidget(workspaceAreaButton_);
    hostLayout->addWidget(workspaceMapToolsGroup_);
    hostLayout->addStretch(1);

    workspaceMapModeSwitcher_ = new QWidget(workspaceModeSwitcher_);
    auto *mapLayout = new QHBoxLayout(workspaceMapModeSwitcher_);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(4);
    workspaceVisualModeButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Visual"), QStringLiteral("pen-tool"));
    workspaceRawModeButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Raw"), QStringLiteral("code"));
    workspaceVisualModeButton_->setCheckable(true);
    workspaceRawModeButton_->setCheckable(true);
    workspaceMapPaneWindowButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Separate Map"), QStringLiteral("screen-share"));
    workspaceMapPaneWindowButton_->setObjectName(QStringLiteral("workspaceMapPaneWindowButton"));
    mapLayout->addWidget(workspaceRawModeButton_);
    mapLayout->addWidget(workspaceVisualModeButton_);
    mapLayout->addWidget(workspaceMapPaneWindowButton_);

    workspaceTextModeSwitcher_ = new QWidget(workspaceModeSwitcher_);
    auto *textLayout = new QHBoxLayout(workspaceTextModeSwitcher_);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    workspaceTextRawModeButton_ = createWorkspaceIconButton(workspaceTextModeSwitcher_, tr("Raw"), QStringLiteral("code"));
    workspaceBlocksModeButton_ = createWorkspaceIconButton(workspaceTextModeSwitcher_, tr("Blocks"), QStringLiteral("toy-brick"));
    workspaceTextRawModeButton_->setCheckable(true);
    workspaceBlocksModeButton_->setCheckable(true);
    workspaceBlocksModeButton_->setToolTip(tr("Structured block canvas for .th and Therion config files."));
    workspaceBlocksModeButton_->setAccessibleName(tr("Blocks"));
    textLayout->addWidget(workspaceTextRawModeButton_);
    textLayout->addWidget(workspaceBlocksModeButton_);

    hostLayout->addWidget(workspaceMapModeSwitcher_);
    hostLayout->addWidget(workspaceTextModeSwitcher_);

    connect(workspaceOpenProjectButton_, &QToolButton::clicked, this, [this]() {
        if (openProjectAction_ != nullptr) {
            openProjectAction_->trigger();
        } else {
            openProject();
        }
    });
    connect(workspaceCloseProjectButton_, &QToolButton::clicked, this, [this]() {
        if (closeProjectAction_ != nullptr) {
            closeProjectAction_->trigger();
        } else {
            closeProject();
        }
    });
    connect(workspaceSaveButton_, &QToolButton::clicked, this, &MainWindow::saveActiveDocument);
    connect(workspaceUndoButton_, &QToolButton::clicked, this, &MainWindow::triggerUndoForActiveDocument);
    connect(workspaceRedoButton_, &QToolButton::clicked, this, &MainWindow::triggerRedoForActiveDocument);
    connect(workspaceCompileCurrentConfigButton_, &QToolButton::clicked, this, &MainWindow::triggerCompileCurrentConfigForActiveDocument);
    connect(workspaceZoomInButton_, &QToolButton::clicked, this, &MainWindow::triggerZoomInForActiveDocument);
    connect(workspaceZoomOutButton_, &QToolButton::clicked, this, &MainWindow::triggerZoomOutForActiveDocument);
    connect(workspaceFitButton_, &QToolButton::clicked, this, &MainWindow::triggerFitForActiveDocument);
    connect(workspaceFitBackgroundButton_, &QToolButton::clicked, this, &MainWindow::triggerFitWithBackgroundForActiveDocument);
    connect(workspaceSelectButton_, &QToolButton::clicked, this, &MainWindow::triggerSelectForActiveDocument);
    connect(workspaceCompleteDraftButton_, &QToolButton::clicked, this, &MainWindow::triggerCompleteDraftForActiveDocument);
    connect(workspaceInsertScrapButton_, &QToolButton::clicked, this, &MainWindow::triggerInsertScrapForActiveDocument);
    connect(workspacePointButton_, &QToolButton::clicked, this, &MainWindow::triggerPointForActiveDocument);
    connect(workspaceLineButton_, &QToolButton::clicked, this, &MainWindow::triggerLineForActiveDocument);
    connect(workspaceFreehandLineButton_, &QToolButton::clicked, this, &MainWindow::triggerFreehandLineForActiveDocument);
    connect(workspaceAreaButton_, &QToolButton::clicked, this, &MainWindow::triggerAreaForActiveDocument);
    connect(workspaceVisualModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        }
    });
    connect(workspaceRawModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        }
    });
    connect(workspaceMapPaneWindowButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->toggleMapPaneWindow();
        }
    });
    connect(workspaceTextRawModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *textTab = currentTextTab(); textTab != nullptr) {
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
        }
    });
    connect(workspaceBlocksModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *textTab = currentTextTab(); textTab != nullptr) {
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
        }
    });

    workspaceModeSwitcher_->raise();
    workspaceMapModeSwitcher_->setVisible(false);
    workspaceTextModeSwitcher_->setVisible(false);
    workspaceZoomGroup_->setVisible(false);
    workspaceMapToolsGroup_->setVisible(false);
    workspaceHistorySeparator_->setVisible(false);
    workspaceCompileSeparator_->setVisible(false);
    workspaceCompileCurrentConfigButton_->setVisible(false);
    workspaceZoomSeparator_->setVisible(false);
    workspaceModeSwitcher_->setVisible(true);
    if (editorAreaLayout_ != nullptr) {
        editorAreaLayout_->insertWidget(0, workspaceModeSwitcher_);
    }
}

void MainWindow::refreshWorkspaceModeSwitcher()
{
    if (workspaceModeSwitcher_ == nullptr
        || workspaceMapModeSwitcher_ == nullptr
        || workspaceTextModeSwitcher_ == nullptr
        || workspaceOpenProjectButton_ == nullptr
        || workspaceNewDocumentButton_ == nullptr
        || workspaceCloseProjectButton_ == nullptr
        || workspaceProjectSeparator_ == nullptr
        || workspaceVisualModeButton_ == nullptr
        || workspaceRawModeButton_ == nullptr
        || workspaceMapPaneWindowButton_ == nullptr
        || workspaceSaveButton_ == nullptr
        || workspaceUndoButton_ == nullptr
        || workspaceRedoButton_ == nullptr
        || workspaceCompileCurrentConfigButton_ == nullptr
        || workspaceZoomGroup_ == nullptr
        || workspaceZoomInButton_ == nullptr
        || workspaceZoomOutButton_ == nullptr
        || workspaceFitButton_ == nullptr
        || workspaceFitBackgroundButton_ == nullptr
        || workspaceMapToolsGroup_ == nullptr
        || workspaceSelectButton_ == nullptr
        || workspaceCompleteDraftButton_ == nullptr
        || workspaceInsertScrapButton_ == nullptr
        || workspacePointButton_ == nullptr
        || workspaceLineButton_ == nullptr
        || workspaceFreehandLineButton_ == nullptr
        || workspaceAreaButton_ == nullptr
        || workspaceHistorySeparator_ == nullptr
        || workspaceCompileSeparator_ == nullptr
        || workspaceZoomSeparator_ == nullptr
        || workspaceTextRawModeButton_ == nullptr
        || workspaceBlocksModeButton_ == nullptr) {
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    refreshWorkspaceIconTheme();
    auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget);
    auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget);
    const bool showMapModes = mapTab != nullptr;
    const bool showTextModes = textTab != nullptr;
    const bool showCompileCurrentConfig = showTextModes && !currentDocumentTherionConfigPath().isEmpty();
    const bool mapPaneDetached = mapTab != nullptr && mapTab->isMapPaneDetached();
    const bool showZoomTools = showMapModes && !mapPaneDetached;
    const bool showMapTools = showMapModes && !mapPaneDetached;
    QColor commandBarBackground = palette().color(QPalette::Base);
    if (showTextModes && textTab != nullptr) {
        const QColor sourceSurface = textTab->sourceSurfaceColor();
        if (sourceSurface.isValid()) {
            commandBarBackground = sourceSurface;
        }
    }
    workspaceModeSwitcher_->setStyleSheet(TherionStudio::workspaceCommandBarStyleSheet(commandBarBackground,
                                                                                        false,
                                                                                        false));

    workspaceModeSwitcher_->setVisible(true);
    workspaceMapModeSwitcher_->setVisible(showMapModes);
    workspaceTextModeSwitcher_->setVisible(showTextModes);
    workspaceOpenProjectButton_->setVisible(true);
    workspaceNewDocumentButton_->setVisible(true);
    workspaceCloseProjectButton_->setVisible(true);
    workspaceProjectSeparator_->setVisible(true);
    workspaceHistorySeparator_->setVisible(showZoomTools);
    workspaceZoomGroup_->setVisible(showZoomTools);
    workspaceZoomSeparator_->setVisible(showZoomTools);
    workspaceMapToolsGroup_->setVisible(showMapTools);
    workspaceSaveButton_->setEnabled(tabWidget != nullptr);
    const bool canUndo = documentCanUndoForWidget(tabWidget);
    const bool canRedo = documentCanRedoForWidget(tabWidget);
    workspaceUndoButton_->setEnabled(canUndo);
    workspaceRedoButton_->setEnabled(canRedo);
    if (undoAction_ != nullptr) {
        undoAction_->setEnabled(canUndo);
    }
    if (redoAction_ != nullptr) {
        redoAction_->setEnabled(canRedo);
    }
    workspaceCompileSeparator_->setVisible(showCompileCurrentConfig);
    workspaceCompileCurrentConfigButton_->setVisible(showCompileCurrentConfig);
    workspaceCompileCurrentConfigButton_->setEnabled(showCompileCurrentConfig);
    workspaceZoomInButton_->setEnabled(showZoomTools);
    workspaceZoomOutButton_->setEnabled(showZoomTools);
    workspaceFitButton_->setEnabled(showZoomTools);
    workspaceFitBackgroundButton_->setEnabled(showZoomTools && mapTab != nullptr && mapTab->backgroundLayerCount() > 0);
    workspaceSelectButton_->setEnabled(showMapTools);
    workspaceCompleteDraftButton_->setEnabled(showMapTools && mapTab != nullptr && mapTab->canCompleteDraftAction());
    workspaceInsertScrapButton_->setEnabled(showMapTools);
    workspacePointButton_->setEnabled(showMapTools);
    workspaceLineButton_->setEnabled(showMapTools);
    workspaceFreehandLineButton_->setEnabled(showMapTools);
    workspaceAreaButton_->setEnabled(showMapTools);

    workspaceModeSwitcherSyncInProgress_ = true;
    if (showMapModes) {
        const QSignalBlocker selectBlocker(workspaceSelectButton_);
        const QSignalBlocker pointBlocker(workspacePointButton_);
        const QSignalBlocker lineBlocker(workspaceLineButton_);
        const QSignalBlocker freehandBlocker(workspaceFreehandLineButton_);
        const QSignalBlocker areaBlocker(workspaceAreaButton_);
        const QSignalBlocker visualBlocker(workspaceVisualModeButton_);
        const QSignalBlocker rawBlocker(workspaceRawModeButton_);
        const QSignalBlocker mapWindowBlocker(workspaceMapPaneWindowButton_);
        const TherionStudio::MapEditorTab::InteractiveDrawMode drawMode = mapTab->interactiveDrawMode();
        workspaceSelectButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::None);
        workspacePointButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Point);
        workspaceLineButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Line);
        workspaceFreehandLineButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Freehand);
        workspaceAreaButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Area);
        workspaceVisualModeButton_->setChecked(mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        workspaceRawModeButton_->setChecked(mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        workspaceVisualModeButton_->setEnabled(!mapPaneDetached);
        workspaceRawModeButton_->setEnabled(!mapPaneDetached);
        workspaceMapPaneWindowButton_->setEnabled(true);
        workspaceMapPaneWindowButton_->setProperty("lucideIconName",
                                                   mapPaneDetached
                                                       ? QStringLiteral("monitor-x")
                                                       : QStringLiteral("screen-share"));
        refreshWorkspaceIconTheme();
        workspaceMapPaneWindowButton_->setToolTip(mapTab->mapPaneWindowActionToolTip());
        workspaceMapPaneWindowButton_->setAccessibleName(mapTab->mapPaneWindowActionText());
        workspaceModeSwitcher_->setToolTip(mapPaneDetached
                                               ? tr("Map pane is detached: raw editor remains in this tab while visual map stays in the detached window.")
                                               : QString());
    } else if (showTextModes) {
        const QSignalBlocker rawTextBlocker(workspaceTextRawModeButton_);
        const QSignalBlocker blocksBlocker(workspaceBlocksModeButton_);
        const bool blocksActive = textTab->editorMode() == TherionStudio::TextEditorTab::EditorMode::Blocks;
        workspaceTextRawModeButton_->setChecked(!blocksActive);
        workspaceBlocksModeButton_->setChecked(blocksActive);
        workspaceTextRawModeButton_->setEnabled(true);
        workspaceBlocksModeButton_->setEnabled(textTab->isBlocksModeAvailable());
        workspaceModeSwitcher_->setToolTip(QString());
    } else {
        workspaceModeSwitcher_->setToolTip(QString());
    }
    workspaceModeSwitcherSyncInProgress_ = false;
    refreshWorkspaceModeSwitcherGeometry();
}

void MainWindow::refreshWorkspaceIconTheme()
{
    if (workspaceModeSwitcher_ == nullptr) {
        return;
    }

    const QPalette palette = workspaceModeSwitcher_->palette();
    const qreal devicePixelRatio = workspaceModeSwitcher_->devicePixelRatioF();
    const QList<QToolButton *> buttons = workspaceModeSwitcher_->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button == nullptr) {
            continue;
        }

        const QString iconName = button->property("lucideIconName").toString();
        if (iconName.isEmpty()) {
            continue;
        }

        button->setIcon(TherionStudio::themedLucideIcon(iconName, palette, button->iconSize().width(), devicePixelRatio));
    }
}

void MainWindow::refreshWorkspaceModeSwitcherGeometry()
{
    if (workspaceModeSwitcher_ == nullptr
        || workspaceModeSwitcher_->parentWidget() == nullptr
        || editorTabs_ == nullptr
        || editorTabs_->tabBar() == nullptr) {
        return;
    }

    workspaceModeSwitcher_->updateGeometry();
    if (sidebarCollapsed_) {
        scheduleSidebarCollapseLayoutSync();
    }
}

void MainWindow::triggerUndoForActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    if (documentUndoForWidget(tabWidget)) {
        refreshWorkspaceModeSwitcher();
    }
}

void MainWindow::triggerRedoForActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    if (documentRedoForWidget(tabWidget)) {
        refreshWorkspaceModeSwitcher();
    }
}

void MainWindow::triggerRawModeForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        if (!mapTab->isMapPaneDetached()) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        }
        return;
    }

    if (auto *textTab = currentTextTab(); textTab != nullptr) {
        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
    }
}

void MainWindow::triggerSecondaryEditorModeForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        if (!mapTab->isMapPaneDetached()) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        }
        return;
    }

    if (auto *textTab = currentTextTab(); textTab != nullptr && textTab->isBlocksModeAvailable()) {
        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
    }
}

void MainWindow::triggerCompileCurrentConfigForActiveDocument()
{
    runTherionCurrentConfig();
}

void MainWindow::triggerZoomInForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerZoomIn();
    }
}

void MainWindow::triggerZoomOutForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerZoomOut();
    }
}

void MainWindow::triggerFitForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerFit();
    }
}

void MainWindow::triggerFitWithBackgroundForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerFitWithBackground();
    }
}

void MainWindow::triggerSelectForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerSelectMode();
    }
}

void MainWindow::triggerCompleteDraftForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerCompleteDraft();
    }
}

void MainWindow::triggerInsertScrapForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerInsertScrap();
    }
}

void MainWindow::triggerPointForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddPoint();
    }
}

void MainWindow::triggerLineForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddLine();
    }
}

void MainWindow::triggerFreehandLineForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddFreehandLine();
    }
}

void MainWindow::triggerAreaForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddArea();
    }
}
