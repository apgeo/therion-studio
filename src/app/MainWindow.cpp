#include "MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QFont>
#include <QDir>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QLabel>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QHash>
#include <QSizePolicy>
#include <QSplitter>
#include <QLineEdit>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTreeView>
#include <QKeySequence>
#include <QWidget>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>

#include "TextEditorTab.h"
#include "MapEditorTab.h"
#include "MainWindowDocumentHelpers.h"
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

QString quickUserManualMarkdown()
{
    return QStringLiteral(
        "# Therion Studio Quick User Manual\n"
        "\n"
        "## Core workflow\n"
        "\n"
        "1. Open a project with `File -> Open Project...`.\n"
        "2. Open `.th`/`.th2` files from the Files sidebar.\n"
        "3. Use Structure + Inspector to rename objects or toggle line flags.\n"
        "4. Use the map workspace for TH2 geometry edits and keep source text in sync.\n"
        "5. Save with `Save` or `Save All`.\n"
        "\n"
        "## Map workspace essentials\n"
        "\n"
        "- `Select`, `Point`, `Line`, `Freehand`, `Smart Trace`, `Area`\n"
        "- `Insert Scrap`, `Complete Draft`, `Undo`, `Redo`\n"
        "- `Zoom -`, `Zoom +`, `Fit`, `Fit + BG`\n"
        "- `Open Map in Window` to detach the map pane\n"
        "\n"
        "### Line-vertex shortcuts\n"
        "\n"
        "- Split selected segment: `Insert` or `I`\n"
        "- Remove selected middle anchor: `Delete` or `Backspace`\n"
        "- Toggle smooth/corner: `S`\n"
        "\n"
        "## Full manual\n"
        "\n"
        "Use `Help -> User Manual (Full)` when you need complete workflow details.\n");
}

QStringList fullUserManualPathCandidates()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    return {
        QDir(cwd).absoluteFilePath(QStringLiteral("docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../docs/USER_MANUAL.md"))};
}

