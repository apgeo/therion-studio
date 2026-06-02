#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QPoint>
#include <QProcess>
#include <QSet>

#include <memory>

#include "MainWindowTherionConsoleController.h"
#include "ProjectStructureScanner.h"
#include "../core/CommandCatalogStore.h"
#include "../core/ISessionStore.h"
#include "../core/ProjectStructureIndex.h"
#include "../core/QtFileSystem.h"

class QLabel;
class QAction;
class QComboBox;
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
class QHBoxLayout;
class QVBoxLayout;
class QModelIndex;
class QResizeEvent;
namespace TherionStudio
{
class TextEditorTab;
class MapEditorTab;
class TherionRunnerService;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    enum class SessionRestoreMode
    {
        RestoreSession,
        StartEmpty
    };

    explicit MainWindow(std::unique_ptr<TherionStudio::ISessionStore> sessionStore,
                        TherionStudio::CommandCatalogStore commandCatalogStore,
                        QWidget *parent = nullptr,
                        SessionRestoreMode restoreMode = SessionRestoreMode::RestoreSession);
    explicit MainWindow(TherionStudio::ISessionStore &sessionStore,
                        TherionStudio::CommandCatalogStore commandCatalogStore,
                        QWidget *parent = nullptr,
                        SessionRestoreMode restoreMode = SessionRestoreMode::RestoreSession);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void createNewWindow();
    void openProject();
    void closeProject();
    void handleProjectTreeActivated(const QModelIndex &index);
    void handleProjectTreeContextMenuRequested(const QPoint &position);
    void handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber);
    void handleTabCloseRequested(int index);
    void closeActiveTab();
    void closeAllTabs();
    void saveActiveDocument();
    void saveAllDocuments();
    void runTherion();
    void runTherionProjectConfig();
    void runTherionCurrentConfig();
    void stopTherion();
    void showSettingsDialog();
    void browseTherionTargetConfig();
    void browseTherionWorkingDirectoryOverride();
    void handleTherionRunnerStandardOutput(const QString &output);
    void handleTherionRunnerStandardError(const QString &output);
    void handleTherionRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleTherionRunnerError(const QString &errorText);
    void handleTherionRunnerStateChanged(bool running);
    void showComingSoon(const QString &featureName);
    void handleMapEditorDetachRequested(TherionStudio::MapEditorTab *tab);

