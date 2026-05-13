#include "MainWindow.h"

#include <QAction>
#include <QAbstractButton>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QFont>
#include <QDir>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QFrame>
#include <QFormLayout>
#include <QProcess>
#include <QSplitter>
#include <QStandardPaths>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPixmap>
#include <QStackedWidget>
#include <QHash>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QAbstractItemModel>

#include <functional>

#include "TextEditorTab.h"
#include "MapEditorTab.h"
#include "MainWindowDocumentHelpers.h"
#include "../core/ProjectStructureIndex.h"
#include "../core/DocumentFile.h"
#include "../core/TherionDocumentEditor.h"
#include "../core/SessionStore.h"

QHash<QString, QString> loadStructureNameOverridesFromJson(const QString &json)
{
    QHash<QString, QString> overrides;
    if (json.trimmed().isEmpty()) {
        return overrides;
    }

    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        return overrides;
    }

    const QJsonObject rootObject = document.object();
    for (auto iterator = rootObject.begin(); iterator != rootObject.end(); ++iterator) {
        const QString name = iterator.value().toString().trimmed();
        if (!name.isEmpty()) {
            overrides.insert(iterator.key(), name);
        }
    }

    return overrides;
}

QString structureNameOverridesToJson(const QHash<QString, QString> &overrides)
{
    QJsonObject rootObject;
    for (auto iterator = overrides.constBegin(); iterator != overrides.constEnd(); ++iterator) {
        rootObject.insert(iterator.key(), iterator.value());
    }

    return QString::fromUtf8(QJsonDocument(rootObject).toJson(QJsonDocument::Compact));
}

bool isProtectedMacUserFolder(const QString &path)
{
    const QString canonicalPath = QDir(path).absolutePath();
    const QStringList protectedRoots = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)};

    for (const QString &protectedRoot : protectedRoots) {
        if (protectedRoot.isEmpty()) {
            continue;
        }

        const QString normalizedProtectedRoot = QDir(protectedRoot).absolutePath();
        if (canonicalPath == normalizedProtectedRoot || canonicalPath.startsWith(normalizedProtectedRoot + QLatin1Char('/'))) {
            return true;
        }
    }

    return false;
}

QWidget *createCenteredMessage(const QString &title, const QString &body)
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

    layout->addStretch(1);

    return widget;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , editorTabs_(new QTabWidget(this))
    , projectModel_(new QFileSystemModel(this))
    , structureModel_(new QStandardItemModel(this))
    , mapObjectsModel_(new QStandardItemModel(this))
{
    setWindowTitle(tr("Therion Studio"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AnimatedDocks);

    projectModel_->setRootPath(QDir::rootPath());

    buildUi();
    restoreSessionState();

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
    editorTabs_->setTabsClosable(true);
    editorTabs_->setMovable(true);
    editorTabs_->setDocumentMode(true);
    connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabCloseRequested);
    connect(editorTabs_, &QTabWidget::currentChanged, this, [this](int) {
        rebuildMapObjectsTree();
        updateMapEditorActionState();
    });

    setCentralWidget(editorTabs_);

    buildStructureSidebar();
    buildConsole();
    buildMenus();

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("New &Window"), this, &MainWindow::createNewWindow, QKeySequence::New);
    fileMenu->addAction(tr("&Open Project..."), this, &MainWindow::openProject, QKeySequence::Open);
    fileMenu->addAction(tr("&Close Project"), this, &MainWindow::closeProject);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), this, &MainWindow::saveActiveDocument, QKeySequence::Save);
    fileMenu->addAction(tr("Save &All"), this, &MainWindow::saveAllDocuments, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Find"), this, [this]() { showFindBar(false); }, QKeySequence::Find);
    editMenu->addAction(tr("Find and &Replace"), this, [this]() { showFindBar(true); }, QKeySequence::Replace);
    openMapEditorAction_ = editMenu->addAction(tr("Open Current Document in &Map Editor"), this, &MainWindow::openCurrentDocumentInMapEditor);
    openMapEditorAction_->setEnabled(false);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    if (structureDock_ != nullptr) {
        viewMenu->addAction(structureDock_->toggleViewAction());
    }
    if (consoleDock_ != nullptr) {
        viewMenu->addAction(consoleDock_->toggleViewAction());
    }

    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(tr("New &Window"), this, &MainWindow::createNewWindow);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("About Therion Studio"), this, [this]() {
        QMessageBox::information(this,
                                 tr("About Therion Studio"),
                                 tr("Therion Studio is a Qt-based editor for Therion projects."));
    });
}

void MainWindow::buildProjectBrowser()
{
    if (sidebarPages_ == nullptr || projectModel_ == nullptr) {
        return;
    }

    auto *projectPage = new QWidget(sidebarPages_);
    auto *projectLayout = new QVBoxLayout(projectPage);
    projectLayout->setContentsMargins(12, 12, 12, 12);
    projectLayout->setSpacing(8);

    auto *projectDescription = new QLabel(tr("Browse the files in the current project."), projectPage);
    projectDescription->setWordWrap(true);
    projectLayout->addWidget(projectDescription);

    projectTree_ = new QTreeView(projectPage);
    projectTree_->setModel(projectModel_);
    projectTree_->setRootIsDecorated(true);
    projectTree_->setAnimated(true);
    projectTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    projectTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    projectTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    projectTree_->setAlternatingRowColors(true);
    projectTree_->hideColumn(1);
    projectTree_->hideColumn(2);
    projectTree_->hideColumn(3);
    connect(projectTree_, &QTreeView::activated, this, &MainWindow::handleProjectTreeActivated);

    projectLayout->addWidget(projectTree_, 1);
    sidebarPages_->addWidget(projectPage);

    resetProjectBrowser();
}

