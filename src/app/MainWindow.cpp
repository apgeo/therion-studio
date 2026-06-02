#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QFont>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QHash>
#include <QGuiApplication>
#include <QScreen>
#include <QSizePolicy>
#include <QSplitter>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QKeyEvent>
#include <QKeySequence>
#include <QVariant>
#include <QWidget>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <functional>
#include <utility>
#include <vector>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "MainWindowDocumentHelpers.h"
#include "MainWindowDocumentController.h"
#include "MainWindowDocumentOpenController.h"
#include "MainWindowDocumentTabOpenController.h"
#include "MainWindowHelpDialog.h"
#include "MainWindowSettingsDialog.h"
#include "MainWindowSessionDocumentController.h"
#include "MainWindowSessionController.h"
#include "MainWindowProjectController.h"
#include "MainWindowSessionStateService.h"
#include "MainWindowStructureNameOverridesService.h"
#include "ApplicationStylePolicy.h"
#include "LucideIconFactory.h"
#include "../core/SessionStore.h"
#include "../core/TherionFileTypes.h"
#include "../platform/ApplicationLanguageOverride.h"

namespace
{
constexpr const char *kMapEditorUiSignalsConnectedProperty = "_therionStudioMapEditorUiSignalsConnected";
#if defined(Q_OS_MACOS)
constexpr quint32 kMacVirtualKeyAnsi1 = 0x12;
constexpr quint32 kMacVirtualKeyAnsi2 = 0x13;
#endif

QSize minimumMainWindowSize()
{
    return QSize(720, 560);
}

QSize defaultMainWindowSize()
{
    QSize preferredSize(1280, 820);
    const QSize minimumSize = minimumMainWindowSize();

    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize availableSize = screen->availableGeometry().size();
        if (availableSize.isValid()) {
            preferredSize.setWidth(qMin(preferredSize.width(),
                                        qMax(minimumSize.width(), availableSize.width() * 4 / 5)));
            preferredSize.setHeight(qMin(preferredSize.height(),
                                         qMax(minimumSize.height(), availableSize.height() * 4 / 5)));
        }
    }

    return preferredSize.expandedTo(minimumSize);
}

void ensureUsableMainWindowSize(QMainWindow *window)
{
    if (window == nullptr) {
        return;
    }

    const QSize minimumSize = minimumMainWindowSize();
    window->setMinimumSize(minimumSize);
    if (window->width() < minimumSize.width() || window->height() < minimumSize.height()) {
        window->resize(window->size().expandedTo(defaultMainWindowSize()));
    }
}

bool isSupportedTextEditorFilePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    const QString fileName = info.fileName();
    if (TherionStudio::isTherionConfigFileName(fileName)) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("th")
        || suffix == QStringLiteral("txt")
        || suffix == QStringLiteral("log")) {
        return true;
    }

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);
    return mimeType.isValid() && mimeType.inherits(QStringLiteral("text/plain"));
}

bool supportsConfigurableDefaultTextEditorMode(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString fileName = info.fileName().trimmed().toLower();
    const QString suffix = info.suffix().trimmed().toLower();
    return TherionStudio::isTherionConfigFileName(fileName)
        || suffix == QStringLiteral("th");
}

bool openFileExternally(QWidget *parent, const QString &filePath)
{
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        return true;
    }

    QMessageBox::warning(parent,
                         QObject::tr("Open in External App"),
                         QObject::tr("Failed to open %1 with the system default application.")
                             .arg(QDir::toNativeSeparators(filePath)));
    return false;
}

void showUnsupportedFilePrompt(QWidget *parent, const QString &filePath)
{
    QMessageBox messageBox(parent);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle(QObject::tr("Unsupported File"));
    messageBox.setText(QObject::tr("Unsupported file."));
    messageBox.setInformativeText(
        QObject::tr("Therion Studio cannot open this file type in the internal editor:\n%1")
            .arg(QDir::toNativeSeparators(filePath)));
    auto *openExternalButton = messageBox.addButton(QObject::tr("Open in External App"), QMessageBox::ActionRole);
    messageBox.addButton(QMessageBox::Cancel);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    messageBox.exec();

    if (messageBox.clickedButton() == openExternalButton) {
        openFileExternally(parent, filePath);
    }
}

constexpr auto kWelcomeTabPropertyName = "therionStudioWelcomeTab";

int findWelcomeTabIndex(const QTabWidget *tabs)
{
    if (tabs == nullptr) {
        return -1;
    }

    for (int index = 0; index < tabs->count(); ++index) {
        QWidget *widget = tabs->widget(index);
        if (widget != nullptr && widget->property(kWelcomeTabPropertyName).toBool()) {
            return index;
        }
    }

    return -1;
}

QWidget *createCenteredMessage(const QString &title,
                               const QString &body,
                               const QString &buttonText = QString(),
                               std::function<void()> onButtonClick = {})
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *bodyLabel = new QLabel(body);
    bodyLabel->setWordWrap(true);

    layout->addStretch(1);

    layout->addWidget(titleLabel);
    layout->addWidget(bodyLabel);
    if (!buttonText.isEmpty()) {
        auto *button = new QPushButton(buttonText);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QObject::connect(button, &QPushButton::clicked, widget, [onButtonClick]() {
            if (onButtonClick) {
                onButtonClick();
            }
        });
        layout->addWidget(button);
    }

    layout->addStretch(1);

    return widget;
}


void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setFrameShape(QFrame::NoFrame);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

QString canonicalDocumentPath(const QString &filePath)
{
    return TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(filePath);
}

bool hasOnlyEditorModeShortcutModifier(const QKeyEvent *event)
{
    if (event == nullptr) {
        return false;
    }

    const Qt::KeyboardModifiers relevantModifiers =
        event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
#if defined(Q_OS_MACOS)
    return relevantModifiers == Qt::ControlModifier || relevantModifiers == Qt::MetaModifier;
#else
    return relevantModifiers == Qt::ControlModifier;
#endif
}

int editorModeShortcutIndexForKeyEvent(const QKeyEvent *event)
{
    if (!hasOnlyEditorModeShortcutModifier(event)) {
        return 0;
    }

    if (event->key() == Qt::Key_1) {
        return 1;
    }
    if (event->key() == Qt::Key_2) {
        return 2;
    }

#if defined(Q_OS_MACOS)
    // Use the physical top-row keys above Q/W so localized layouts such as Czech
    // do not require Shift to type the digit character.
    switch (event->nativeVirtualKey()) {
    case kMacVirtualKeyAnsi1:
        return 1;
    case kMacVirtualKeyAnsi2:
        return 2;
    default:
        break;
    }
#endif

    return 0;
}

class DetachedMapWindow final : public QMainWindow
{
public:
    explicit DetachedMapWindow(TherionStudio::MapEditorTab *mapTab, QWidget *parent = nullptr)
        : QMainWindow(parent)
        , mapTab_(mapTab)
    {
        setAttribute(Qt::WA_DeleteOnClose, true);
        setCentralWidget(mapTab);
    }

    TherionStudio::MapEditorTab *mapTab() const
    {
        return mapTab_;
    }