QString resolveFullUserManualPath()
{
    const QStringList candidates = fullUserManualPathCandidates();
    for (const QString &candidatePath : candidates) {
        const QFileInfo info(candidatePath);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

QString loadUtf8TextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

void showMarkdownDialog(QWidget *parent,
                        const QString &title,
                        const QString &markdown,
                        const QString &sourceLabel = QString())
{
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(title);
    dialog->resize(860, 680);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    if (!sourceLabel.trimmed().isEmpty()) {
        auto *sourceInfo = new QLabel(sourceLabel, dialog);
        sourceInfo->setWordWrap(true);
        sourceInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(sourceInfo);
    }

    auto *browser = new QTextBrowser(dialog);
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    browser->document()->setMarkdown(markdown);
    layout->addWidget(browser, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::close);
    layout->addWidget(buttons);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

QString canonicalDocumentPath(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString canonical = fileInfo.canonicalFilePath();
    return canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical;
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
        refreshTherionConfigDisplay();
        refreshMapBackgroundPanel();
        refreshDocumentStatusWidgets();
    });

    auto *mainContentHost = new QWidget(this);
    mainContentLayout_ = new QHBoxLayout(mainContentHost);
    mainContentLayout_->setContentsMargins(0, 0, 0, 0);
    mainContentLayout_->setSpacing(0);
    mainContentSplitter_ = new QSplitter(Qt::Horizontal, mainContentHost);
    mainContentSplitter_->setChildrenCollapsible(true);
    setCentralWidget(mainContentHost);

    buildStructureSidebar();
    mainContentSplitter_->addWidget(editorTabs_);
    mainContentLayout_->addWidget(mainContentSplitter_, 1);
    mainContentSplitter_->setCollapsible(0, true);
    mainContentSplitter_->setCollapsible(1, false);
    mainContentSplitter_->setStretchFactor(0, 0);
    mainContentSplitter_->setStretchFactor(1, 1);
    buildConsole();
    buildMenus();

    initializeDocumentStatusWidgets();
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::initializeDocumentStatusWidgets()
{
    if (statusBar() == nullptr || statusDocumentPathLabel_ != nullptr || statusDocumentEncodingLabel_ != nullptr) {
        return;
    }

    statusDocumentPathLabel_ = new QLabel(statusBar());
    statusDocumentPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusDocumentPathLabel_->setMinimumWidth(200);
    statusDocumentPathLabel_->setMaximumWidth(560);
    statusDocumentPathLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    statusDocumentEncodingLabel_ = new QLabel(statusBar());
    statusDocumentEncodingLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusDocumentEncodingLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusDocumentEncodingLabel_->setMinimumWidth(130);

    statusBar()->addPermanentWidget(statusDocumentPathLabel_, 0);
    statusBar()->addPermanentWidget(statusDocumentEncodingLabel_, 0);
    refreshDocumentStatusWidgets();
}

void MainWindow::refreshDocumentStatusWidgets()
{
    if (statusDocumentPathLabel_ == nullptr || statusDocumentEncodingLabel_ == nullptr) {
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    QString pathText = tr("No file open");
    QString encodingText;

    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        pathText = textTab->statusPathText();
        encodingText = textTab->statusEncodingText();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        pathText = mapTab->statusPathText();
        encodingText = mapTab->statusEncodingText();
    }

    const int maxPathWidth = statusDocumentPathLabel_->maximumWidth();
    const QFontMetrics metrics(statusDocumentPathLabel_->fontMetrics());
    const QString elidedPathText = metrics.elidedText(pathText, Qt::ElideMiddle, maxPathWidth);
    statusDocumentPathLabel_->setText(elidedPathText);
    statusDocumentPathLabel_->setToolTip(pathText);
    if (encodingText.trimmed().isEmpty()) {
        statusDocumentEncodingLabel_->clear();
    } else {
        statusDocumentEncodingLabel_->setText(tr("Encoding: %1").arg(encodingText));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    refreshDocumentStatusWidgets();
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("New &Window"), this, &MainWindow::createNewWindow, QKeySequence::New);
    openProjectAction_ = fileMenu->addAction(tr("&Open Project..."), this, &MainWindow::openProject, QKeySequence::Open);
    closeProjectAction_ = fileMenu->addAction(tr("&Close Project"), this, &MainWindow::closeProject);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), this, &MainWindow::saveActiveDocument, QKeySequence::Save);
    fileMenu->addAction(tr("Save &All"), this, &MainWindow::saveAllDocuments, QKeySequence::SaveAs);
    fileMenu->addAction(tr("&Close"), this, &MainWindow::closeActiveTab);
    fileMenu->addAction(tr("Close All Tabs"),
                        this,
                        &MainWindow::closeAllTabs,
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Find"), this, [this]() { showFindBar(false); }, QKeySequence::Find);
    editMenu->addAction(tr("Find and &Replace"), this, [this]() { showFindBar(true); }, QKeySequence::Replace);

    QMenu *mapMenu = menuBar()->addMenu(tr("&Map"));
    openMapEditorAction_ = mapMenu->addAction(tr("Open Current Document in &Map Editor"),
                                              this,
                                              &MainWindow::openCurrentDocumentInMapEditor,
                                              QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_G));
    openMapEditorAction_->setEnabled(false);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *toggleSidebarAction = viewMenu->addAction(tr("Show Sidebar"));
    toggleSidebarAction->setCheckable(true);
    toggleSidebarAction->setChecked(true);
    toggleSidebarAction->setStatusTip(tr("Show or hide the sidebar panel"));
    connect(toggleSidebarAction, &QAction::toggled, this, [this](bool visible) {
        if (sidebarContainer_ == nullptr) {
            return;
        }
        if (!visible) {
            rememberSidebarWidth();
            sidebarContainer_->setVisible(false);
            if (sidebarContentContainer_ != nullptr) {
                sidebarContentContainer_->setVisible(false);
            }
            return;
        }
        sidebarContainer_->setVisible(true);
        if (sidebarContentContainer_ != nullptr) {
            sidebarContentContainer_->setVisible(true);
        }
        if (sidebarCollapsed_) {
            sidebarCollapsed_ = false;
            setSidebarCollapsed(true);
            return;
        }
        restoreSidebarWidth();
    });
    QAction *showConsolePaneAction = viewMenu->addAction(tr("Show Console"));
    showConsolePaneAction->setStatusTip(tr("Switch the sidebar to the console pane"));
    connect(showConsolePaneAction, &QAction::triggered, this, [this]() {
        if (sidebarContainer_ != nullptr) {
            sidebarContainer_->setVisible(true);
            if (sidebarContentContainer_ != nullptr) {
                sidebarContentContainer_->setVisible(true);
            }
            if (sidebarCollapsed_) {
                setSidebarCollapsed(false);
            }
            restoreSidebarWidth();
        }
        setSidebarPane(SidebarPane::Console);
    });

    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(tr("New &Window"), this, &MainWindow::createNewWindow);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("Quick User Manual"), this, [this]() {
        showMarkdownDialog(this, tr("Quick User Manual"), quickUserManualMarkdown());
    });
    helpMenu->addAction(tr("User Manual (Full)"), this, [this]() {
        const QString manualPath = resolveFullUserManualPath();
        if (manualPath.isEmpty()) {
            showMarkdownDialog(this,
                               tr("User Manual (Full)"),
                               quickUserManualMarkdown(),
                               tr("Full manual file `docs/USER_MANUAL.md` was not found in expected locations. Showing quick manual instead."));
            return;
        }

        const QString manualText = loadUtf8TextFile(manualPath);
        if (manualText.trimmed().isEmpty()) {
            showMarkdownDialog(this,
                               tr("User Manual (Full)"),
                               quickUserManualMarkdown(),
                               tr("Failed to load `%1`. Showing quick manual instead.").arg(QDir::toNativeSeparators(manualPath)));
            return;
        }

        showMarkdownDialog(this,
                           tr("User Manual (Full)"),
                           manualText,
                           tr("Source: %1").arg(QDir::toNativeSeparators(manualPath)));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("About Therion Studio"), this, [this]() {
        QMessageBox::information(this,
                                 tr("About Therion Studio"),
                                 tr("Therion Studio is a Qt-based editor for Therion projects."));
    });

    updateProjectActionState();
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

    refreshTherionConfigDisplay();
    updateProjectActionState();
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

    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }
        const QString filePath = documentPathForWidget(detachedTab);
        if (filePath.isEmpty() || documentPaths.contains(filePath)) {
            continue;
        }
        documentPaths.append(filePath);
    }

    TherionStudio::SessionStore::setOpenDocumentPaths(documentPaths);

    QString activeDocumentPath;
    for (auto iterator = detachedMapWindowsByPath_.constBegin(); iterator != detachedMapWindowsByPath_.constEnd(); ++iterator) {
        QMainWindow *window = iterator.value();
        if (window != nullptr && window->isActiveWindow()) {
            activeDocumentPath = iterator.key();
            break;
        }
    }
    if (activeDocumentPath.isEmpty()) {
        QWidget *currentWidget = currentDocumentWidget();
        activeDocumentPath = currentWidget != nullptr ? documentPathForWidget(currentWidget) : QString();
    }

    TherionStudio::SessionStore::setActiveDocumentPath(activeDocumentPath);
}