void MainWindow::buildStructureSidebar()
{
    structureDock_ = new QDockWidget(tr("Sidebar"), this);
    structureDock_->setObjectName(QStringLiteral("SidebarDock"));
    structureDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    connect(structureDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible) {
            rememberSidebarWidth();
            return;
        }

        restoreSidebarWidth();
    });

    auto *container = new QWidget(structureDock_);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto *header = new QWidget(container);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 12, 12, 8);
    headerLayout->setSpacing(8);

    sidebarModeTabs_ = new QTabBar(header);
    sidebarModeTabs_->addTab(tr("Files"));
    sidebarModeTabs_->addTab(tr("Structure"));
    sidebarModeTabs_->addTab(tr("Map"));
    sidebarModeTabs_->setExpanding(true);
    connect(sidebarModeTabs_, &QTabBar::currentChanged, this, [this](int index) {
        setSidebarPane(static_cast<SidebarPane>(index));
    });

    sidebarCollapseButton_ = new QToolButton(header);
    sidebarCollapseButton_->setText(tr("Collapse"));
    sidebarCollapseButton_->setToolTip(tr("Collapse the sidebar"));
    connect(sidebarCollapseButton_, &QToolButton::clicked, this, [this]() {
        rememberSidebarWidth();
        if (structureDock_ != nullptr) {
            structureDock_->hide();
        }
    });

    headerLayout->addWidget(sidebarModeTabs_, 1);
    headerLayout->addWidget(sidebarCollapseButton_);

    sidebarPages_ = new QStackedWidget(container);

    containerLayout->addWidget(header);
    containerLayout->addWidget(sidebarPages_, 1);

    buildProjectBrowser();

    auto *structurePage = new QWidget(sidebarPages_);
    auto *structureLayout = new QVBoxLayout(structurePage);
    structureLayout->setContentsMargins(12, 12, 12, 12);
    structureLayout->setSpacing(8);

    mapEditorSidebarSplitter_ = new QSplitter(Qt::Vertical, structurePage);
    mapEditorSidebarSplitter_->setChildrenCollapsible(false);

    structureTree_ = new QTreeView(mapEditorSidebarSplitter_);
    structureTree_->setModel(structureModel_);
    structureTree_->setRootIsDecorated(true);
    structureTree_->setAnimated(true);
    structureTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    structureTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    structureTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    structureTree_->setAlternatingRowColors(true);
    connect(structureTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &previous) {
        handleStructureSelectionChanged(current, previous, structureTree_);
    });
    connect(structureTree_, &QTreeView::activated, this, [this](const QModelIndex &index) {
        handleStructureItemActivated(index, structureTree_);
    });

    mapEditorSidebarSplitter_->addWidget(structureTree_);
    structureLayout->addWidget(mapEditorSidebarSplitter_, 1);
    sidebarPages_->addWidget(structurePage);

    buildInspector();

    auto *mapObjectsPage = new QWidget(sidebarPages_);
    auto *mapObjectsLayout = new QVBoxLayout(mapObjectsPage);
    mapObjectsLayout->setContentsMargins(12, 12, 12, 12);
    mapObjectsLayout->setSpacing(8);

    auto *mapObjectsDescription = new QLabel(tr("Objects in the active TH2 file are grouped by scrap."), mapObjectsPage);
    mapObjectsDescription->setWordWrap(true);
    mapObjectsLayout->addWidget(mapObjectsDescription);

    mapObjectsTree_ = new QTreeView(mapObjectsPage);
    mapObjectsTree_->setModel(mapObjectsModel_);
    mapObjectsTree_->setRootIsDecorated(true);
    mapObjectsTree_->setAnimated(true);
    mapObjectsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapObjectsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapObjectsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapObjectsTree_->setAlternatingRowColors(true);
    connect(mapObjectsTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &previous) {
        handleStructureSelectionChanged(current, previous, mapObjectsTree_);
    });
    connect(mapObjectsTree_, &QTreeView::activated, this, [this](const QModelIndex &index) {
        handleStructureItemActivated(index, mapObjectsTree_);
    });

    mapObjectsLayout->addWidget(mapObjectsTree_, 1);
    sidebarPages_->addWidget(mapObjectsPage);

    structureDock_->setWidget(container);
    addDockWidget(Qt::LeftDockWidgetArea, structureDock_);
    setSidebarPane(activeSidebarPane_);
}

void MainWindow::buildInspector()
{
    if (mapEditorSidebarSplitter_ == nullptr) {
        return;
    }

    auto *inspectorFrame = new QFrame(mapEditorSidebarSplitter_);
    inspectorFrame->setFrameShape(QFrame::StyledPanel);

    auto *inspectorLayout = new QVBoxLayout(inspectorFrame);
    inspectorLayout->setContentsMargins(12, 12, 12, 12);
    inspectorLayout->setSpacing(8);

    inspectorSummary_ = new QLabel(tr("Select a structure item to inspect it."), inspectorFrame);
    inspectorSummary_->setWordWrap(true);
    inspectorSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    inspectorLayout->addWidget(inspectorSummary_);

    auto *form = new QFormLayout;

    inspectorCategoryLabel_ = new QLabel(tr("Category"), inspectorFrame);
    inspectorCategoryValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorNameLabel_ = new QLabel(tr("Name"), inspectorFrame);
    inspectorNameEdit_ = new QLineEdit(inspectorFrame);
    inspectorNameEdit_->setEnabled(false);
    connect(inspectorNameEdit_, &QLineEdit::textEdited, this, &MainWindow::handleInspectorNameEdited);

    inspectorProjectValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorRelativePathValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorObjectKindValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorEditabilityValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorSourceValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorLineValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorValidationLabel_ = new QLabel(tr("Select a structure item to edit its metadata."), inspectorFrame);
    inspectorValidationLabel_->setWordWrap(true);
    inspectorValidationLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    form->addRow(inspectorCategoryLabel_, inspectorCategoryValue_);
    form->addRow(inspectorNameLabel_, inspectorNameEdit_);
    form->addRow(tr("Project"), inspectorProjectValue_);
    form->addRow(tr("Relative Path"), inspectorRelativePathValue_);
    form->addRow(tr("Object Kind"), inspectorObjectKindValue_);
    form->addRow(tr("Editability"), inspectorEditabilityValue_);
    form->addRow(tr("Source File"), inspectorSourceValue_);
    form->addRow(tr("Line"), inspectorLineValue_);

    inspectorLayout->addLayout(form);
    inspectorLayout->addWidget(inspectorValidationLabel_);

    auto *buttonRow = new QHBoxLayout;
    inspectorOpenSourceButton_ = new QPushButton(tr("Open Source"), inspectorFrame);
    inspectorOpenSourceButton_->setEnabled(false);
    connect(inspectorOpenSourceButton_, &QPushButton::clicked, this, &MainWindow::openSelectedStructureSource);

    inspectorApplyButton_ = new QPushButton(tr("Apply"), inspectorFrame);
    inspectorApplyButton_->setEnabled(false);
    connect(inspectorApplyButton_, &QPushButton::clicked, this, &MainWindow::handleInspectorApplyTriggered);

    buttonRow->addWidget(inspectorOpenSourceButton_);
    buttonRow->addWidget(inspectorApplyButton_);
    buttonRow->addStretch(1);
    inspectorLayout->addLayout(buttonRow);

    mapEditorSidebarSplitter_->addWidget(inspectorFrame);
    mapEditorSidebarSplitter_->setStretchFactor(0, 3);
    mapEditorSidebarSplitter_->setStretchFactor(1, 2);
    mapEditorSidebarSplitter_->setCollapsible(1, true);
}

void MainWindow::setSidebarPane(SidebarPane pane)
{
    if (sidebarPages_ == nullptr) {
        return;
    }

    if (pane == SidebarPane::MapEditor && !currentDocumentSupportsMapPane()) {
        pane = lastNonMapSidebarPane_;
    }

    activeSidebarPane_ = pane;
    if (pane != SidebarPane::MapEditor) {
        lastNonMapSidebarPane_ = pane;
    }

    sidebarPages_->setCurrentIndex(static_cast<int>(pane));

    if (sidebarModeTabs_ != nullptr && sidebarModeTabs_->currentIndex() != static_cast<int>(pane)) {
        QSignalBlocker blocker(sidebarModeTabs_);
        sidebarModeTabs_->setCurrentIndex(static_cast<int>(pane));
    }

    updateSidebarModeTabIcons(sidebarModeTabs_, static_cast<int>(pane));
}

