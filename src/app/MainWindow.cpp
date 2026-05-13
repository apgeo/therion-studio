#include "MainWindow.h"

#include <QAction>
#include <QDockWidget>
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
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QHash>
#include <QLineEdit>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeView>
#include <QWidget>
#include <QJsonDocument>
#include <QJsonObject>

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
    openProjectAction_ = fileMenu->addAction(tr("&Open Project..."), this, &MainWindow::openProject, QKeySequence::Open);
    closeProjectAction_ = fileMenu->addAction(tr("&Close Project"), this, &MainWindow::closeProject);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), this, &MainWindow::saveActiveDocument, QKeySequence::Save);
    fileMenu->addAction(tr("Save &All"), this, &MainWindow::saveAllDocuments, QKeySequence::SaveAs);
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
    if (structureDock_ != nullptr) {
        QAction *toggleSidebarAction = structureDock_->toggleViewAction();
        toggleSidebarAction->setText(tr("Show Sidebar"));
        toggleSidebarAction->setStatusTip(tr("Show or hide the sidebar panel"));
        viewMenu->addAction(toggleSidebarAction);
    }
    QAction *showConsolePaneAction = viewMenu->addAction(tr("Show Console"));
    showConsolePaneAction->setStatusTip(tr("Switch the sidebar to the console pane"));
    connect(showConsolePaneAction, &QAction::triggered, this, [this]() {
        if (structureDock_ != nullptr) {
            structureDock_->show();
        }
        setSidebarPane(SidebarPane::Console);
    });

    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(tr("New &Window"), this, &MainWindow::createNewWindow);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
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
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayersChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
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