    void setCloseCallback(std::function<void(TherionStudio::MapEditorTab *)> callback)
    {
        closeCallback_ = std::move(callback);
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (closeCallback_) {
            closeCallback_(mapTab_);
        }
        QMainWindow::closeEvent(event);
    }

private:
    QPointer<TherionStudio::MapEditorTab> mapTab_;
    std::function<void(TherionStudio::MapEditorTab *)> closeCallback_;
};

} // namespace

MainWindow::MainWindow(std::unique_ptr<TherionStudio::ISessionStore> sessionStore,
                       TherionStudio::CommandCatalogStore commandCatalogStore,
                       QWidget *parent,
                       SessionRestoreMode restoreMode)
    : MainWindow(*sessionStore, std::move(commandCatalogStore), parent, restoreMode)
{
    ownedSessionStore_ = std::move(sessionStore);
}

MainWindow::MainWindow(TherionStudio::ISessionStore &sessionStore,
                       TherionStudio::CommandCatalogStore commandCatalogStore,
                       QWidget *parent,
                       SessionRestoreMode restoreMode)
    : QMainWindow(parent)
    , editorTabs_(new QTabWidget(this))
    , projectModel_(new QFileSystemModel(this))
    , structureModel_(new QStandardItemModel(this))
    , mapObjectsModel_(new QStandardItemModel(this))
    , sessionStore_(&sessionStore)
    , commandCatalogStore_(std::move(commandCatalogStore))
    , structureSidebarScanner_(new TherionStudio::ProjectStructureScanner(this))
{
    setWindowTitle(tr("Therion Studio"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AnimatedDocks);

    projectModel_->setRootPath(QDir::rootPath());

    connect(structureSidebarScanner_, &TherionStudio::ProjectStructureScanner::scanFinished,
            this, &MainWindow::handleStructureSidebarScanFinished);

    buildUi();
    qApp->installEventFilter(this);
    setMinimumSize(minimumMainWindowSize());
    resize(defaultMainWindowSize());
    if (restoreMode == SessionRestoreMode::RestoreSession) {
        restoreSessionState();
    }

    if (editorTabs_->count() == 0) {
        addWelcomeTab();
    }

    rebuildStructureSidebar();
    rebuildMapObjectsTree();
    updateMapEditorActionState();
    restoreSidebarWidth();
}

void MainWindow::buildUi()
{
    editorTabs_->setObjectName(QStringLiteral("mainEditorTabs"));
    editorTabs_->setTabsClosable(true);
    editorTabs_->setMovable(true);
    editorTabs_->setDocumentMode(true);
    connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabCloseRequested);
    connect(editorTabs_, &QTabWidget::currentChanged, this, [this](int) {
        rebuildMapObjectsTree();
        updateMapEditorActionState();
        refreshTherionConfigDisplay();
        refreshMapBackgroundPanel();
        refreshDocumentStatusWidgets();
        refreshWorkspaceModeSwitcher();
        refreshWorkspaceModeSwitcherGeometry();
        refreshViewMenuActions();
    });

    auto *mainContentHost = new QWidget(this);
    mainContentLayout_ = new QHBoxLayout(mainContentHost);
    mainContentLayout_->setContentsMargins(0, 0, 0, 0);
    mainContentLayout_->setSpacing(0);
    mainContentSplitter_ = new QSplitter(Qt::Horizontal, mainContentHost);
    applyThinSplitterStyle(mainContentSplitter_, QStringLiteral("mainContentSplitter"));
    mainContentSplitter_->setChildrenCollapsible(true);
    setCentralWidget(mainContentHost);

    editorAreaHost_ = new QWidget(mainContentSplitter_);
    editorAreaHost_->setObjectName(QStringLiteral("mainEditorArea"));
    editorAreaHost_->setAttribute(Qt::WA_StyledBackground, true);
    editorAreaHost_->setStyleSheet(TherionStudio::mainEditorSurfaceStyleSheet());
    auto *editorAreaHostLayout = new QHBoxLayout(editorAreaHost_);
    editorAreaHostLayout->setContentsMargins(0, 0, 0, 0);
    editorAreaHostLayout->setSpacing(0);

    auto *editorAreaDivider = new QFrame(editorAreaHost_);
    editorAreaDivider->setObjectName(QStringLiteral("mainEditorAreaLeftDivider"));
    editorAreaDivider->setFixedWidth(1);
    editorAreaDivider->setFrameShape(QFrame::NoFrame);

    editorAreaColumn_ = new QWidget(editorAreaHost_);
    editorAreaColumn_->setObjectName(QStringLiteral("mainEditorAreaColumn"));
    editorAreaColumn_->setAttribute(Qt::WA_StyledBackground, true);
    editorAreaHostLayout->addWidget(editorAreaDivider);
    editorAreaHostLayout->addWidget(editorAreaColumn_, 1);

    editorAreaLayout_ = new QVBoxLayout(editorAreaColumn_);
    editorAreaLayout_->setContentsMargins(0, 0, 0, 0);
    editorAreaLayout_->setSpacing(0);
    editorAreaLayout_->addWidget(editorTabs_, 1);

    buildStructureSidebar();
    mainContentSplitter_->addWidget(editorAreaHost_);
    mainContentLayout_->addWidget(mainContentSplitter_, 1);
    mainContentSplitter_->setCollapsible(0, true);
    mainContentSplitter_->setCollapsible(1, false);
    mainContentSplitter_->setStretchFactor(0, 0);
    mainContentSplitter_->setStretchFactor(1, 1);
    buildConsole();
    buildMenus();
    initializeWorkspaceModeSwitcher();

    initializeDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
    refreshWorkspaceModeSwitcherGeometry();
    QTimer::singleShot(0, this, [this]() {
        refreshWorkspaceModeSwitcherGeometry();
    });
    statusBar()->showMessage(tr("Ready"));
}