void MainWindow::buildConsole()
{
    consoleDock_ = new QDockWidget(tr("Console"), this);
    consoleDock_->setObjectName(QStringLiteral("ConsoleDock"));

    auto *widget = new QWidget(consoleDock_);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);

    therionExecutableEdit_ = new QLineEdit(widget);
    therionExecutableEdit_->setPlaceholderText(tr("therion"));
    therionExecutableEdit_->setText(TherionStudio::SessionStore::therionExecutablePath().trimmed().isEmpty()
                                       ? QStringLiteral("therion")
                                       : TherionStudio::SessionStore::therionExecutablePath().trimmed());

    therionWorkingDirectoryEdit_ = new QLineEdit(widget);
    therionWorkingDirectoryEdit_->setPlaceholderText(tr("Defaults to the current project root"));
    therionWorkingDirectoryEdit_->setText(TherionStudio::SessionStore::therionWorkingDirectory().trimmed());

    therionArgumentsEdit_ = new QLineEdit(widget);
    therionArgumentsEdit_->setPlaceholderText(tr("Additional Therion command-line options"));
    therionArgumentsEdit_->setText(TherionStudio::SessionStore::therionArguments().trimmed());

    form->addRow(tr("Executable"), therionExecutableEdit_);
    form->addRow(tr("Working Directory"), therionWorkingDirectoryEdit_);
    form->addRow(tr("Arguments"), therionArgumentsEdit_);
    layout->addLayout(form);

    therionStatusLabel_ = new QLabel(tr("Idle"), widget);
    therionStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(therionStatusLabel_);

    auto *buttonRow = new QHBoxLayout;
    therionRunButton_ = new QPushButton(tr("Run Therion"), widget);
    therionStopButton_ = new QPushButton(tr("Stop"), widget);
    therionStopButton_->setEnabled(false);
    therionResetWorkingDirectoryButton_ = new QPushButton(tr("Use Project Root"), widget);

    connect(therionRunButton_, &QPushButton::clicked, this, &MainWindow::runTherion);
    connect(therionStopButton_, &QPushButton::clicked, this, &MainWindow::stopTherion);
    connect(therionResetWorkingDirectoryButton_, &QPushButton::clicked, this, [this]() {
        therionWorkingDirectoryEdit_->setText(projectRootPath_);
    });

    buttonRow->addWidget(therionRunButton_);
    buttonRow->addWidget(therionStopButton_);
    buttonRow->addWidget(therionResetWorkingDirectoryButton_);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    consoleView_ = new QPlainTextEdit(widget);
    consoleView_->setReadOnly(true);
    consoleView_->setPlaceholderText(tr("Therion runner output will appear here."));
    layout->addWidget(consoleView_, 1);

    therionProcess_ = new QProcess(this);
    connect(therionProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::handleTherionProcessReadyReadStandardOutput);
    connect(therionProcess_, &QProcess::readyReadStandardError, this, &MainWindow::handleTherionProcessReadyReadStandardError);
    connect(therionProcess_, &QProcess::finished, this, &MainWindow::handleTherionProcessFinished);
    connect(therionProcess_, &QProcess::errorOccurred, this, &MainWindow::handleTherionProcessError);

    consoleDock_->setWidget(widget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);
    appendConsoleLine(tr("Therion Studio shell initialized."));
    updateTherionRunnerState();
}

void MainWindow::restoreSessionState()
{
    const QByteArray geometry = TherionStudio::SessionStore::mainWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray state = TherionStudio::SessionStore::mainWindowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    const QString lastProjectPath = TherionStudio::SessionStore::lastProjectPath();
    if (!lastProjectPath.isEmpty() && QDir(lastProjectPath).exists() && !isProtectedMacUserFolder(lastProjectPath)) {
        projectRootPath_ = lastProjectPath;
        projectModel_->setRootPath(projectRootPath_);
        projectTree_->setRootIndex(projectModel_->index(projectRootPath_));
        appendConsoleLine(tr("Restored project root %1").arg(projectRootPath_));
        if (therionWorkingDirectoryEdit_ != nullptr && therionWorkingDirectoryEdit_->text().trimmed().isEmpty()) {
            therionWorkingDirectoryEdit_->setText(projectRootPath_);
        }
        loadStructureNameOverrides();
        syncOpenDocumentsToProjectRoot();
        rebuildStructureSidebar();
    } else if (!lastProjectPath.isEmpty() && QDir(lastProjectPath).exists()) {
        appendConsoleLine(tr("Skipped automatic project restore for protected folder %1").arg(lastProjectPath));
    }

    restoreOpenDocuments();
}

void MainWindow::persistSessionState()
{
    TherionStudio::SessionStore::setMainWindowGeometry(saveGeometry());
    TherionStudio::SessionStore::setMainWindowState(saveState());
    TherionStudio::SessionStore::setLastProjectPath(projectRootPath_);
    TherionStudio::SessionStore::setTherionExecutablePath(therionExecutableEdit_ != nullptr ? therionExecutableEdit_->text().trimmed() : QString());
    TherionStudio::SessionStore::setTherionWorkingDirectory(therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString());
    TherionStudio::SessionStore::setTherionArguments(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString());
    saveStructureNameOverrides();
    persistOpenDocuments();
}

void MainWindow::restoreOpenDocuments()
{
    const QStringList openDocumentPaths = TherionStudio::SessionStore::openDocumentPaths();
    if (openDocumentPaths.isEmpty()) {
        return;
    }

    const QString activeDocumentPath = TherionStudio::SessionStore::activeDocumentPath();

    for (const QString &documentPath : openDocumentPaths) {
        if (documentPath.isEmpty() || isProtectedMacUserFolder(documentPath)) {
            continue;
        }

        if (QFileInfo(documentPath).suffix().toLower() == QStringLiteral("th2")) {
            openMapEditorTab(documentPath);
        } else {
            openTextTab(documentPath);
        }
    }

    if (!activeDocumentPath.isEmpty()) {
        QWidget *activeWidget = documentWidgetForFilePath(activeDocumentPath);
        if (activeWidget != nullptr) {
            editorTabs_->setCurrentWidget(activeWidget);
        }

        if (activeWidget != nullptr && documentPathForWidget(activeWidget).isEmpty()) {
            editorTabs_->setCurrentIndex(editorTabs_->count() - 1);
        }
    }
}

void MainWindow::persistOpenDocuments()
{
    QStringList documentPaths;

    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        const QString filePath = documentPathForWidget(tabWidget);
        if (filePath.isEmpty()) {
            continue;
        }

        documentPaths.append(filePath);
    }

    TherionStudio::SessionStore::setOpenDocumentPaths(documentPaths);

    QWidget *currentWidget = currentDocumentWidget();
    TherionStudio::SessionStore::setActiveDocumentPath(currentWidget != nullptr ? documentPathForWidget(currentWidget) : QString());
}