void MainWindow::addWelcomeTab()
{
    editorTabs_->addTab(createCenteredMessage(tr("Therion Studio"),
                                             tr("Open a project to begin working with Therion documents, maps, and structure views.")),
                        tr("Welcome"));
    refreshDocumentStatusWidgets();
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
    refreshTherionConfigDisplay();
    updateProjectActionState();
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
    refreshTherionConfigDisplay();
    updateProjectActionState();
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

    refreshDocumentStatusWidgets();
    persistOpenDocuments();
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
    bool closedAny = false;
    for (int index = editorTabs_->count() - 1; index >= 0; --index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget == nullptr) {
            continue;
        }

        if (documentPathForWidget(tabWidget).isEmpty()) {
            continue;
        }

        if (!confirmCloseTab(index)) {
            break;
        }

        editorTabs_->removeTab(index);
        tabWidget->deleteLater();
        closedAny = true;
    }

    const QList<TherionStudio::MapEditorTab *> detachedTabs = detachedMapEditorTabs();
    for (TherionStudio::MapEditorTab *detachedTab : detachedTabs) {
        if (detachedTab == nullptr) {
            continue;
        }
        if (!confirmCloseDocumentWidget(detachedTab)) {
            break;
        }

        const QString path = detachedMapPathsByTab_.value(detachedTab);
        if (path.isEmpty()) {
            continue;
        }

        if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
            const bool wasClearingTabs = clearingDocumentTabs_;
            clearingDocumentTabs_ = true;
            window->close();
            clearingDocumentTabs_ = wasClearingTabs;
            closedAny = true;
        }
    }

    if (editorTabs_->count() == 0) {
        addWelcomeTab();
    }

    refreshDocumentStatusWidgets();
    persistOpenDocuments();
    if (closedAny) {
        statusBar()->showMessage(tr("Closed all open document tabs"), 2000);
    }
}