void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *newWindowAction = fileMenu->addAction(tr("New &Window"));
    newWindowAction->setMenuRole(QAction::NoRole);
    newWindowAction->setShortcut(QKeySequence::New);
    connect(newWindowAction, &QAction::triggered, this, &MainWindow::createNewWindow);

    openProjectAction_ = fileMenu->addAction(tr("&Open Project..."));
    openProjectAction_->setShortcut(QKeySequence::Open);
    connect(openProjectAction_, &QAction::triggered, this, &MainWindow::openProject);

    closeProjectAction_ = fileMenu->addAction(tr("&Close Project"));
    connect(closeProjectAction_, &QAction::triggered, this, &MainWindow::closeProject);
    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveActiveDocument);

    QAction *saveAllAction = fileMenu->addAction(tr("Save &All"));
    saveAllAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAllAction, &QAction::triggered, this, &MainWindow::saveAllDocuments);

    QAction *closeTabAction = fileMenu->addAction(tr("&Close"));
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeActiveTab);

    QAction *closeAllTabsAction = fileMenu->addAction(tr("Close All Tabs"));
    closeAllTabsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(closeAllTabsAction, &QAction::triggered, this, &MainWindow::closeAllTabs);

    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction(tr("Settings..."));
    settingsAction->setMenuRole(QAction::PreferencesRole);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = editMenu->addAction(tr("&Undo"));
    undoAction_->setShortcuts(QKeySequence::Undo);
    undoAction_->setStatusTip(tr("Undo the last change in the active document."));
    connect(undoAction_, &QAction::triggered, this, &MainWindow::triggerUndoForActiveDocument);

    redoAction_ = editMenu->addAction(tr("&Redo"));
    redoAction_->setShortcuts(QKeySequence::Redo);
    redoAction_->setStatusTip(tr("Redo the last undone change in the active document."));
    connect(redoAction_, &QAction::triggered, this, &MainWindow::triggerRedoForActiveDocument);
    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("&Find"));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() { showFindBar(false); });

    QAction *replaceAction = editMenu->addAction(tr("Find and &Replace"));
    replaceAction->setShortcut(QKeySequence::Replace);
    connect(replaceAction, &QAction::triggered, this, [this]() { showFindBar(true); });

    auto *rawModeShortcut = new QAction(this);
    rawModeShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    rawModeShortcut->setShortcutContext(Qt::WindowShortcut);
    addAction(rawModeShortcut);
    connect(rawModeShortcut, &QAction::triggered, this, &MainWindow::triggerRawModeForActiveDocument);

    auto *secondaryModeShortcut = new QAction(this);
    secondaryModeShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    secondaryModeShortcut->setShortcutContext(Qt::WindowShortcut);
    addAction(secondaryModeShortcut);
    connect(secondaryModeShortcut, &QAction::triggered, this, &MainWindow::triggerSecondaryEditorModeForActiveDocument);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    sidebarCollapseAction_ = viewMenu->addAction(QString());
    sidebarCollapseAction_->setStatusTip(tr("Expand or collapse the left sidebar."));
    connect(sidebarCollapseAction_, &QAction::triggered, this, [this]() {
        setSidebarCollapsed(!isSidebarEffectivelyCollapsed());
    });

    rightPanelCollapseAction_ = viewMenu->addAction(QString());
    rightPanelCollapseAction_->setStatusTip(tr("Expand or collapse the active document right-side panel."));
    connect(rightPanelCollapseAction_, &QAction::triggered, this, [this]() {
        setCurrentDocumentRightPanelCollapsed(!currentDocumentRightPanelCollapsed());
    });
    contextHelpCollapseAction_ = viewMenu->addAction(QString());
    contextHelpCollapseAction_->setStatusTip(tr("Expand or collapse contextual help while the map pane is detached."));
    connect(contextHelpCollapseAction_, &QAction::triggered, this, [this]() {
        setCurrentDetachedMapContextHelpCollapsed(!currentDetachedMapContextHelpCollapsed());
    });

    viewMenu->addSeparator();
    mapMagnifierAction_ = viewMenu->addAction(QString());
    mapMagnifierAction_->setData(sessionStore_ == nullptr || sessionStore_->therionMapMagnifierEnabled());
    mapMagnifierAction_->setStatusTip(tr("Show or hide the map magnifier overlay in map editors."));
    connect(mapMagnifierAction_, &QAction::triggered, this, [this]() {
        const bool enabled = !mapMagnifierAction_->data().toBool();
        mapMagnifierAction_->setData(enabled);
        if (sessionStore_ != nullptr) {
            sessionStore_->setTherionMapMagnifierEnabled(enabled);
        }
        setMapMagnifierEnabledForOpenTabs(enabled);
        refreshViewMenuActions();
    });

#if !defined(Q_OS_MACOS)
    viewMenu->addSeparator();
    fullScreenAction_ = viewMenu->addAction(QString());
    fullScreenAction_->setMenuRole(QAction::NoRole);
    fullScreenAction_->setShortcut(QKeySequence::FullScreen);
    fullScreenAction_->setStatusTip(tr("Enter or exit full screen mode."));
    connect(fullScreenAction_, &QAction::triggered, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
        refreshFullScreenAction();
    });
#endif

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *userManualAction = helpMenu->addAction(tr("User Manual"));
    connect(userManualAction, &QAction::triggered, this, [this]() {
        TherionStudio::showUserManualDialog(this);
    });
    helpMenu->addSeparator();
    QAction *aboutAction = helpMenu->addAction(tr("About Therion Studio"));
#if defined(Q_OS_MACOS)
    aboutAction->setMenuRole(QAction::AboutRole);
#else
    aboutAction->setMenuRole(QAction::NoRole);
#endif
    connect(aboutAction, &QAction::triggered, this, [this]() {
        TherionStudio::showAboutDialog(this);
    });

    updateProjectActionState();
    refreshViewMenuActions();
}

void MainWindow::restoreSessionState()
{
    TherionStudio::MainWindowSessionController::RestoreSessionActions actions;
    actions.restoreGeometry = [this](const QByteArray &geometry) {
        return restoreGeometry(geometry);
    };
    actions.restoreState = [this](const QByteArray &state) {
        restoreState(state);
    };
    actions.resizeToDefault = [this]() {
        resize(defaultMainWindowSize());
    };
    actions.ensureUsableWindowSize = [this]() {
        ensureUsableMainWindowSize(this);
    };
    actions.applyProjectRootToBrowser = [this](const QString &projectPath) {
        projectRootPath_ = projectPath;
        projectModel_->setRootPath(projectRootPath_);
        projectTree_->setRootIndex(projectModel_->index(projectRootPath_));
    };
    actions.appendConsoleLine = [this](const QString &line) {
        appendConsoleLine(line);
    };
    actions.loadStructureNameOverrides = [this]() { loadStructureNameOverrides(); };
    actions.syncOpenDocumentsToProjectRoot = [this]() { syncOpenDocumentsToProjectRoot(); };
    actions.rebuildStructureSidebar = [this]() { rebuildStructureSidebar(); };
    actions.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    actions.updateProjectActionState = [this]() { updateProjectActionState(); };
    actions.restoreOpenDocuments = [this]() { restoreOpenDocuments(); };
    TherionStudio::MainWindowSessionController::restoreSession(*sessionStore_, actions);
}

void MainWindow::persistSessionState()
{
    TherionStudio::MainWindowSessionStateService::MainWindowStateSnapshot snapshot;
    snapshot.windowGeometry = saveGeometry();
    snapshot.windowState = saveState();
    snapshot.projectRootPath = projectRootPath_;
    snapshot.therionExecutablePath = therionExecutableInput();
    snapshot.therionWorkingDirectory = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString();
    snapshot.therionRunTargetMode = therionRunTargetMode();
    snapshot.therionTargetConfigPath = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text().trimmed() : QString();
    snapshot.structureNameOverridesJson =
        TherionStudio::MainWindowStructureNameOverridesService::serialize(structureNameOverrides_);
    TherionStudio::MainWindowSessionStateService::persistMainWindowState(*sessionStore_, snapshot);
    persistOpenDocuments();
}