void MainWindow::addWelcomeTab()
{
    editorTabs_->addTab(createCenteredMessage(tr("Therion Studio"),
                                             tr("Open a project to begin working with Therion documents, maps, and structure views.")),
                        tr("Welcome"));
}

void MainWindow::createNewWindow()
{
    auto *window = new MainWindow;
    window->show();
}

void MainWindow::openProject()
{
    const QString projectPath = QFileDialog::getExistingDirectory(this,
                                                                  tr("Open Therion Project"),
                                                                  QString());
    if (projectPath.isEmpty()) {
        statusBar()->showMessage(tr("Open project cancelled"), 2000);
        return;
    }

    if (!QDir(projectPath).exists()) {
        QMessageBox::warning(this, tr("Open Project"), tr("The selected folder does not exist."));
        return;
    }

    projectRootPath_ = projectPath;
    projectModel_->setRootPath(projectRootPath_);
    projectTree_->setRootIndex(projectModel_->index(projectRootPath_));
    if (therionWorkingDirectoryEdit_ != nullptr && therionWorkingDirectoryEdit_->text().trimmed().isEmpty()) {
        therionWorkingDirectoryEdit_->setText(projectRootPath_);
    }
    loadStructureNameOverrides();
    syncOpenDocumentsToProjectRoot();
    TherionStudio::SessionStore::setLastProjectPath(projectRootPath_);
    rebuildStructureSidebar();
    statusBar()->showMessage(tr("Project root set to %1").arg(projectRootPath_), 3000);
    appendConsoleLine(tr("Project root set to %1").arg(projectRootPath_));
}

void MainWindow::closeProject()
{
    if (projectRootPath_.isEmpty()) {
        statusBar()->showMessage(tr("No project is open"), 2000);
        return;
    }

    if (!confirmCloseDirtyDocuments()) {
        return;
    }

    clearDocumentTabs();
    resetProjectBrowser();
    TherionStudio::SessionStore::setLastProjectPath(QString());
    persistOpenDocuments();
    rebuildStructureSidebar();
    statusBar()->showMessage(tr("Project closed"), 3000);
    appendConsoleLine(tr("Closed project %1").arg(projectRootPath_));
    projectRootPath_.clear();
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
    } else {
        openTextTab(filePath);
    }
}

void MainWindow::handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber)
{
    updateStructureSelectionFromEditorLocation(filePath, lineNumber);
    updateMapObjectSelectionFromEditorLocation(filePath, lineNumber);
}

void MainWindow::handleTabCloseRequested(int index)
{
    if (!confirmCloseTab(index)) {
        return;
    }

    QWidget *tabWidget = editorTabs_->widget(index);
    if (tabWidget == nullptr) {
        return;
    }

    editorTabs_->removeTab(index);
    tabWidget->deleteLater();

    if (editorTabs_->count() == 0) {
        addWelcomeTab();
    }
}

void MainWindow::resetProjectBrowser()
{
    const QString defaultRootPath = QDir::rootPath();
    projectModel_->setRootPath(defaultRootPath);
    projectTree_->setRootIndex(projectModel_->index(defaultRootPath));
}

#if 0
void MainWindow::rebuildStructureSidebar()
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Structure")});

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        auto *rootItem = new QStandardItem(tr("Open a project to populate the survey hierarchy"));
        rootItem->setEditable(false);
        structureModel_->appendRow(rootItem);
        projectStructureSummary_ = tr("Open a project to view its survey hierarchy summary.");
        inspectorSummary_->setText(projectStructureSummary_);
        structureTree_->expandAll();
        return;
    }

    QString errorMessage;
    const QVector<TherionStudio::ProjectStructureEntry> entries = TherionStudio::ProjectStructureIndex::scanProject(projectRootPath_, &errorMessage);
    if (!errorMessage.isEmpty()) {
        appendConsoleLine(errorMessage);
    }

    QHash<QString, int> categoryCounts;
    int totalItems = 0;
    int rootSurveyCount = 0;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        categoryCounts[entry.category] += 1;
        ++totalItems;
        if (entry.category == QStringLiteral("Surveys") && entry.depth == 0) {
            ++rootSurveyCount;
        }
    }

    projectStructureSummary_ = tr("Project structure summary: %1")
                                   .arg(formatProjectStructureSummary(categoryCounts, totalItems, rootSurveyCount));

    auto *projectItem = new QStandardItem(QFileInfo(projectRootPath_).fileName().isEmpty()
                                              ? projectRootPath_
                                              : QFileInfo(projectRootPath_).fileName());
    projectItem->setEditable(false);
    projectItem->setData(projectRootPath_, SourceFileRole);
    projectItem->setData(0, LineNumberRole);
    projectItem->setData(QStringLiteral("Project"), CategoryRole);
    projectItem->setData(projectItem->text(), NameRole);
    structureModel_->appendRow(projectItem);

    auto *summaryItem = new QStandardItem(projectStructureSummary_);
    summaryItem->setEditable(false);
    projectItem->appendRow(summaryItem);

    if (entries.isEmpty()) {
        auto *emptyItem = new QStandardItem(tr("No survey hierarchy was found in the selected project"));
        emptyItem->setEditable(false);
        projectItem->appendRow(emptyItem);
        inspectorSummary_->setText(projectStructureSummary_);
    } else {
        QVector<QStandardItem *> parentStack;
        for (const TherionStudio::ProjectStructureEntry &entry : entries) {
            while (parentStack.size() > entry.depth) {
                parentStack.removeLast();
            }

            QString entryName = entry.name;
            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = ::structureOverrideKey(projectRootPath_, entry.sourceFile, entry.lineNumber);
                entryName = structureNameOverrides_.value(overrideKey, entry.name);
            }

            TherionStudio::ProjectStructureEntry displayEntry = entry;
            displayEntry.name = entryName;

            auto *entryItem = createIndexedItem(structureBrowserItemText(displayEntry),
                                                entry.sourceFile,
                                                entry.lineNumber,
                                                entry.category,
                                                entryName);

            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = ::structureOverrideKey(projectRootPath_, entry.sourceFile, entry.lineNumber);
                entryItem->setData(overrideKey, Qt::UserRole + 5);
            }

            QStandardItem *parentItem = parentStack.isEmpty() ? projectItem : parentStack.last();
            parentItem->appendRow(entryItem);
            parentStack.append(entryItem);
        }

        inspectorSummary_->setText(projectStructureSummary_);
    }

    structureTree_->expandAll();
}