private:
    enum class SidebarPane
    {
        FileBrowser = 0,
        StructureBrowser = 1,
        Console = 2
    };

    void buildUi();
    void buildMenus();
    void buildProjectBrowser();
    void buildStructureSidebar();
    void buildMapBackgroundPanel(QWidget *parent, QVBoxLayout *parentLayout);
    void buildConsole();
    void addWelcomeTab();
    void restoreSessionState();
    void persistSessionState();
    void restoreOpenDocuments();
    void persistOpenDocuments();
    void resetProjectBrowser();
    void refreshProjectBrowserView(const QString &focusPath = QString());
    void rebuildStructureSidebar();
    void requestStructureSidebarRebuild();
    void handleStructureSidebarScanFinished(const TherionStudio::ProjectStructureScanner::Result &result);
    void applyStructureSidebarIndex(const TherionStudio::ProjectIndexSnapshot &projectIndex);
    void showStructureSidebarMessage(const QString &message);
    bool activateStructureSidebarAction(const QString &action);
    void updateStructureSidebarSourceLocations(const TherionStudio::ProjectIndexSnapshot &projectIndex);
    void rebuildMapObjectsTree();
    void showSidebarPane(SidebarPane pane);
    void setSidebarPane(SidebarPane pane);
    void syncOpenDocumentsToProjectRoot();
    QWidget *documentWidgetForFilePath(const QString &filePath) const;
    void handleStructureSelectionChanged(const QModelIndex &current, const QModelIndex &previous, QTreeView *sourceTree);
    void handleStructureItemActivated(const QModelIndex &index, QTreeView *sourceTree);
    bool confirmCloseTab(int index);
    bool confirmCloseDirtyDocuments();
    void clearDocumentTabs();
    TherionStudio::TextEditorTab *openTextTab(const QString &filePath, bool showUnsupportedPrompt = true);
    TherionStudio::MapEditorTab *openMapEditorTab(const QString &filePath);
    void connectMapEditorTabUiSignals(TherionStudio::MapEditorTab *tab);
    QWidget *currentDocumentWidget() const;
    TherionStudio::TextEditorTab *currentTextTab() const;
    void showFindBar(bool replaceMode);
    void updateTabTitle(QWidget *tabWidget);
    void appendConsoleLine(const QString &line);
    void clearTherionConsoleOutput();
    void copyTherionConsoleOutput();
    void updateTherionRunnerState();
    void updateProjectActionState();
    void updateMapEditorActionState();
    QString therionWorkingDirectoryOverride() const;
    QString therionConfigResolutionDirectory() const;
    QString resolvedTherionWorkingDirectory() const;
    bool hasExplicitTherionConfigArgument() const;
    QString therionExecutableInput() const;
    QString therionRunTargetMode() const;
    QString currentDocumentTherionConfigPath() const;
    QString resolvedTherionTargetConfigPath() const;
    QString resolvedTherionConfigPath() const;
    void resetProjectTherionRunContext();
    void refreshTherionConfigDisplay();
    void refreshTherionRunTargetControls();
    void rememberSidebarWidth();
    void restoreSidebarWidth();
    bool isSidebarEffectivelyCollapsed() const;
    void scheduleSidebarCollapseLayoutSync();
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
    void refreshMapBackgroundPanel();
    TherionStudio::MapEditorTab *currentMapEditorTab() const;
    void detachMapEditorTab(TherionStudio::MapEditorTab *tab);
    void reattachDetachedMapEditorTab(TherionStudio::MapEditorTab *tab, bool focusTab);
    void focusDetachedMapEditorTab(const QString &canonicalPath);
    TherionStudio::MapEditorTab *detachedMapEditorTabForPath(const QString &canonicalPath) const;
    QList<TherionStudio::MapEditorTab *> detachedMapEditorTabs() const;
    bool confirmCloseDocumentWidget(QWidget *documentWidget);
    void initializeDocumentStatusWidgets();
    void refreshDocumentStatusWidgets();
    void setCompilerStatusIdle();
    void setCompilerStatusRunning(const QString &configPath);
    void setCompilerStatusResult(bool success, const QString &details);
    void updateCompilerStatusButton(const QString &text, const QString &toolTip, const QString &accentColor);
    void initializeWorkspaceModeSwitcher();
    void refreshWorkspaceModeSwitcher();
    void refreshWorkspaceIconTheme();
    void refreshWorkspaceModeSwitcherGeometry();
    void refreshViewMenuActions();
    void refreshFullScreenAction();
    void setMapMagnifierEnabledForOpenTabs(bool enabled);
    bool currentDocumentHasRightPanel() const;
    bool currentDocumentRightPanelCollapsed() const;
    QString currentDocumentRightPanelLabel() const;
    void setCurrentDocumentRightPanelCollapsed(bool collapsed);
    TherionStudio::MapEditorTab *currentDetachedMapTabWithContextHelp() const;
    bool currentDetachedMapContextHelpCollapsed() const;
    void setCurrentDetachedMapContextHelpCollapsed(bool collapsed);
    void triggerUndoForActiveDocument();
    void triggerRedoForActiveDocument();
    void triggerRawModeForActiveDocument();
    void triggerSecondaryEditorModeForActiveDocument();
    void triggerCompileCurrentConfigForActiveDocument();
    void triggerZoomInForActiveDocument();
    void triggerZoomOutForActiveDocument();
    void triggerFitForActiveDocument();
    void triggerFitWithBackgroundForActiveDocument();
    void triggerSelectForActiveDocument();
    void triggerCompleteDraftForActiveDocument();
    void triggerInsertScrapForActiveDocument();
    void triggerPointForActiveDocument();
    void triggerLineForActiveDocument();
    void triggerFreehandLineForActiveDocument();
    void triggerAreaForActiveDocument();

    QHBoxLayout *mainContentLayout_ = nullptr;
    QWidget *editorAreaHost_ = nullptr;
    QWidget *editorAreaColumn_ = nullptr;
    QVBoxLayout *editorAreaLayout_ = nullptr;
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
    QSplitter *mainContentSplitter_ = nullptr;
    QWidget *sidebarContainer_ = nullptr;
    QWidget *sidebarContentContainer_ = nullptr;
    QToolButton *sidebarFilesButton_ = nullptr;
    QToolButton *sidebarStructureButton_ = nullptr;
    QToolButton *sidebarConsoleButton_ = nullptr;
    QToolButton *sidebarCompileButton_ = nullptr;
    QStackedWidget *sidebarPages_ = nullptr;
    QWidget *consoleSidebarPage_ = nullptr;
    QVBoxLayout *consoleSidebarPageLayout_ = nullptr;
    QToolButton *sidebarCollapseButton_ = nullptr;
    QDockWidget *consoleDock_ = nullptr;
    QToolButton *consoleCollapseButton_ = nullptr;
    QAction *openProjectAction_ = nullptr;
    QAction *closeProjectAction_ = nullptr;
    QAction *undoAction_ = nullptr;
    QAction *redoAction_ = nullptr;
    QAction *sidebarCollapseAction_ = nullptr;
    QAction *rightPanelCollapseAction_ = nullptr;
    QAction *contextHelpCollapseAction_ = nullptr;
    QAction *mapMagnifierAction_ = nullptr;
    QAction *fullScreenAction_ = nullptr;
    QPlainTextEdit *consoleView_ = nullptr;
    QLineEdit *therionWorkingDirectoryEdit_ = nullptr;
    QPushButton *therionBrowseWorkingDirectoryButton_ = nullptr;
    QLineEdit *therionArgumentsEdit_ = nullptr;
    QComboBox *therionRunTargetCombo_ = nullptr;
    QLineEdit *therionTargetConfigEdit_ = nullptr;
    QPushButton *therionBrowseTargetConfigButton_ = nullptr;
    QLabel *therionConfigNameValue_ = nullptr;
    QLabel *therionConfigPathValue_ = nullptr;
    QLabel *therionWorkingDirectoryValue_ = nullptr;
    QPushButton *therionRunButton_ = nullptr;
    QPushButton *therionStopButton_ = nullptr;
    QPushButton *therionResetWorkingDirectoryButton_ = nullptr;
    QPushButton *therionClearOutputButton_ = nullptr;
    QPushButton *therionCopyOutputButton_ = nullptr;
    TherionStudio::TherionRunnerService *therionRunnerService_ = nullptr;
    TherionStudio::MainWindowTherionConsoleController therionConsoleController_;
    QLabel *statusMapZoomLabel_ = nullptr;
    QLabel *statusMapModeLabel_ = nullptr;
    QToolButton *statusCompilerButton_ = nullptr;
    QLabel *statusDocumentEncodingLabel_ = nullptr;
    QString activeTherionRunConfigPath_;
    QWidget *workspaceModeSwitcher_ = nullptr;
    QWidget *workspaceMapModeSwitcher_ = nullptr;
    QWidget *workspaceTextModeSwitcher_ = nullptr;
    QWidget *workspaceZoomGroup_ = nullptr;
    QWidget *workspaceMapToolsGroup_ = nullptr;
    QToolButton *workspaceOpenProjectButton_ = nullptr;
    QToolButton *workspaceCloseProjectButton_ = nullptr;
    QToolButton *workspaceSaveButton_ = nullptr;
    QToolButton *workspaceUndoButton_ = nullptr;
    QToolButton *workspaceRedoButton_ = nullptr;
    QToolButton *workspaceCompileCurrentConfigButton_ = nullptr;
    QToolButton *workspaceZoomInButton_ = nullptr;
    QToolButton *workspaceZoomOutButton_ = nullptr;
    QToolButton *workspaceFitButton_ = nullptr;
    QToolButton *workspaceFitBackgroundButton_ = nullptr;
    QToolButton *workspaceSelectButton_ = nullptr;
    QToolButton *workspaceCompleteDraftButton_ = nullptr;
    QToolButton *workspaceInsertScrapButton_ = nullptr;
    QToolButton *workspacePointButton_ = nullptr;
    QToolButton *workspaceLineButton_ = nullptr;
    QToolButton *workspaceFreehandLineButton_ = nullptr;
    QToolButton *workspaceAreaButton_ = nullptr;
    QFrame *workspaceProjectSeparator_ = nullptr;
    QFrame *workspaceHistorySeparator_ = nullptr;
    QFrame *workspaceCompileSeparator_ = nullptr;
    QFrame *workspaceZoomSeparator_ = nullptr;
    QToolButton *workspaceVisualModeButton_ = nullptr;
    QToolButton *workspaceRawModeButton_ = nullptr;
    QToolButton *workspaceMapPaneWindowButton_ = nullptr;
    QToolButton *workspaceTextRawModeButton_ = nullptr;
    QToolButton *workspaceBlocksModeButton_ = nullptr;
    bool workspaceModeSwitcherSyncInProgress_ = false;
    QFileSystemModel *projectModel_ = nullptr;
    QStandardItemModel *structureModel_ = nullptr;
    QStandardItemModel *mapObjectsModel_ = nullptr;
    QString projectRootPath_;
    QString projectStructureSummary_;
    QString lastAppliedStructureSidebarSignature_;
    QString structureExpansionProjectRootPath_;
    QSet<QString> structureExpandedNodeKeys_;
    QHash<QString, QString> structureNameOverrides_;
    SidebarPane activeSidebarPane_ = SidebarPane::FileBrowser;
    int sidebarExpandedWidth_ = 320;
    int sidebarRailWidth_ = 56;
    int consoleExpandedHeight_ = 240;
    bool sidebarCollapsed_ = false;
    bool consoleCollapsed_ = false;
    bool updatingSidebarSplitter_ = false;
    bool sidebarCollapseSyncPending_ = false;
    bool updatingMapBackgroundPanel_ = false;
    bool hasAppliedStructureSidebarIndex_ = false;
    bool hasStructureExpansionState_ = false;
    bool clearingDocumentTabs_ = false;
    bool shuttingDown_ = false;

    QHash<QString, QPointer<QMainWindow>> detachedMapWindowsByPath_;
    QHash<TherionStudio::MapEditorTab *, QString> detachedMapPathsByTab_;

    TherionStudio::QtFileSystem fileSystem_;
    std::unique_ptr<TherionStudio::ISessionStore> ownedSessionStore_;
    TherionStudio::ISessionStore *sessionStore_ = nullptr;
    TherionStudio::CommandCatalogStore commandCatalogStore_;
    TherionStudio::ProjectStructureScanner *structureSidebarScanner_ = nullptr;
};