void MainWindow::restoreOpenDocuments()
{
    TherionStudio::MainWindowSessionDocumentController::RestoreOpenDocumentsActions actions;
    actions.openMapEditorDocument = [this](const QString &documentPath) {
        openMapEditorTab(documentPath);
    };
    actions.openTextEditorDocument = [this](const QString &documentPath) {
        return openTextTab(documentPath, false) != nullptr;
    };
    actions.appendSkippedUnsupportedDocumentConsole = [this](const QString &documentPath) {
        appendConsoleLine(tr("Skipped unsupported document during session restore: %1")
                              .arg(QDir::toNativeSeparators(documentPath)));
    };
    actions.activateRestoredDocumentPath = [this](const QString &activeDocumentPath) {
        QWidget *activeWidget = documentWidgetForFilePath(activeDocumentPath);
        if (activeWidget != nullptr) {
            const int tabIndex = editorTabs_->indexOf(activeWidget);
            if (tabIndex >= 0) {
                editorTabs_->setCurrentWidget(activeWidget);
                if (documentPathForWidget(activeWidget).isEmpty()) {
                    editorTabs_->setCurrentIndex(editorTabs_->count() - 1);
                }
            } else {
                focusDetachedMapEditorTab(canonicalDocumentPath(activeDocumentPath));
            }
        }
    };
    actions.persistOpenDocuments = [this]() {
        persistOpenDocuments();
    };
    TherionStudio::MainWindowSessionDocumentController::restoreOpenDocuments(*sessionStore_, actions);
}

void MainWindow::persistOpenDocuments()
{
    TherionStudio::MainWindowSessionDocumentController::PersistOpenDocumentsSnapshot snapshot;

    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        const QString filePath = documentPathForWidget(tabWidget);
        if (filePath.isEmpty()) {
            continue;
        }

        snapshot.tabDocumentPaths.append(filePath);
    }

    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }
        const QString filePath = documentPathForWidget(detachedTab);
        if (filePath.isEmpty()) {
            continue;
        }
        snapshot.detachedMapDocumentPaths.append(filePath);
    }

    for (auto iterator = detachedMapWindowsByPath_.constBegin(); iterator != detachedMapWindowsByPath_.constEnd(); ++iterator) {
        QMainWindow *window = iterator.value();
        if (window != nullptr && window->isActiveWindow()) {
            snapshot.activeDetachedDocumentPaths.append(iterator.key());
            break;
        }
    }
    QWidget *currentWidget = currentDocumentWidget();
    snapshot.currentDocumentPath = currentWidget != nullptr ? documentPathForWidget(currentWidget) : QString();

    TherionStudio::MainWindowSessionDocumentController::persistOpenDocuments(snapshot, *sessionStore_);
}

void MainWindow::addWelcomeTab()
{
    QWidget *welcomeWidget = nullptr;
    if (projectRootPath_.isEmpty()) {
        welcomeWidget = createCenteredMessage(tr("Therion Studio"),
                                              tr("Open a project to begin working with Therion documents, maps, and structure views."),
                                              tr("Open Project..."),
                                              [this]() {
                                                  openProject();
                                              });
    } else {
        welcomeWidget = createCenteredMessage(tr("Therion Studio"),
                                              tr("Open file from sidebar to begin editing this project."));
    }
    welcomeWidget->setProperty(kWelcomeTabPropertyName, true);

    const int existingWelcomeIndex = findWelcomeTabIndex(editorTabs_);
    if (existingWelcomeIndex >= 0) {
        const bool wasCurrent = editorTabs_->currentIndex() == existingWelcomeIndex;
        QWidget *existingWidget = editorTabs_->widget(existingWelcomeIndex);
        editorTabs_->removeTab(existingWelcomeIndex);
        if (existingWidget != nullptr) {
            existingWidget->deleteLater();
        }
        editorTabs_->insertTab(existingWelcomeIndex, welcomeWidget, tr("Welcome"));
        if (wasCurrent) {
            editorTabs_->setCurrentIndex(existingWelcomeIndex);
        }
    } else {
        editorTabs_->addTab(welcomeWidget, tr("Welcome"));
    }

    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
}

void MainWindow::createNewWindow()
{
    auto sessionStore = std::make_unique<TherionStudio::InMemorySessionStore>();
    if (sessionStore_ != nullptr) {
        sessionStore->setApplicationLanguage(sessionStore_->applicationLanguage());
        sessionStore->setDefaultTextEditorMode(sessionStore_->defaultTextEditorMode());
        sessionStore->setTherionExecutablePath(sessionStore_->therionExecutablePath());
        sessionStore->setTherionRunTargetMode(sessionStore_->therionRunTargetMode());
        sessionStore->setTherionMapMagnifierEnabled(sessionStore_->therionMapMagnifierEnabled());
    }

    auto *window = new MainWindow(std::move(sessionStore),
                                  commandCatalogStore_,
                                  nullptr,
                                  SessionRestoreMode::StartEmpty);
    window->show();
}