void MainWindow::rebuildMapObjectsTree()
{
    if (mapObjectsModel_ == nullptr) {
        return;
    }

    mapObjectsModel_->clear();
    mapObjectsModel_->setHorizontalHeaderLabels({tr("Objects")});

    QWidget *tabWidget = currentDocumentWidget();
    const QString filePath = tabWidget != nullptr ? documentPathForWidget(tabWidget) : QString();
    if (tabWidget == nullptr || !filePath.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        auto *placeholderItem = new QStandardItem(tr("Open a TH2 document to browse its objects by scrap"));
        placeholderItem->setEditable(false);
        mapObjectsModel_->appendRow(placeholderItem);
        return;
    }

    QString text;
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        text = textTab->text();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        text = mapTab->text();
    }

    const QVector<TherionStudio::ProjectStructureEntry> entries = TherionStudio::ProjectStructureIndex::scanTh2Objects(filePath, text);
    if (entries.isEmpty()) {
        auto *placeholderItem = new QStandardItem(tr("No TH2 scraps, points, lines, or areas were found in the current document"));
        placeholderItem->setEditable(false);
        mapObjectsModel_->appendRow(placeholderItem);
        return;
    }

    QVector<QStandardItem *> parentStack;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        while (parentStack.size() > entry.depth) {
            parentStack.removeLast();
        }

        auto *entryItem = createIndexedItem(mapObjectItemText(entry), entry.sourceFile, entry.lineNumber, entry.category, entry.name);
        QStandardItem *parentItem = parentStack.isEmpty() ? mapObjectsModel_->invisibleRootItem() : parentStack.last();
        parentItem->appendRow(entryItem);
        parentStack.append(entryItem);
    }

    mapObjectsTree_->expandAll();
    updateMapObjectSelectionFromEditorLocation(filePath, documentCurrentLineNumberForWidget(tabWidget));
}

void MainWindow::handleStructureSelectionChanged(const QModelIndex &current, const QModelIndex &, QTreeView *sourceTree)
{
    updateInspectorFromStructureItem(current);

    if (!current.isValid()) {
        return;
    }

    if (isStructureContainerIndex(current)) {
        return;
    }

    const QString sourceFile = current.data(SourceFileRole).toString();
    const int lineNumber = current.data(LineNumberRole).toInt();
    if (sourceFile.isEmpty()) {
        return;
    }

    QWidget *tabWidget = QFileInfo(sourceFile).suffix().toLower() == QStringLiteral("th2")
        ? static_cast<QWidget *>(openMapEditorTab(sourceFile))
        : static_cast<QWidget *>(openTextTab(sourceFile));
    if (tabWidget != nullptr && lineNumber > 0) {
        documentGoToLineForWidget(tabWidget, lineNumber);
    }
}

void MainWindow::handleStructureItemActivated(const QModelIndex &index, QTreeView *sourceTree)
{
    QTreeView *tree = sourceTree != nullptr ? sourceTree : structureTree_;
    if (!index.isValid()) {
        return;
    }

    if (isStructureContainerIndex(index)) {
        if (tree != nullptr && tree->isExpanded(index)) {
            tree->collapse(index);
        } else {
            if (tree != nullptr) {
                tree->expand(index);
            }
        }

        const QModelIndex sourceIndex = firstStructureSourceIndex(index);
        if (sourceIndex.isValid()) {
            if (tree != nullptr) {
                tree->setCurrentIndex(sourceIndex);
            }
        }
        return;
    }

    handleStructureSelectionChanged(index, QModelIndex(), tree);
}