void MainWindow::resetProjectBrowser()
{
    const QString defaultRootPath = QDir::rootPath();
    projectModel_->setRootPath(defaultRootPath);
    projectTree_->setRootIndex(projectModel_->index(defaultRootPath));
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
    QList<QWidget *> allDocuments;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        allDocuments.append(editorTabs_->widget(index));
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        allDocuments.append(detachedTab);
    }

    for (QWidget *tabWidget : allDocuments) {
        if (tabWidget == nullptr || documentPathForWidget(tabWidget).isEmpty()) {
            continue;
        }
        QString errorMessage;
        if (!documentIsDirtyForWidget(tabWidget) || documentSaveForWidget(tabWidget, &errorMessage)) {
            if (editorTabs_->indexOf(tabWidget) >= 0) {
                updateTabTitle(tabWidget);
            }
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
    const QString canonicalPath = canonicalDocumentPath(filePath);

    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(index));
        if (existingTab != nullptr && existingTab->filePath() == canonicalPath) {
            existingTab->setProjectRootPath(projectRootPath_);
            editorTabs_->setCurrentIndex(index);
            refreshDocumentStatusWidgets();
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
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });

    connect(tab, &TherionStudio::TextEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
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
    refreshDocumentStatusWidgets();
    persistOpenDocuments();
    appendConsoleLine(tr("Opened %1").arg(canonicalPath));
    return tab;
}

TherionStudio::MapEditorTab *MainWindow::openMapEditorTab(const QString &filePath)
{
    const QString canonicalPath = canonicalDocumentPath(filePath);

    if (auto *detachedTab = detachedMapEditorTabForPath(canonicalPath); detachedTab != nullptr) {
        detachedTab->setProjectRootPath(projectRootPath_);
        focusDetachedMapEditorTab(canonicalPath);
        refreshDocumentStatusWidgets();
        return detachedTab;
    }

    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index));
        if (existingTab != nullptr && existingTab->filePath() == canonicalPath) {
            existingTab->setProjectRootPath(projectRootPath_);
            editorTabs_->setCurrentIndex(index);
            connect(existingTab,
                    &TherionStudio::MapEditorTab::openDedicatedWindowRequested,
                    this,
                    &MainWindow::handleMapEditorDetachRequested,
                    Qt::UniqueConnection);
            refreshDocumentStatusWidgets();
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
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::MapEditorTab::documentTextChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayersChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
        }
    });
    connect(tab,
            &TherionStudio::MapEditorTab::openDedicatedWindowRequested,
            this,
            &MainWindow::handleMapEditorDetachRequested,
            Qt::UniqueConnection);

    handleTextEditorCurrentLineChanged(tab->filePath(), tab->currentLineNumber());

    updateTabTitle(tab);
    refreshDocumentStatusWidgets();
    persistOpenDocuments();
    appendConsoleLine(tr("Opened %1").arg(canonicalPath));
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

    connect(tab,
            &TherionStudio::MapEditorTab::openDedicatedWindowRequested,
            this,
            &MainWindow::handleMapEditorDetachRequested,
            Qt::UniqueConnection);
    tab->setProjectRootPath(projectRootPath_);
    updateTabTitle(tab);
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