void MainWindow::showSettingsDialog()
{
    if (sessionStore_ == nullptr) {
        return;
    }

    TherionStudio::MainWindowSettingsDialog::Settings initialSettings;
    const QString nativeApplicationLanguage =
        TherionStudio::Platform::applicationLanguageOverride();
    initialSettings.applicationLanguage = nativeApplicationLanguage != QStringLiteral("system")
        ? nativeApplicationLanguage
        : sessionStore_->applicationLanguage();
    initialSettings.therionExecutablePath = sessionStore_->therionExecutablePath();
    initialSettings.defaultTextEditorMode = sessionStore_->defaultTextEditorMode();

    TherionStudio::MainWindowSettingsDialog dialog(initialSettings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const TherionStudio::MainWindowSettingsDialog::Settings updatedSettings = dialog.settings();
    const QString updatedApplicationLanguage =
        TherionStudio::Platform::normalizeApplicationLanguageSetting(
            updatedSettings.applicationLanguage);
    const bool languageChanged =
        updatedApplicationLanguage
        != TherionStudio::Platform::normalizeApplicationLanguageSetting(
            initialSettings.applicationLanguage);

    sessionStore_->setApplicationLanguage(updatedApplicationLanguage);
    TherionStudio::Platform::setApplicationLanguageOverride(updatedApplicationLanguage);
    sessionStore_->setTherionExecutablePath(updatedSettings.therionExecutablePath);
    sessionStore_->setDefaultTextEditorMode(updatedSettings.defaultTextEditorMode);
    refreshTherionConfigDisplay();

    if (languageChanged) {
        QMessageBox::information(this,
                                 tr("Settings"),
                                 tr("Language changes will take effect after restarting Therion Studio."));
    }
}

void MainWindow::openProject()
{
    const QString selectedProjectPath =
        QFileDialog::getExistingDirectory(this, tr("Open Therion Project"), QString());

    TherionStudio::MainWindowProjectController::Actions actions;
    actions.showWarningDialog = [this](const QString &title, const QString &message) {
        QMessageBox::warning(this, title, message);
    };
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    actions.appendConsoleLine = [this](const QString &line) {
        appendConsoleLine(line);
    };
    actions.setProjectRootPath = [this](const QString &projectRootPath) {
        projectRootPath_ = projectRootPath;
    };
    actions.applyProjectRootToBrowser = [this](const QString &projectRootPath) {
        projectModel_->setRootPath(projectRootPath);
        projectTree_->setRootIndex(projectModel_->index(projectRootPath));
    };
    actions.loadStructureNameOverrides = [this]() { loadStructureNameOverrides(); };
    actions.syncOpenDocumentsToProjectRoot = [this]() { syncOpenDocumentsToProjectRoot(); };
    actions.rebuildStructureSidebar = [this]() { rebuildStructureSidebar(); };
    actions.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    actions.updateProjectActionState = [this]() { updateProjectActionState(); };
    actions.ensureWelcomeTab = [this]() { addWelcomeTab(); };
    TherionStudio::MainWindowProjectController::openProject(selectedProjectPath,
                                                            editorTabs_->count() == 0,
                                                            findWelcomeTabIndex(editorTabs_) >= 0,
                                                            *sessionStore_,
                                                            actions);
}

void MainWindow::closeProject()
{
    TherionStudio::MainWindowProjectController::Actions actions;
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    actions.appendConsoleLine = [this](const QString &line) {
        appendConsoleLine(line);
    };
    actions.setProjectRootPath = [this](const QString &projectRootPath) {
        projectRootPath_ = projectRootPath;
    };
    actions.clearDocumentTabs = [this]() { clearDocumentTabs(); };
    actions.resetProjectBrowser = [this]() { resetProjectBrowser(); };
    actions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    actions.resetProjectTherionRunContext = [this]() { resetProjectTherionRunContext(); };
    actions.rebuildStructureSidebar = [this]() { rebuildStructureSidebar(); };
    actions.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    actions.updateProjectActionState = [this]() { updateProjectActionState(); };
    TherionStudio::MainWindowProjectController::closeProject(projectRootPath_,
                                                             [this]() { return confirmCloseDirtyDocuments(); },
                                                             *sessionStore_,
                                                             actions);
}
void MainWindow::handleProjectTreeActivated(const QModelIndex &index)
{
    if (projectModel_ == nullptr || !index.isValid() || projectModel_->isDir(index)) {
        return;
    }

    const QString filePath = projectModel_->filePath(index);
    if (filePath.isEmpty()) {
        return;
    }

    if (QFileInfo(filePath).suffix().toLower() == QStringLiteral("th2")) {
        openMapEditorTab(filePath);
    } else if (isSupportedTextEditorFilePath(filePath)) {
        openTextTab(filePath);
    } else {
        showUnsupportedFilePrompt(this, filePath);
    }
}

void MainWindow::handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber)
{
    updateStructureSelectionFromEditorLocation(filePath, lineNumber);
    updateMapObjectSelectionFromEditorLocation(filePath, lineNumber);
}

void MainWindow::handleTabCloseRequested(int index)
{
    TherionStudio::MainWindowDocumentController::Actions actions;
    actions.confirmCloseTab = [this](int tabIndex) { return confirmCloseTab(tabIndex); };
    actions.closeTab = [this](int tabIndex) {
        QWidget *tabWidget = editorTabs_->widget(tabIndex);
        if (tabWidget == nullptr) {
            return false;
        }

        editorTabs_->removeTab(tabIndex);
        tabWidget->deleteLater();
        return true;
    };
    actions.hasNoAttachedTabs = [this]() { return editorTabs_->count() == 0; };
    actions.ensureWelcomeTab = [this]() { addWelcomeTab(); };
    actions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    actions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    TherionStudio::MainWindowDocumentController::CloseTabRequest request;
    request.tabIndex = index;
    TherionStudio::MainWindowDocumentController::closeTab(request, actions);
}

void MainWindow::closeActiveTab()
{
    const int currentIndex = editorTabs_->currentIndex();
    if (currentIndex < 0) {
        return;
    }

    handleTabCloseRequested(currentIndex);
}

void MainWindow::closeAllTabs()
{
    TherionStudio::MainWindowDocumentController::CloseAllRequest request;
    for (int index = editorTabs_->count() - 1; index >= 0; --index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget == nullptr) {
            continue;
        }

        const QString documentPath = documentPathForWidget(tabWidget);
        if (documentPath.isEmpty()) {
            continue;
        }

        TherionStudio::MainWindowDocumentController::CloseTabEntry entry;
        entry.tabIndex = index;
        entry.documentPath = documentPath;
        request.tabEntries.append(entry);
    }

    const QList<TherionStudio::MapEditorTab *> detachedTabs = detachedMapEditorTabs();
    for (TherionStudio::MapEditorTab *detachedTab : detachedTabs) {
        if (detachedTab == nullptr) {
            continue;
        }

        const QString path = detachedMapPathsByTab_.value(detachedTab);
        if (path.isEmpty()) {
            continue;
        }
        request.detachedDocumentPaths.append(path);
    }

    TherionStudio::MainWindowDocumentController::Actions actions;
    actions.confirmCloseTab = [this](int tabIndex) { return confirmCloseTab(tabIndex); };
    actions.confirmCloseDocumentPath = [this](const QString &documentPath) {
        if (auto *detachedTab = detachedMapEditorTabForPath(documentPath); detachedTab != nullptr) {
            return confirmCloseDocumentWidget(detachedTab);
        }
        return false;
    };
    actions.closeTab = [this](int tabIndex) {
        QWidget *tabWidget = editorTabs_->widget(tabIndex);
        if (tabWidget == nullptr) {
            return false;
        }

        editorTabs_->removeTab(tabIndex);
        tabWidget->deleteLater();
        return true;
    };
    actions.closeDetachedDocumentPath = [this](const QString &documentPath) {
        if (QMainWindow *window = detachedMapWindowsByPath_.value(documentPath)) {
            const bool wasClearingTabs = clearingDocumentTabs_;
            clearingDocumentTabs_ = true;
            window->close();
            clearingDocumentTabs_ = wasClearingTabs;
            return true;
        }

        return false;
    };
    actions.hasNoAttachedTabs = [this]() { return editorTabs_->count() == 0; };
    actions.ensureWelcomeTab = [this]() { addWelcomeTab(); };
    actions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    actions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    TherionStudio::MainWindowDocumentController::closeAll(request, actions);
}

void MainWindow::resetProjectBrowser()
{
    const QString defaultRootPath = QDir::rootPath();
    projectModel_->setRootPath(defaultRootPath);
    projectTree_->setRootIndex(projectModel_->index(defaultRootPath));
}

void MainWindow::refreshProjectBrowserView(const QString &focusPath)
{
    if (projectModel_ == nullptr || projectTree_ == nullptr) {
        return;
    }

    const QString rootPath = projectRootPath_.isEmpty() ? QDir::rootPath() : projectRootPath_;
    projectModel_->setRootPath(rootPath);
    projectTree_->setRootIndex(projectModel_->index(rootPath));

    const QString normalizedFocusPath = focusPath.trimmed();
    if (normalizedFocusPath.isEmpty()) {
        return;
    }

    const QString absoluteFocusPath = QFileInfo(normalizedFocusPath).absoluteFilePath();
    QTimer::singleShot(0, this, [this, absoluteFocusPath]() {
        if (projectModel_ == nullptr || projectTree_ == nullptr) {
            return;
        }

        QModelIndex targetIndex = projectModel_->index(absoluteFocusPath);
        if (!targetIndex.isValid()) {
            return;
        }

        QModelIndex parentIndex = targetIndex.parent();
        while (parentIndex.isValid() && parentIndex != projectTree_->rootIndex()) {
            projectTree_->expand(parentIndex);
            parentIndex = parentIndex.parent();
        }

        projectTree_->scrollTo(targetIndex);
        projectTree_->setCurrentIndex(targetIndex);
    });
}