void MainWindow::updateInspectorFromStructureItem(const QModelIndex &index)
{
    if (!index.isValid()) {
        inspectorSummary_->setText(projectStructureSummary_.isEmpty() ? tr("Open a project to view structure details.") : projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Category"));
        inspectorNameLabel_->setText(tr("Name"));
        inspectorCategoryValue_->setText(tr("-"));
        inspectorNameEdit_->setText(QString());
        inspectorNameEdit_->setEnabled(false);
        inspectorProjectValue_->setText(tr("-"));
        inspectorRelativePathValue_->setText(tr("-"));
        inspectorObjectKindValue_->setText(tr("-"));
        inspectorEditabilityValue_->setText(tr("-"));
        inspectorSourceValue_->setText(tr("-"));
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("Select a structure item to edit its metadata."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setEnabled(false);
        selectedStructureIndex_ = QPersistentModelIndex();
        selectedStructureSourceFile_.clear();
        selectedStructureName_.clear();
        selectedStructureLine_ = 0;
        return;
    }

    const QString category = index.data(CategoryRole).toString();
    const QString name = index.data(NameRole).toString();
    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();

    const bool isProjectNode = category == QStringLiteral("Project");
    const bool isContainerNode = sourceFile.isEmpty() || lineNumber <= 0 || isProjectNode;

    if (isProjectNode) {
        selectedStructureIndex_ = QPersistentModelIndex(index);
        inspectorSummary_->setText(projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Node Type"));
        inspectorNameLabel_->setText(tr("Project"));
        inspectorCategoryValue_->setText(tr("Project"));
        inspectorNameEdit_->setEnabled(false);
        inspectorNameEdit_->setText(name);
        inspectorSourceValue_->setText(QDir(projectRootPath_).absolutePath());
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("Project nodes summarize the current project and are not directly editable."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setText(tr("Open First Item"));
        inspectorOpenSourceButton_->setEnabled(selectedStructureIndex_.isValid() ? firstStructureSourceIndex(selectedStructureIndex_).isValid() : false);
        return;
    }

    if (category.isEmpty() || name.isEmpty()) {
        inspectorSummary_->setText(projectStructureSummary_.isEmpty() ? tr("Open a project to view structure details.") : projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Category"));
        inspectorNameLabel_->setText(tr("Name"));
        inspectorCategoryValue_->setText(tr("-"));
        inspectorNameEdit_->setText(QString());
        inspectorNameEdit_->setEnabled(false);
        inspectorProjectValue_->setText(tr("-"));
        inspectorRelativePathValue_->setText(tr("-"));
        inspectorObjectKindValue_->setText(tr("-"));
        inspectorEditabilityValue_->setText(tr("-"));
        inspectorSourceValue_->setText(tr("-"));
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("The selected row is not a structure object that can be edited."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setText(tr("Open Source"));
        inspectorOpenSourceButton_->setEnabled(false);
        selectedStructureIndex_ = QPersistentModelIndex();
        selectedStructureSourceFile_.clear();
        selectedStructureName_.clear();
        selectedStructureLine_ = 0;
        return;
    }

    selectedStructureIndex_ = QPersistentModelIndex(index);
    selectedStructureSourceFile_ = sourceFile;
    selectedStructureName_ = name;
    selectedStructureLine_ = lineNumber;

    const QString objectKind = inspectorObjectKindLabel(category);
    const QString nameLabel = inspectorNameLabelForCategory(category);
    const QString relativePath = QDir(projectRootPath_).relativeFilePath(sourceFile);
    const QString projectName = QFileInfo(projectRootPath_).fileName().isEmpty() ? projectRootPath_ : QFileInfo(projectRootPath_).fileName();

    inspectorSummary_->setText(tr("%1: %2\nSource: %3:%4")
                                   .arg(objectKind)
                                   .arg(name)
                                   .arg(relativePath)
                                   .arg(lineNumber));
    inspectorCategoryLabel_->setText(tr("Object Kind"));
    inspectorNameLabel_->setText(nameLabel);
    inspectorCategoryValue_->setText(category);
    inspectorNameEdit_->setEnabled(!isContainerNode);
    inspectorNameEdit_->setText(name);
    inspectorProjectValue_->setText(projectName);
    inspectorRelativePathValue_->setText(relativePath);
    inspectorObjectKindValue_->setText(objectKind);
    inspectorEditabilityValue_->setText(isContainerNode ? tr("Container") : tr("Editable in-memory"));
    inspectorSourceValue_->setText(sourceFile);
    inspectorLineValue_->setText(lineNumber > 0 ? QString::number(lineNumber) : tr("-"));
    inspectorOpenSourceButton_->setText(isContainerNode ? tr("Open First Item") : tr("Open Source"));
    inspectorOpenSourceButton_->setEnabled(isContainerNode ? firstStructureSourceIndex(index).isValid() : true);
    inspectorValidationLabel_->setText(isContainerNode
                                           ? tr("This row groups child objects; use Open First Item or double-click to drill down.")
                                           : tr("Editing is currently in-memory for the selected structure entry."));
    inspectorApplyButton_->setEnabled(false);
}

void MainWindow::handleInspectorNameEdited(const QString &text)
{
    const bool canApply = selectedStructureIndex_.isValid() && !text.trimmed().isEmpty() && text.trimmed() != selectedStructureName_;
    if (!text.trimmed().isEmpty()) {
        inspectorValidationLabel_->setText(tr("Editing is currently in-memory for the selected structure entry."));
    } else {
        inspectorValidationLabel_->setText(tr("Name is required."));
    }
    inspectorApplyButton_->setEnabled(canApply);
}

void MainWindow::handleInspectorApplyTriggered()
{
    if (!selectedStructureIndex_.isValid()) {
        return;
    }

    const QString newName = inspectorNameEdit_->text().trimmed();
    if (newName.isEmpty()) {
        inspectorValidationLabel_->setText(tr("Name is required."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    if (newName == selectedStructureName_) {
        inspectorValidationLabel_->setText(tr("No changes to apply."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    const QString category = selectedStructureIndex_.data(CategoryRole).toString();
    const QString sourceFile = selectedStructureIndex_.data(SourceFileRole).toString();
    const int lineNumber = selectedStructureIndex_.data(LineNumberRole).toInt();
    if (category.isEmpty() || sourceFile.isEmpty()) {
        inspectorValidationLabel_->setText(tr("The selected structure item cannot be updated."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    QString errorMessage;
    QWidget *tabWidget = documentWidgetForFilePath(sourceFile);
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        if (!textTab->rewriteStructureEntryName(lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!textTab->save(&errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        if (!mapTab->rewriteStructureEntryName(lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!mapTab->save(&errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    } else {
        QString contents;
        if (!TherionStudio::DocumentFile::readUtf8TextFile(sourceFile, &contents, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!TherionStudio::TherionDocumentEditor::rewriteStructureEntryName(&contents, lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!TherionStudio::DocumentFile::writeUtf8TextFile(sourceFile, contents, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    }

    const QString overrideKey = ::structureOverrideKey(projectRootPath_, sourceFile, lineNumber);
    structureNameOverrides_.remove(overrideKey);
    saveStructureNameOverrides();
    rebuildStructureSidebar();
    rebuildMapObjectsTree();
    updateStructureSelectionFromEditorLocation(sourceFile, lineNumber);
    updateMapObjectSelectionFromEditorLocation(sourceFile, lineNumber);
    inspectorSummary_->setText(tr("%1: %2\nSource: %3:%4")
                                   .arg(inspectorObjectKindLabel(category))
                                   .arg(newName)
                                   .arg(QDir(projectRootPath_).relativeFilePath(sourceFile))
                                   .arg(lineNumber));
    inspectorValidationLabel_->setText(tr("Updated the selected structure item in the source document."));
    inspectorApplyButton_->setEnabled(false);
}

void MainWindow::openSelectedStructureSource()
{
    if (!selectedStructureIndex_.isValid()) {
        return;
    }

    QModelIndex sourceIndex = selectedStructureIndex_;
    if (isStructureContainerIndex(sourceIndex)) {
        sourceIndex = firstStructureSourceIndex(sourceIndex);
    }

    if (!sourceIndex.isValid()) {
        return;
    }

    const QString sourceFile = sourceIndex.data(SourceFileRole).toString();
    const int lineNumber = sourceIndex.data(LineNumberRole).toInt();
    if (sourceFile.isEmpty()) {
        return;
    }

    QWidget *tabWidget = nullptr;
    if (QFileInfo(sourceFile).suffix().toLower() == QStringLiteral("th2")) {
        tabWidget = openMapEditorTab(sourceFile);
    } else {
        tabWidget = openTextTab(sourceFile);
    }

    if (tabWidget != nullptr && lineNumber > 0) {
        documentGoToLineForWidget(tabWidget, lineNumber);
    }
}

void MainWindow::openCurrentDocumentInMapEditor()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    const QString filePath = documentPathForWidget(tabWidget);
    if (!filePath.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        QMessageBox::information(this, tr("Open in Map Editor"), tr("Open a .th2 document first."));
        return;
    }

    openMapEditorTab(filePath);
}

void MainWindow::updateMapEditorActionState()
{
    const bool hasTh2Document = currentDocumentSupportsMapPane();

    if (openMapEditorAction_ != nullptr) {
        openMapEditorAction_->setEnabled(hasTh2Document);
    }

    if (sidebarModeTabs_ != nullptr) {
        const int mapPaneIndex = static_cast<int>(SidebarPane::MapEditor);
        sidebarModeTabs_->setTabEnabled(mapPaneIndex, hasTh2Document);

        if (!hasTh2Document && activeSidebarPane_ == SidebarPane::MapEditor) {
            setSidebarPane(lastNonMapSidebarPane_);
            return;
        }

        updateSidebarModeTabIcons(sidebarModeTabs_, static_cast<int>(activeSidebarPane_));
    }
}

bool MainWindow::currentDocumentSupportsMapPane() const
{
    QWidget *tabWidget = currentDocumentWidget();
    return tabWidget != nullptr && documentPathForWidget(tabWidget).endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive);
}

void MainWindow::rememberSidebarWidth()
{
    if (structureDock_ == nullptr || !structureDock_->isVisible()) {
        return;
    }

    sidebarExpandedWidth_ = qMax(220, structureDock_->width());
}

void MainWindow::restoreSidebarWidth()
{
    if (structureDock_ == nullptr || sidebarExpandedWidth_ <= 0) {
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        if (structureDock_ == nullptr || !structureDock_->isVisible()) {
            return;
        }

        resizeDocks({structureDock_}, {sidebarExpandedWidth_}, Qt::Horizontal);
    });
}

QModelIndex MainWindow::firstStructureSourceIndex(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() == nullptr) {
        return QModelIndex();
    }

    if (!index.data(SourceFileRole).toString().isEmpty() && index.data(LineNumberRole).toInt() > 0) {
        return index;
    }

    const QAbstractItemModel *model = index.model();
    const int rowCount = model->rowCount(index);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = model->index(row, 0, index);
        const QModelIndex sourceIndex = firstStructureSourceIndex(childIndex);
        if (sourceIndex.isValid()) {
            return sourceIndex;
        }
    }

    return QModelIndex();
}

bool MainWindow::isStructureContainerIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    const QString category = index.data(CategoryRole).toString();
    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();
    return category == QStringLiteral("Project") || sourceFile.isEmpty() || lineNumber <= 0;
}

QModelIndex MainWindow::findStructureIndexForSourceLocation(const QString &filePath, int lineNumber) const
{
    if (structureModel_ == nullptr || filePath.isEmpty() || lineNumber <= 0) {
        return QModelIndex();
    }

    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    QModelIndex bestIndex;
    int bestLineNumber = -1;

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = structureModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = structureModel_->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const QString childSourceFile = childIndex.data(SourceFileRole).toString();
            const QString candidateSourceFile = QFileInfo(childSourceFile).canonicalFilePath().isEmpty()
                                                    ? QFileInfo(childSourceFile).absoluteFilePath()
                                                    : QFileInfo(childSourceFile).canonicalFilePath();
            const int candidateLineNumber = childIndex.data(LineNumberRole).toInt();

            if (candidateSourceFile == canonicalPath && candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };

    visitNode(QModelIndex());
    return bestIndex;
}

void MainWindow::updateStructureSelectionFromEditorLocation(const QString &filePath, int lineNumber)
{
    const QModelIndex targetIndex = findStructureIndexForSourceLocation(filePath, lineNumber);
    if (!targetIndex.isValid() || targetIndex == structureTree_->currentIndex()) {
        return;
    }

    if (structureTree_->selectionModel() != nullptr) {
        QSignalBlocker blocker(structureTree_->selectionModel());
        structureTree_->setCurrentIndex(targetIndex);
    } else {
        structureTree_->setCurrentIndex(targetIndex);
    }

    structureTree_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
}

QModelIndex MainWindow::findMapObjectIndexForSourceLocation(const QString &filePath, int lineNumber) const
{
    if (mapObjectsModel_ == nullptr || filePath.isEmpty() || lineNumber <= 0) {
        return QModelIndex();
    }

    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    QModelIndex bestIndex;
    int bestLineNumber = -1;

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = mapObjectsModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = mapObjectsModel_->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const QString childSourceFile = childIndex.data(SourceFileRole).toString();
            const QString candidateSourceFile = QFileInfo(childSourceFile).canonicalFilePath().isEmpty()
                                                    ? QFileInfo(childSourceFile).absoluteFilePath()
                                                    : QFileInfo(childSourceFile).canonicalFilePath();
            const int candidateLineNumber = childIndex.data(LineNumberRole).toInt();

            if (candidateSourceFile == canonicalPath && candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };

    visitNode(QModelIndex());
    return bestIndex;
}

void MainWindow::updateMapObjectSelectionFromEditorLocation(const QString &filePath, int lineNumber)
{
    if (mapObjectsTree_ == nullptr) {
        return;
    }

    const QModelIndex targetIndex = findMapObjectIndexForSourceLocation(filePath, lineNumber);
    if (!targetIndex.isValid() || targetIndex == mapObjectsTree_->currentIndex()) {
        return;
    }

    if (mapObjectsTree_->selectionModel() != nullptr) {
        QSignalBlocker blocker(mapObjectsTree_->selectionModel());
        mapObjectsTree_->setCurrentIndex(targetIndex);
    } else {
        mapObjectsTree_->setCurrentIndex(targetIndex);
    }

    mapObjectsTree_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
    updateInspectorFromStructureItem(targetIndex);
}
#endif

void MainWindow::openCurrentDocumentInMapEditor()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    const QString filePath = documentPathForWidget(tabWidget);
    if (!filePath.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        QMessageBox::information(this, tr("Open in Map Editor"), tr("Open a .th2 document first."));
        return;
    }

    openMapEditorTab(filePath);
}

QString MainWindow::structureOverrideKey(const QString &sourceFile, int lineNumber) const
{
    return QStringLiteral("%1|%2|%3").arg(QDir(projectRootPath_).absolutePath(), sourceFile).arg(lineNumber);
}

void MainWindow::loadStructureNameOverrides()
{
    structureNameOverrides_ = loadStructureNameOverridesFromJson(TherionStudio::SessionStore::structureNameOverrides());
}

void MainWindow::saveStructureNameOverrides()
{
    TherionStudio::SessionStore::setStructureNameOverrides(structureNameOverridesToJson(structureNameOverrides_));
}

void MainWindow::saveActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        showComingSoon(tr("Save"));
        return;
    }

    QString errorMessage;
    if (!documentSaveForWidget(tabWidget, &errorMessage)) {
        QMessageBox::warning(this, tr("Save"), errorMessage);
        return;
    }

    updateTabTitle(tabWidget);
    statusBar()->showMessage(tr("Saved %1").arg(documentDisplayNameForWidget(tabWidget)), 2000);
}

void MainWindow::saveAllDocuments()
{
    bool hadFailure = false;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget == nullptr) {
            continue;
        }

        QString errorMessage;
        if (!documentIsDirtyForWidget(tabWidget) || documentSaveForWidget(tabWidget, &errorMessage)) {
            updateTabTitle(tabWidget);
            continue;
        }

        hadFailure = true;
        QMessageBox::warning(this, tr("Save All"), errorMessage);
        break;
    }

    if (!hadFailure) {
        statusBar()->showMessage(tr("All open documents saved"), 2000);
    }
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

TherionStudio::TextEditorTab *MainWindow::openTextTab(const QString &filePath)
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(index));
        if (existingTab != nullptr && existingTab->filePath() == canonicalPath) {
            existingTab->setProjectRootPath(projectRootPath_);
            editorTabs_->setCurrentIndex(index);
            return existingTab;
        }
    }

    auto *tab = new TherionStudio::TextEditorTab;
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    const int tabIndex = editorTabs_->addTab(tab, tab->displayName());
    editorTabs_->setCurrentIndex(tabIndex);

    connect(tab, &TherionStudio::TextEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
    });

    connect(tab, &TherionStudio::TextEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
    });

    connect(tab, &TherionStudio::TextEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::TextEditorTab::documentTextChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
        }
    });

    handleTextEditorCurrentLineChanged(tab->filePath(), tab->currentLineNumber());

    updateTabTitle(tab);
    persistOpenDocuments();
    appendConsoleLine(tr("Opened %1").arg(canonicalPath));
    return tab;
}

TherionStudio::MapEditorTab *MainWindow::openMapEditorTab(const QString &filePath)
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index));
        if (existingTab != nullptr && existingTab->filePath() == canonicalPath) {
            existingTab->setProjectRootPath(projectRootPath_);
            editorTabs_->setCurrentIndex(index);
            return existingTab;
        }
    }

    auto *tab = new TherionStudio::MapEditorTab;
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    const int tabIndex = editorTabs_->addTab(tab, tab->displayName());
    editorTabs_->setCurrentIndex(tabIndex);

    connect(tab, &TherionStudio::MapEditorTab::titleChanged, this, [this, tab]() { updateTabTitle(tab); });
    connect(tab, &TherionStudio::MapEditorTab::dirtyStateChanged, this, [this, tab](bool) { updateTabTitle(tab); });
    connect(tab, &TherionStudio::MapEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::MapEditorTab::documentTextChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
        }
    });

    handleTextEditorCurrentLineChanged(tab->filePath(), tab->currentLineNumber());

    updateTabTitle(tab);
    persistOpenDocuments();
    appendConsoleLine(tr("Opened %1").arg(canonicalPath));
    return tab;
}

QWidget *MainWindow::documentWidgetForFilePath(const QString &filePath) const
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (documentPathForWidget(tabWidget) == canonicalPath) {
            return tabWidget;
        }
    }

    return nullptr;
}

