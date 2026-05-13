#pragma once

#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QProcess>

class QLabel;
class QFileSystemModel;
class QCloseEvent;
class QDockWidget;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QStandardItemModel;
class QStandardItem;
class QTabBar;
class QTabWidget;
class QToolButton;
class QTreeView;
class QModelIndex;

namespace TherionStudio
{
class TextEditorTab;
class MapEditorTab;
class SessionStore;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void createNewWindow();
    void openProject();
    void closeProject();
    void handleProjectTreeActivated(const QModelIndex &index);
    void handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber);
    void openCurrentDocumentInMapEditor();
    void handleTabCloseRequested(int index);
    void saveActiveDocument();
    void saveAllDocuments();
    void runTherion();
    void stopTherion();
    void handleTherionProcessReadyReadStandardOutput();
    void handleTherionProcessReadyReadStandardError();
    void handleTherionProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleTherionProcessError(QProcess::ProcessError error);
    void showComingSoon(const QString &featureName);

private:
    enum class SidebarPane
    {
        FileBrowser = 0,
        StructureBrowser = 1,
        MapEditor = 2
    };

    void buildUi();
    void buildMenus();
    void buildProjectBrowser();
    void buildStructureSidebar();
    void buildInspector();
    void buildConsole();
    void addWelcomeTab();
    void restoreSessionState();
    void persistSessionState();
    void restoreOpenDocuments();
    void persistOpenDocuments();
    void resetProjectBrowser();
    void rebuildStructureSidebar();
    void rebuildMapObjectsTree();
    void setSidebarPane(SidebarPane pane);
    void syncOpenDocumentsToProjectRoot();
    QWidget *documentWidgetForFilePath(const QString &filePath) const;
    void handleStructureSelectionChanged(const QModelIndex &current, const QModelIndex &previous, QTreeView *sourceTree);
    void handleStructureItemActivated(const QModelIndex &index, QTreeView *sourceTree);
    void updateInspectorFromStructureItem(const QModelIndex &index);
    void handleInspectorNameEdited(const QString &text);
    void handleInspectorApplyTriggered();
    void openSelectedStructureSource();
    bool confirmCloseTab(int index);
    bool confirmCloseDirtyDocuments();
    void clearDocumentTabs();
    TherionStudio::TextEditorTab *openTextTab(const QString &filePath);
    TherionStudio::MapEditorTab *openMapEditorTab(const QString &filePath);
    QWidget *currentDocumentWidget() const;
    TherionStudio::TextEditorTab *currentTextTab() const;
    void showFindBar(bool replaceMode);
    void updateTabTitle(QWidget *tabWidget);
    void appendConsoleLine(const QString &line);
    void updateTherionRunnerState();
    void updateMapEditorActionState();
    bool currentDocumentSupportsMapPane() const;
    QString resolvedTherionWorkingDirectory() const;
    void rememberSidebarWidth();
    void restoreSidebarWidth();
    QModelIndex firstStructureSourceIndex(const QModelIndex &index) const;
    bool isStructureContainerIndex(const QModelIndex &index) const;
    QModelIndex findStructureIndexForSourceLocation(const QString &filePath, int lineNumber) const;
    QModelIndex findMapObjectIndexForSourceLocation(const QString &filePath, int lineNumber) const;
    void updateStructureSelectionFromEditorLocation(const QString &filePath, int lineNumber);
    void updateMapObjectSelectionFromEditorLocation(const QString &filePath, int lineNumber);
    QString structureOverrideKey(const QString &sourceFile, int lineNumber) const;
    void loadStructureNameOverrides();
    void saveStructureNameOverrides();

    QTabWidget *editorTabs_ = nullptr;
    QTreeView *projectTree_ = nullptr;
    QTreeView *structureTree_ = nullptr;
    QTreeView *mapObjectsTree_ = nullptr;
    QDockWidget *structureDock_ = nullptr;
    QSplitter *mapEditorSidebarSplitter_ = nullptr;
    QStackedWidget *sidebarPages_ = nullptr;
    QTabBar *sidebarModeTabs_ = nullptr;
    QToolButton *sidebarCollapseButton_ = nullptr;
    QDockWidget *consoleDock_ = nullptr;
    QAction *openMapEditorAction_ = nullptr;
    QPlainTextEdit *consoleView_ = nullptr;
    QLineEdit *therionExecutableEdit_ = nullptr;
    QLineEdit *therionWorkingDirectoryEdit_ = nullptr;
    QLineEdit *therionArgumentsEdit_ = nullptr;
    QLabel *therionStatusLabel_ = nullptr;
    QPushButton *therionRunButton_ = nullptr;
    QPushButton *therionStopButton_ = nullptr;
    QPushButton *therionResetWorkingDirectoryButton_ = nullptr;
    QProcess *therionProcess_ = nullptr;
    QLabel *inspectorSummary_ = nullptr;
    QLabel *inspectorCategoryLabel_ = nullptr;
    QLabel *inspectorNameLabel_ = nullptr;
    QLabel *inspectorCategoryValue_ = nullptr;
    QLineEdit *inspectorNameEdit_ = nullptr;
    QLabel *inspectorProjectValue_ = nullptr;
    QLabel *inspectorRelativePathValue_ = nullptr;
    QLabel *inspectorObjectKindValue_ = nullptr;
    QLabel *inspectorEditabilityValue_ = nullptr;
    QLabel *inspectorSourceValue_ = nullptr;
    QLabel *inspectorLineValue_ = nullptr;
    QLabel *inspectorValidationLabel_ = nullptr;
    QPushButton *inspectorOpenSourceButton_ = nullptr;
    QPushButton *inspectorApplyButton_ = nullptr;
    QFileSystemModel *projectModel_ = nullptr;
    QStandardItemModel *structureModel_ = nullptr;
    QStandardItemModel *mapObjectsModel_ = nullptr;
    QString projectRootPath_;
    QString projectStructureSummary_;
    QPersistentModelIndex selectedStructureIndex_;
    QString selectedStructureSourceFile_;
    QString selectedStructureName_;
    QHash<QString, QString> structureNameOverrides_;
    SidebarPane activeSidebarPane_ = SidebarPane::FileBrowser;
    SidebarPane lastNonMapSidebarPane_ = SidebarPane::FileBrowser;
    int sidebarExpandedWidth_ = 320;
    int selectedStructureLine_ = 0;
};