QString MainWindow::structureOverrideKey(const QString &sourceFile, int lineNumber) const
{
    return QStringLiteral("%1|%2|%3").arg(QDir(projectRootPath_).absolutePath(), sourceFile).arg(lineNumber);
}

void MainWindow::loadStructureNameOverrides()
{
    structureNameOverrides_ = TherionStudio::MainWindowStructureNameOverridesService::parse(
        sessionStore_->structureNameOverrides());
}

void MainWindow::saveActiveDocument()
{
    TherionStudio::MainWindowDocumentController::Actions actions;
    actions.showComingSoon = [this](const QString &featureName) { showComingSoon(featureName); };
    actions.saveDocument = [this](const QString &documentPath, QString *errorMessage) {
        QWidget *tabWidget = documentWidgetForFilePath(documentPath);
        if (tabWidget == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Unable to resolve the active document.");
            }
            return false;
        }

        return documentSaveForWidget(tabWidget, errorMessage);
    };
    actions.showWarningDialog = [this](const QString &title, const QString &message) {
        QMessageBox::warning(this, title, message);
    };
    actions.updateTabTitle = [this](const QString &documentPath) {
        if (QWidget *tabWidget = documentWidgetForFilePath(documentPath)) {
            updateTabTitle(tabWidget);
        }
    };
    actions.documentDisplayName = [this](const QString &documentPath) {
        if (QWidget *tabWidget = documentWidgetForFilePath(documentPath)) {
            return documentDisplayNameForWidget(tabWidget);
        }
        return documentPath;
    };
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    TherionStudio::MainWindowDocumentController::SaveActiveRequest request;
    request.hasActiveDocument = currentDocumentWidget() != nullptr;
    request.activeDocumentPath = documentPathForWidget(currentDocumentWidget());
    TherionStudio::MainWindowDocumentController::saveActive(request, actions);
}

void MainWindow::saveAllDocuments()
{
    QStringList documentPaths;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget == nullptr) {
            continue;
        }

        const QString path = documentPathForWidget(tabWidget);
        if (path.isEmpty()) {
            continue;
        }

        documentPaths.append(path);
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }

        const QString path = documentPathForWidget(detachedTab);
        if (path.isEmpty()) {
            continue;
        }

        documentPaths.append(path);
    }

    TherionStudio::MainWindowDocumentController::Actions actions;
    actions.documentIsDirty = [this](const QString &documentPath) {
        QWidget *tabWidget = documentWidgetForFilePath(documentPath);
        return tabWidget != nullptr && documentIsDirtyForWidget(tabWidget);
    };
    actions.saveDocument = [this](const QString &documentPath, QString *errorMessage) {
        QWidget *tabWidget = documentWidgetForFilePath(documentPath);
        if (tabWidget == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Unable to resolve open document %1").arg(documentPath);
            }
            return false;
        }

        return documentSaveForWidget(tabWidget, errorMessage);
    };
    actions.isAttachedDocument = [this](const QString &documentPath) {
        QWidget *tabWidget = documentWidgetForFilePath(documentPath);
        return tabWidget != nullptr && editorTabs_->indexOf(tabWidget) >= 0;
    };
    actions.updateTabTitle = [this](const QString &documentPath) {
        if (QWidget *tabWidget = documentWidgetForFilePath(documentPath)) {
            updateTabTitle(tabWidget);
        }
    };
    actions.showWarningDialog = [this](const QString &title, const QString &message) {
        QMessageBox::warning(this, title, message);
    };
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    TherionStudio::MainWindowDocumentController::SaveAllRequest request;
    request.documentPaths = documentPaths;
    TherionStudio::MainWindowDocumentController::saveAll(request, actions);
}

void MainWindow::showFindBar(bool replaceMode)
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        showComingSoon(replaceMode ? tr("Replace") : tr("Find"));
        return;
    }

    documentShowFindBarForWidget(tabWidget, replaceMode);
}

TherionStudio::TextEditorTab *MainWindow::openTextTab(const QString &filePath, bool showUnsupportedPrompt)
{
    QList<TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath> attachedTextTabs;
    attachedTextTabs.reserve(editorTabs_->count());
    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(index));
        if (existingTab == nullptr) {
            continue;
        }

        TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath entry;
        entry.index = index;
        entry.documentPath = existingTab->filePath();
        attachedTextTabs.append(entry);
    }

    const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenTextTab(
        filePath,
        isSupportedTextEditorFilePath(canonicalDocumentPath(filePath)),
        attachedTextTabs);
    const QString canonicalPath = openPlan.canonicalPath;
    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenTextTabAction::UnsupportedDocument) {
        if (showUnsupportedPrompt) {
            showUnsupportedFilePrompt(this, canonicalPath);
        }
        return nullptr;
    }

    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenTextTabAction::ReuseAttachedTab) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(openPlan.reuseTabIndex));
        if (existingTab == nullptr) {
            return nullptr;
        }

        existingTab->setProjectRootPath(projectRootPath_);
        existingTab->setModeSelectorVisible(false);
        TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
        openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
        openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
        openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
        TherionStudio::MainWindowDocumentTabOpenController::ActivateExistingTabRequest openRequest;
        openRequest.tabIndex = openPlan.reuseTabIndex;
        TherionStudio::MainWindowDocumentTabOpenController::activateExistingTab(openRequest, openActions);
        return existingTab;
    }

    auto *tab = new TherionStudio::TextEditorTab(fileSystem_, commandCatalogStore_);
    if (supportsConfigurableDefaultTextEditorMode(canonicalPath)
        && sessionStore_ != nullptr
        && sessionStore_->defaultTextEditorMode().trimmed().toLower() == QStringLiteral("blocks")) {
        tab->setInitialEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
    }
    tab->setModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    connect(tab, &TherionStudio::TextEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });

    connect(tab, &TherionStudio::TextEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
        }
    });

    connect(tab, &TherionStudio::TextEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::TextEditorTab::documentTextChanged, this, [this, tab]() {
        if (!projectRootPath_.isEmpty()) {
            requestStructureSidebarRebuild();
        }
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::TextEditorTab::editorModeChanged, this, [this, tab](TherionStudio::TextEditorTab::EditorMode) {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
            refreshViewMenuActions();
        }
    });

    TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
    openActions.removeWelcomeTabIfPresent = [this]() {
        const int welcomeTabIndex = findWelcomeTabIndex(editorTabs_);
        if (welcomeTabIndex < 0) {
            return;
        }

        QWidget *welcomeWidget = editorTabs_->widget(welcomeTabIndex);
        editorTabs_->removeTab(welcomeTabIndex);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
    };
    openActions.addTab = [this, tab](const QString &tabTitle) {
        return editorTabs_->addTab(tab, tabTitle);
    };
    openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
    openActions.handleCurrentLineChanged = [this](const QString &openedPath, int lineNumber) {
        handleTextEditorCurrentLineChanged(openedPath, lineNumber);
    };
    openActions.updateCurrentTabTitle = [this, tab]() { updateTabTitle(tab); };
    openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
    openActions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    openActions.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };

    TherionStudio::MainWindowDocumentTabOpenController::AttachNewTabRequest openRequest;
    openRequest.tabTitle = tab->displayName();
    openRequest.openedDocumentPath = tab->filePath();
    openRequest.currentLineNumber = tab->currentLineNumber();
    openRequest.consoleOpenedLine = tr("Opened %1").arg(canonicalPath);
    TherionStudio::MainWindowDocumentTabOpenController::attachNewTab(openRequest, openActions);
    return tab;
}