void MainWindow::syncOpenDocumentsToProjectRoot()
{
    for (int index = 0; index < editorTabs_->count(); ++index) {
        documentSetProjectRootPathForWidget(editorTabs_->widget(index), projectRootPath_);
    }
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

    persistSessionState();
    QMainWindow::closeEvent(event);
}

bool MainWindow::confirmCloseTab(int index)
{
    QWidget *tabWidget = editorTabs_->widget(index);
    if (tabWidget == nullptr || !documentIsDirtyForWidget(tabWidget)) {
        return true;
    }

    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Warning);
    prompt.setWindowTitle(tr("Unsaved Changes"));
    prompt.setText(tr("The document %1 has unsaved changes.").arg(documentDisplayNameForWidget(tabWidget)));
    prompt.setInformativeText(tr("Do you want to save your changes before closing this tab?"));
    QPushButton *saveButton = prompt.addButton(tr("Save"), QMessageBox::AcceptRole);
    QPushButton *discardButton = prompt.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    prompt.addButton(QMessageBox::Cancel);
    prompt.setDefaultButton(saveButton);
    prompt.exec();

    if (prompt.clickedButton() == saveButton) {
        QString errorMessage;
        if (!documentSaveForWidget(tabWidget, &errorMessage)) {
            QMessageBox::warning(this, tr("Save"), errorMessage);
            return false;
        }

        updateTabTitle(tabWidget);
        return true;
    }

    if (prompt.clickedButton() == discardButton) {
        return true;
    }

    return false;
}