bool MainWindow::confirmCloseTab(int index)
{
    QWidget *tabWidget = editorTabs_->widget(index);
    return confirmCloseDocumentWidget(tabWidget);
}

bool MainWindow::confirmCloseDocumentWidget(QWidget *documentWidget)
{
    if (documentWidget == nullptr || documentPathForWidget(documentWidget).isEmpty() || !documentIsDirtyForWidget(documentWidget)) {
        return true;
    }

    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Warning);
    prompt.setWindowTitle(tr("Unsaved Changes"));
    prompt.setText(tr("The document %1 has unsaved changes.").arg(documentDisplayNameForWidget(documentWidget)));
    prompt.setInformativeText(tr("Do you want to save your changes before closing this document?"));
    QPushButton *saveButton = prompt.addButton(tr("Save"), QMessageBox::AcceptRole);
    QPushButton *discardButton = prompt.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    prompt.addButton(QMessageBox::Cancel);
    prompt.setDefaultButton(saveButton);
    prompt.exec();

    if (prompt.clickedButton() == saveButton) {
        QString errorMessage;
        if (!documentSaveForWidget(documentWidget, &errorMessage)) {
            QMessageBox::warning(this, tr("Save"), errorMessage);
            return false;
        }

        if (editorTabs_->indexOf(documentWidget) >= 0) {
            updateTabTitle(documentWidget);
        }
        return true;
    }

    if (prompt.clickedButton() == discardButton) {
        return true;
    }

    return false;
}

bool MainWindow::confirmCloseDirtyDocuments()
{
    QList<QWidget *> documents;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget != nullptr && !documentPathForWidget(tabWidget).isEmpty()) {
            documents.append(tabWidget);
        }
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab != nullptr && !documentPathForWidget(detachedTab).isEmpty()) {
            documents.append(detachedTab);
        }
    }

    for (QWidget *documentWidget : documents) {
        if (!confirmCloseDocumentWidget(documentWidget)) {
            return false;
        }
    }

    return true;
}

void MainWindow::clearDocumentTabs()
{
    clearingDocumentTabs_ = true;

    const auto detachedPaths = detachedMapWindowsByPath_.keys();
    for (const QString &path : detachedPaths) {
        if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
            if (window->isVisible()) {
                window->close();
            } else {
                window->deleteLater();
            }
        }
    }
    detachedMapWindowsByPath_.clear();
    detachedMapPathsByTab_.clear();

    while (editorTabs_->count() > 0) {
        QWidget *tabWidget = editorTabs_->widget(0);
        editorTabs_->removeTab(0);
        if (tabWidget != nullptr) {
            tabWidget->deleteLater();
        }
    }

    addWelcomeTab();
    clearingDocumentTabs_ = false;
    refreshDocumentStatusWidgets();
}

void MainWindow::updateProjectActionState()
{
    const bool hasOpenProject = !projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists();
    if (openProjectAction_ != nullptr) {
        openProjectAction_->setEnabled(!hasOpenProject);
    }
    if (closeProjectAction_ != nullptr) {
        closeProjectAction_->setEnabled(hasOpenProject);
    }
}