void MainWindow::connectMapEditorTabUiSignals(TherionStudio::MapEditorTab *tab)
{
    if (tab == nullptr) {
        return;
    }

    connect(tab,
            &TherionStudio::MapEditorTab::openDedicatedWindowRequested,
            this,
            &MainWindow::handleMapEditorDetachRequested,
            Qt::UniqueConnection);

    if (tab->property(kMapEditorUiSignalsConnectedProperty).toBool()) {
        return;
    }
    tab->setProperty(kMapEditorUiSignalsConnectedProperty, true);

    connect(tab,
            &TherionStudio::MapEditorTab::workspaceModeChanged,
            this,
            [this, tab](TherionStudio::MapEditorTab::WorkspaceMode) {
                if (currentDocumentWidget() == tab) {
                    refreshWorkspaceModeSwitcher();
                    refreshViewMenuActions();
                }
            });
    connect(tab,
            &TherionStudio::MapEditorTab::mapPaneDetachStateChanged,
            this,
            [this, tab](bool) {
                if (currentDocumentWidget() == tab) {
                    refreshDocumentStatusWidgets();
                    refreshWorkspaceModeSwitcher();
                    refreshViewMenuActions();
                }
            });
    connect(tab, &TherionStudio::MapEditorTab::commandSurfaceStateChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
            refreshViewMenuActions();
        }
    });
}

void MainWindow::setMapMagnifierEnabledForOpenTabs(bool enabled)
{
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index))) {
            mapTab->setMagnifierEnabled(enabled);
        }
    }

    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab != nullptr) {
            detachedTab->setMagnifierEnabled(enabled);
        }
    }
}

TherionStudio::MapEditorTab *MainWindow::openMapEditorTab(const QString &filePath)
{
    QStringList detachedMapDocumentPaths;
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }

        detachedMapDocumentPaths.append(detachedTab->filePath());
    }

    QList<TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath> attachedMapTabs;
    attachedMapTabs.reserve(editorTabs_->count());
    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index));
        if (existingTab == nullptr) {
            continue;
        }

        TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath entry;
        entry.index = index;
        entry.documentPath = existingTab->filePath();
        attachedMapTabs.append(entry);
    }

    const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenMapTab(
        filePath,
        detachedMapDocumentPaths,
        attachedMapTabs);
    const QString canonicalPath = openPlan.canonicalPath;

    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenMapTabAction::FocusDetachedTab) {
        auto *detachedTab = detachedMapEditorTabForPath(canonicalPath);
        if (detachedTab == nullptr) {
            return nullptr;
        }

        detachedTab->setProjectRootPath(projectRootPath_);
        detachedTab->setInlineWorkspaceModeSelectorVisible(true);
        focusDetachedMapEditorTab(canonicalPath);
        refreshDocumentStatusWidgets();
        refreshWorkspaceModeSwitcher();
        return detachedTab;
    }

    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenMapTabAction::ReuseAttachedTab) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(openPlan.reuseTabIndex));
        if (existingTab == nullptr) {
            return nullptr;
        }

        existingTab->setProjectRootPath(projectRootPath_);
        existingTab->setInlineWorkspaceModeSelectorVisible(false);
        connectMapEditorTabUiSignals(existingTab);
        TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
        openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
        openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
        openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
        TherionStudio::MainWindowDocumentTabOpenController::ActivateExistingTabRequest openRequest;
        openRequest.tabIndex = openPlan.reuseTabIndex;
        TherionStudio::MainWindowDocumentTabOpenController::activateExistingTab(openRequest, openActions);
        return existingTab;
    }

    auto *tab = new TherionStudio::MapEditorTab(fileSystem_, *sessionStore_, commandCatalogStore_);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    connect(tab, &TherionStudio::MapEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::MapEditorTab::documentTextChanged, this, [this, tab]() {
        if (!projectRootPath_.isEmpty()) {
            requestStructureSidebarRebuild();
        }
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayersChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayerPropertiesChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::modeStatusChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::zoomStatusChanged, this, [this, tab](int) {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connectMapEditorTabUiSignals(tab);

    TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
    openActions.removeWelcomeTabIfPresent = [this]() {
        const int welcomeTabIndex = findWelcomeTabIndex(editorTabs_);
        if (welcomeTabIndex < 0) {
            return;
        }

        QWidget *welcomeWidget = editorTabs_->widget(welcomeTabIndex);
        editorTabs_->removeTab(welcomeTabIndex);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
    };
    openActions.addTab = [this, tab](const QString &tabTitle) {
        return editorTabs_->addTab(tab, tabTitle);
    };
    openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
    openActions.handleCurrentLineChanged = [this](const QString &openedPath, int lineNumber) {
        handleTextEditorCurrentLineChanged(openedPath, lineNumber);
    };
    openActions.updateCurrentTabTitle = [this, tab]() { updateTabTitle(tab); };
    openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
    openActions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    openActions.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };

    TherionStudio::MainWindowDocumentTabOpenController::AttachNewTabRequest openRequest;
    openRequest.tabTitle = tab->displayName();
    openRequest.openedDocumentPath = tab->filePath();
    openRequest.currentLineNumber = tab->currentLineNumber();
    openRequest.consoleOpenedLine = tr("Opened %1").arg(canonicalPath);
    TherionStudio::MainWindowDocumentTabOpenController::attachNewTab(openRequest, openActions);
    return tab;
}

QWidget *MainWindow::documentWidgetForFilePath(const QString &filePath) const
{
    const QString canonicalPath = canonicalDocumentPath(filePath);

    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (documentPathForWidget(tabWidget) == canonicalPath) {
            return tabWidget;
        }
    }

    if (auto *detachedTab = detachedMapEditorTabForPath(canonicalPath); detachedTab != nullptr) {
        return detachedTab;
    }

    return nullptr;
}