bool MainWindow::confirmCloseDirtyDocuments()
{
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (!confirmCloseTab(index)) {
            return false;
        }
    }

    return true;
}

void MainWindow::clearDocumentTabs()
{
    while (editorTabs_->count() > 0) {
        QWidget *tabWidget = editorTabs_->widget(0);
        editorTabs_->removeTab(0);
        if (tabWidget != nullptr) {
            tabWidget->deleteLater();
        }
    }

    addWelcomeTab();
}

void MainWindow::appendConsoleLine(const QString &line)
{
    if (consoleView_ == nullptr) {
        return;
    }

    consoleView_->appendPlainText(line);
}

QString MainWindow::resolvedTherionWorkingDirectory() const
{
    const QString typedWorkingDirectory = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString();
    if (!typedWorkingDirectory.isEmpty()) {
        return QDir(typedWorkingDirectory).absolutePath();
    }

    if (!projectRootPath_.isEmpty()) {
        return QDir(projectRootPath_).absolutePath();
    }

    return QString();
}

void MainWindow::updateTherionRunnerState()
{
    const bool isRunning = therionProcess_ != nullptr && therionProcess_->state() != QProcess::NotRunning;
    if (therionRunButton_ != nullptr) {
        therionRunButton_->setEnabled(!isRunning);
    }
    if (therionStopButton_ != nullptr) {
        therionStopButton_->setEnabled(isRunning);
    }
    if (therionExecutableEdit_ != nullptr) {
        therionExecutableEdit_->setEnabled(!isRunning);
    }
    if (therionWorkingDirectoryEdit_ != nullptr) {
        therionWorkingDirectoryEdit_->setEnabled(!isRunning);
    }
    if (therionArgumentsEdit_ != nullptr) {
        therionArgumentsEdit_->setEnabled(!isRunning);
    }
}

void MainWindow::runTherion()
{
    if (therionProcess_ == nullptr) {
        appendConsoleLine(tr("Therion runner is unavailable."));
        return;
    }

    if (therionProcess_->state() != QProcess::NotRunning) {
        appendConsoleLine(tr("Therion is already running."));
        return;
    }

    const QString executablePath = therionExecutableEdit_ != nullptr && !therionExecutableEdit_->text().trimmed().isEmpty()
                                       ? therionExecutableEdit_->text().trimmed()
                                       : QStringLiteral("therion");
    const QString workingDirectory = resolvedTherionWorkingDirectory();
    if (workingDirectory.isEmpty()) {
        QMessageBox::warning(this,
                             tr("Run Therion"),
                             tr("Open a project or set a working directory before running Therion."));
        return;
    }

    if (!QDir(workingDirectory).exists()) {
        QMessageBox::warning(this,
                             tr("Run Therion"),
                             tr("The selected working directory does not exist."));
        return;
    }

    const QString argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString();
    const QStringList arguments = argumentsText.isEmpty() ? QStringList() : QProcess::splitCommand(argumentsText);

    therionProcess_->setWorkingDirectory(workingDirectory);
    therionProcess_->start(executablePath, arguments);

    appendConsoleLine(tr("Running %1 %2 in %3")
                          .arg(executablePath)
                          .arg(argumentsText)
                          .arg(workingDirectory));
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(tr("Starting Therion..."));
    }
    updateTherionRunnerState();
}

void MainWindow::stopTherion()
{
    if (therionProcess_ == nullptr || therionProcess_->state() == QProcess::NotRunning) {
        appendConsoleLine(tr("Therion is not running."));
        return;
    }

    appendConsoleLine(tr("Stopping Therion runner..."));
    therionProcess_->kill();
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(tr("Stopping Therion..."));
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionProcessReadyReadStandardOutput()
{
    if (therionProcess_ == nullptr || consoleView_ == nullptr) {
        return;
    }

    const QString output = QString::fromLocal8Bit(therionProcess_->readAllStandardOutput());
    if (!output.isEmpty()) {
        consoleView_->appendPlainText(output);
    }
}

void MainWindow::handleTherionProcessReadyReadStandardError()
{
    if (therionProcess_ == nullptr || consoleView_ == nullptr) {
        return;
    }

    const QString output = QString::fromLocal8Bit(therionProcess_->readAllStandardError());
    if (!output.isEmpty()) {
        consoleView_->appendPlainText(tr("[stderr] %1").arg(output));
    }
}

void MainWindow::handleTherionProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QString statusText = exitStatus == QProcess::NormalExit
        ? tr("Therion finished with exit code %1.").arg(exitCode)
        : tr("Therion crashed while running.");
    appendConsoleLine(statusText);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(statusText);
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    if (therionProcess_ == nullptr) {
        return;
    }

    const QString errorText = tr("Therion runner error: %1").arg(therionProcess_->errorString());
    appendConsoleLine(errorText);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(errorText);
    }
    updateTherionRunnerState();
}
