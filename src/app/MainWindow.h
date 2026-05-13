#pragma once

#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QProcess>
#include <QPoint>

class QLabel;
class QFileSystemModel;
class QCloseEvent;
class QDoubleSpinBox;
class QDockWidget;
class QFrame;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSplitter;
class QStackedWidget;
class QStandardItemModel;
class QStandardItem;
class QTabWidget;
class QToolButton;
class QTreeView;
class QVBoxLayout;
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
    void handleProjectTreeContextMenuRequested(const QPoint &position);
    void handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber);
    void openCurrentDocumentInMapEditor();
    void handleTabCloseRequested(int index);
    void closeActiveTab();
    void closeAllTabs();
    void saveActiveDocument();
    void saveAllDocuments();
    void runTherion();
    void stopTherion();
    void browseTherionExecutable();
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
        MapEditor = 2,
        Console = 3
    };

    void buildUi();
    void buildMenus();
    void buildProjectBrowser();
    void buildStructureSidebar();
    void buildInspector();
    void buildMapBackgroundPanel(QWidget *parent, QVBoxLayout *parentLayout);
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
    void copyTherionConsoleOutput();
    void updateTherionRunnerState();
    void updateProjectActionState();
    void updateMapEditorActionState();
    bool currentDocumentSupportsMapPane() const;
    QString resolvedTherionWorkingDirectory() const;
    bool hasExplicitTherionConfigArgument() const;
    QString resolvedTherionConfigPath() const;
    void refreshTherionConfigDisplay();
    void rememberSidebarWidth();
    void restoreSidebarWidth();
    void setSidebarCollapsed(bool collapsed);
    void updateSidebarCollapseButton();
    void rememberConsoleHeight();
    void restoreConsoleHeight();
    void setConsoleCollapsed(bool collapsed);
    void updateConsoleCollapseButton();
    QModelIndex firstStructureSourceIndex(const QModelIndex &index) const;
    bool isStructureContainerIndex(const QModelIndex &index) const;
    QModelIndex findStructureIndexForSourceLocation(const QString &filePath, int lineNumber) const;
    QModelIndex findMapObjectIndexForSourceLocation(const QString &filePath, int lineNumber) const;
    void updateStructureSelectionFromEditorLocation(const QString &filePath, int lineNumber);
    void updateMapObjectSelectionFromEditorLocation(const QString &filePath, int lineNumber);
    QString structureOverrideKey(const QString &sourceFile, int lineNumber) const;
    void loadStructureNameOverrides();
    void saveStructureNameOverrides();
    void refreshMapBackgroundPanel();
    TherionStudio::MapEditorTab *currentMapEditorTab() const;

    QTabWidget *editorTabs_ = nullptr;
    QTreeView *projectTree_ = nullptr;
    QTreeView *structureTree_ = nullptr;
    QTreeView *mapObjectsTree_ = nullptr;
    QFrame *mapBackgroundPanel_ = nullptr;
    QListWidget *mapBackgroundLayersList_ = nullptr;
    QToolButton *mapBackgroundAddButton_ = nullptr;
    QPushButton *mapBackgroundRemoveButton_ = nullptr;
    QPushButton *mapBackgroundMoveUpButton_ = nullptr;
    QPushButton *mapBackgroundMoveDownButton_ = nullptr;
    QPushButton *mapBackgroundVisibilityButton_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosXSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosYSpin_ = nullptr;
    QPushButton *mapBackgroundNudgeLeftButton_ = nullptr;
    QPushButton *mapBackgroundNudgeRightButton_ = nullptr;
    QPushButton *mapBackgroundNudgeUpButton_ = nullptr;
    QPushButton *mapBackgroundNudgeDownButton_ = nullptr;
    QSlider *mapBackgroundOpacitySlider_ = nullptr;
    QSlider *mapBackgroundGammaSlider_ = nullptr;
    QPushButton *mapBackgroundOpacityResetButton_ = nullptr;
    QPushButton *mapBackgroundGammaResetButton_ = nullptr;
    QDockWidget *structureDock_ = nullptr;
    QSplitter *mapEditorSidebarSplitter_ = nullptr;
    QWidget *sidebarContentContainer_ = nullptr;
    QToolButton *sidebarFilesButton_ = nullptr;
    QToolButton *sidebarStructureButton_ = nullptr;
    QToolButton *sidebarMapButton_ = nullptr;
    QToolButton *sidebarConsoleButton_ = nullptr;
    QStackedWidget *sidebarPages_ = nullptr;
    QWidget *consoleSidebarPage_ = nullptr;
    QVBoxLayout *consoleSidebarPageLayout_ = nullptr;
    QToolButton *sidebarCollapseButton_ = nullptr;
    QDockWidget *consoleDock_ = nullptr;
    QToolButton *consoleCollapseButton_ = nullptr;
    QAction *openProjectAction_ = nullptr;
    QAction *closeProjectAction_ = nullptr;
    QAction *openMapEditorAction_ = nullptr;
    QPlainTextEdit *consoleView_ = nullptr;
    QLineEdit *therionExecutableEdit_ = nullptr;
    QPushButton *therionBrowseExecutableButton_ = nullptr;
    QLineEdit *therionWorkingDirectoryEdit_ = nullptr;
    QLineEdit *therionArgumentsEdit_ = nullptr;
    QLabel *therionRunPolicyLabel_ = nullptr;
    QLabel *therionConfigNameValue_ = nullptr;
    QLabel *therionConfigPathValue_ = nullptr;
    QLabel *therionStatusLabel_ = nullptr;
    QPushButton *therionRunButton_ = nullptr;
    QPushButton *therionStopButton_ = nullptr;
    QPushButton *therionResetWorkingDirectoryButton_ = nullptr;
    QPushButton *therionCopyOutputButton_ = nullptr;
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
    QString autoResolvedTherionWorkingDirectory_;
    SidebarPane activeSidebarPane_ = SidebarPane::FileBrowser;
    SidebarPane lastNonMapSidebarPane_ = SidebarPane::FileBrowser;
    int sidebarExpandedWidth_ = 320;
    int consoleExpandedHeight_ = 240;
    bool sidebarCollapsed_ = false;
    bool consoleCollapsed_ = false;
    bool updatingMapBackgroundPanel_ = false;
    int selectedStructureLine_ = 0;
};