void MainWindow::syncOpenDocumentsToProjectRoot()
{
    for (int index = 0; index < editorTabs_->count(); ++index) {
        documentSetProjectRootPathForWidget(editorTabs_->widget(index), projectRootPath_);
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        documentSetProjectRootPathForWidget(detachedTab, projectRootPath_);
    }
    refreshDocumentStatusWidgets();
}

void MainWindow::handleMapEditorDetachRequested(TherionStudio::MapEditorTab *tab)
{
    detachMapEditorTab(tab);
}

void MainWindow::detachMapEditorTab(TherionStudio::MapEditorTab *tab)
{
    if (tab == nullptr) {
        return;
    }

    const QString canonicalPath = canonicalDocumentPath(tab->filePath());
    if (canonicalPath.isEmpty()) {
        return;
    }

    if (detachedMapPathsByTab_.contains(tab)) {
        focusDetachedMapEditorTab(canonicalPath);
        refreshWorkspaceModeSwitcher();
        return;
    }

    const int tabIndex = editorTabs_->indexOf(tab);
    if (tabIndex >= 0) {
        editorTabs_->removeTab(tabIndex);
        if (editorTabs_->count() == 0) {
            addWelcomeTab();
        }
    }

    // Make the detached map window a true top-level window and explicitly
    // reparent the tab out of the tab stack before attaching it.
    tab->setParent(nullptr);
    tab->setInlineWorkspaceModeSelectorVisible(true);
    auto *window = new DetachedMapWindow(tab);
    window->setWindowTitle(documentDisplayNameForWidget(tab));
    window->setWindowFilePath(canonicalPath);
    window->setCloseCallback([this](TherionStudio::MapEditorTab *mapTab) {
        if (mapTab == nullptr) {
            return;
        }

        if (!clearingDocumentTabs_ && !shuttingDown_) {
            reattachDetachedMapEditorTab(mapTab, true);
            return;
        }

        const QString path = detachedMapPathsByTab_.take(mapTab);
        if (!path.isEmpty()) {
            detachedMapWindowsByPath_.remove(path);
        }
    });

    detachedMapWindowsByPath_.insert(canonicalPath, window);
    detachedMapPathsByTab_.insert(tab, canonicalPath);

    connect(tab, &TherionStudio::MapEditorTab::titleChanged, this, [this, tab]() {
        const QString path = detachedMapPathsByTab_.value(tab);
        if (!path.isEmpty()) {
            if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
                window->setWindowTitle(documentDisplayNameForWidget(tab));
            }
        }

        updateTabTitle(tab);
    });
    connect(tab, &QObject::destroyed, this, [this, tab]() {
        const QString path = detachedMapPathsByTab_.take(tab);
        if (!path.isEmpty()) {
            detachedMapWindowsByPath_.remove(path);
        }
    });
    connect(window, &QObject::destroyed, this, [this, canonicalPath]() {
        detachedMapWindowsByPath_.remove(canonicalPath);
    });

    window->resize(tab->size().isValid() ? tab->size() : QSize(1200, 800));
    window->show();
    window->raise();
    window->activateWindow();

    refreshWorkspaceModeSwitcher();
    persistOpenDocuments();
    appendConsoleLine(tr("Opened dedicated map window for %1").arg(canonicalPath));
}

void MainWindow::reattachDetachedMapEditorTab(TherionStudio::MapEditorTab *tab, bool focusTab)
{
    if (tab == nullptr) {
        return;
    }

    const QString path = detachedMapPathsByTab_.take(tab);
    if (!path.isEmpty()) {
        if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
            if (window->centralWidget() == tab) {
                window->takeCentralWidget();
            }
        }
        detachedMapWindowsByPath_.remove(path);
    }

    if (editorTabs_->count() == 1 && documentPathForWidget(editorTabs_->widget(0)).isEmpty()) {
        QWidget *welcomeWidget = editorTabs_->widget(0);
        editorTabs_->removeTab(0);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
    }

    int existingIndex = editorTabs_->indexOf(tab);
    if (existingIndex < 0) {
        existingIndex = editorTabs_->addTab(tab, tab->displayName());
    }
    if (focusTab && existingIndex >= 0) {
        editorTabs_->setCurrentIndex(existingIndex);
    }

    connectMapEditorTabUiSignals(tab);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    updateTabTitle(tab);
    refreshWorkspaceModeSwitcher();
    persistOpenDocuments();
}

void MainWindow::focusDetachedMapEditorTab(const QString &canonicalPath)
{
    if (QMainWindow *window = detachedMapWindowsByPath_.value(canonicalPath)) {
        window->showNormal();
        window->raise();
        window->activateWindow();
    }
}

TherionStudio::MapEditorTab *MainWindow::detachedMapEditorTabForPath(const QString &canonicalPath) const
{
    if (QMainWindow *window = detachedMapWindowsByPath_.value(canonicalPath)) {
        if (auto *detachedWindow = dynamic_cast<DetachedMapWindow *>(window)) {
            return detachedWindow->mapTab();
        }
    }
    return nullptr;
}

QList<TherionStudio::MapEditorTab *> MainWindow::detachedMapEditorTabs() const
{
    QList<TherionStudio::MapEditorTab *> tabs;
    for (auto iterator = detachedMapPathsByTab_.constBegin(); iterator != detachedMapPathsByTab_.constEnd(); ++iterator) {
        if (iterator.key() != nullptr) {
            tabs.append(iterator.key());
        }
    }
    return tabs;
}

TherionStudio::TextEditorTab *MainWindow::currentTextTab() const
{
    return qobject_cast<TherionStudio::TextEditorTab *>(currentDocumentWidget());
}

QWidget *MainWindow::currentDocumentWidget() const
{
    QWidget *widget = editorTabs_->currentWidget();
    if (documentPathForWidget(widget).isEmpty()) {
        return nullptr;
    }

    return widget;
}

void MainWindow::updateTabTitle(QWidget *tabWidget)
{
    if (tabWidget == nullptr) {
        return;
    }

    const int tabIndex = editorTabs_->indexOf(tabWidget);
    if (tabIndex < 0) {
        return;
    }

    editorTabs_->setTabText(tabIndex, documentDisplayNameForWidget(tabWidget));
    editorTabs_->setTabToolTip(tabIndex, documentPathForWidget(tabWidget));
    if (currentDocumentWidget() == tabWidget) {
        refreshDocumentStatusWidgets();
    }

    persistOpenDocuments();
}

void MainWindow::showComingSoon(const QString &featureName)
{
    const QString message = tr("%1 is not implemented yet.").arg(featureName);
    statusBar()->showMessage(message, 3000);
    appendConsoleLine(message);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmCloseDirtyDocuments()) {
        event->ignore();
        return;
    }

    shuttingDown_ = true;
    persistSessionState();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    if ((event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)
        && QApplication::activeWindow() == this) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int editorModeShortcutIndex = editorModeShortcutIndexForKeyEvent(keyEvent);
        if (editorModeShortcutIndex != 0) {
            keyEvent->accept();
            if (event->type() == QEvent::KeyPress) {
                if (editorModeShortcutIndex == 1) {
                    triggerRawModeForActiveDocument();
                } else {
                    triggerSecondaryEditorModeForActiveDocument();
                }
            }
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}
