#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"

#include <QAbstractItemView>
#include <QDockWidget>
#include <QDesktopServices>
#include <QFileSystemModel>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
class DockTitleBarWidget final : public QWidget
{
public:
    explicit DockTitleBarWidget(const QString &title, QWidget *parent = nullptr)
        : QWidget(parent)
        , titleLabel_(new QLabel(title, this))
        , toggleButton_(new QToolButton(this))
    {
        layout_ = new QHBoxLayout(this);
        layout_->setContentsMargins(8, 4, 4, 4);
        layout_->setSpacing(4);
        layout_->addWidget(titleLabel_);
        layout_->addStretch(1);
        toggleButton_->setAutoRaise(true);
        toggleButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
        layout_->addWidget(toggleButton_);
    }

    void setCollapsedVisual(bool collapsed)
    {
        titleLabel_->setVisible(!collapsed);
        layout_->setContentsMargins(collapsed ? 2 : 8, 2, 2, 2);
        layout_->setSpacing(collapsed ? 0 : 4);
        updateGeometry();
    }

    QToolButton *toggleButton() const
    {
        return toggleButton_;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        event->ignore();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        event->ignore();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        event->ignore();
    }

    QSize minimumSizeHint() const override
    {
        const QSize buttonSize = toggleButton_->sizeHint();
        const bool titleVisible = titleLabel_->isVisible();
        const int left = titleVisible ? 8 : 2;
        const int right = 2;
        const int topBottom = 4;
        const int titleWidth = titleVisible ? titleLabel_->sizeHint().width() + 4 : 0;
        return QSize(left + titleWidth + buttonSize.width() + right,
                     qMax(buttonSize.height(), titleLabel_->sizeHint().height()) + topBottom);
    }

    QSize sizeHint() const override
    {
        return minimumSizeHint();
    }

private:
    QHBoxLayout *layout_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QToolButton *toggleButton_ = nullptr;
};

constexpr int SidebarCollapsedRailWidth = 56;

bool isTherionProjectFilePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    if (info.fileName().compare(QStringLiteral("thconfig"), Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th") || suffix == QStringLiteral("th2");
}
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
    projectTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    projectTree_->setAlternatingRowColors(true);
    projectTree_->hideColumn(1);
    projectTree_->hideColumn(2);
    projectTree_->hideColumn(3);
    connect(projectTree_, &QTreeView::activated, this, &MainWindow::handleProjectTreeActivated);
    connect(projectTree_, &QTreeView::customContextMenuRequested, this, &MainWindow::handleProjectTreeContextMenuRequested);

    projectLayout->addWidget(projectTree_, 1);
    sidebarPages_->addWidget(projectPage);

    resetProjectBrowser();
}

void MainWindow::handleProjectTreeContextMenuRequested(const QPoint &position)
{
    if (projectTree_ == nullptr || projectModel_ == nullptr) {
        return;
    }

    const QModelIndex index = projectTree_->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    const QString filePath = projectModel_->filePath(index);
    if (filePath.isEmpty()) {
        return;
    }

    const QFileInfo info(filePath);
    if (!info.exists()) {
        return;
    }

    QMenu menu(projectTree_);

    if (info.isFile()) {
        menu.addAction(tr("Open"), this, [this, index]() {
            handleProjectTreeActivated(index);
        });

        if (info.suffix().compare(QStringLiteral("th2"), Qt::CaseInsensitive) == 0) {
            menu.addAction(tr("Open in Map Editor"), this, [this, filePath]() {
                openMapEditorTab(filePath);
            });
        }

        if (!isTherionProjectFilePath(filePath)) {
            menu.addAction(tr("Open Externally"), this, [this, filePath]() {
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
                    QMessageBox::warning(this,
                                         tr("Open Externally"),
                                         tr("Failed to open %1 with the system default application.")
                                             .arg(QDir::toNativeSeparators(filePath)));
                }
            });
        }
    }

    if (menu.isEmpty()) {
        return;
    }

    menu.exec(projectTree_->viewport()->mapToGlobal(position));
}

void MainWindow::buildStructureSidebar()
{
    structureDock_ = new QDockWidget(tr("Sidebar"), this);
    structureDock_->setObjectName(QStringLiteral("SidebarDock"));
    structureDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto *titleBar = new QWidget(structureDock_);
    titleBar->setFixedHeight(0);
    titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sidebarCollapseButton_ = nullptr;
    structureDock_->setTitleBarWidget(titleBar);
    connect(structureDock_, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea) {
        updateSidebarCollapseButton();
    });
    connect(structureDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible) {
            if (!sidebarCollapsed_) {
                rememberSidebarWidth();
            }
            return;
        }

        if (sidebarCollapsed_) {
            if (sidebarContentContainer_ != nullptr) {
                sidebarContentContainer_->setVisible(false);
            }
            QTimer::singleShot(0, this, [this]() {
                if (structureDock_ == nullptr || !structureDock_->isVisible()) {
                    return;
                }

                resizeDocks({structureDock_}, {SidebarCollapsedRailWidth}, Qt::Horizontal);
            });
            updateSidebarCollapseButton();
            return;
        }

        restoreSidebarWidth();
        updateSidebarCollapseButton();
    });

    auto *container = new QWidget(structureDock_);
    auto *containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto *activityBar = new QFrame(container);
    activityBar->setFrameShape(QFrame::StyledPanel);
    auto *activityLayout = new QVBoxLayout(activityBar);
    activityLayout->setContentsMargins(6, 8, 6, 8);
    activityLayout->setSpacing(6);

    const QIcon filesIcon = style()->standardIcon(QStyle::SP_DirIcon);
    const QIcon structureIcon = style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    const QIcon mapIcon = style()->standardIcon(QStyle::SP_FileIcon);
    const QIcon consoleIcon = style()->standardIcon(QStyle::SP_ComputerIcon);

    sidebarFilesButton_ = new QToolButton(activityBar);
    sidebarFilesButton_->setIcon(filesIcon);
    sidebarFilesButton_->setToolTip(tr("Files"));
    sidebarFilesButton_->setCheckable(true);
    connect(sidebarFilesButton_, &QToolButton::clicked, this, [this]() {
        if (sidebarCollapsed_) {
            setSidebarCollapsed(false);
            setSidebarPane(SidebarPane::FileBrowser);
            return;
        }
        if (activeSidebarPane_ == SidebarPane::FileBrowser) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(SidebarPane::FileBrowser);
    });
    activityLayout->addWidget(sidebarFilesButton_);

    sidebarStructureButton_ = new QToolButton(activityBar);
    sidebarStructureButton_->setIcon(structureIcon);
    sidebarStructureButton_->setToolTip(tr("Structure"));
    sidebarStructureButton_->setCheckable(true);
    connect(sidebarStructureButton_, &QToolButton::clicked, this, [this]() {
        if (sidebarCollapsed_) {
            setSidebarCollapsed(false);
            setSidebarPane(SidebarPane::StructureBrowser);
            return;
        }
        if (activeSidebarPane_ == SidebarPane::StructureBrowser) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(SidebarPane::StructureBrowser);
    });
    activityLayout->addWidget(sidebarStructureButton_);

    sidebarMapButton_ = new QToolButton(activityBar);
    sidebarMapButton_->setIcon(mapIcon);
    sidebarMapButton_->setToolTip(tr("Map"));
    sidebarMapButton_->setCheckable(true);
    connect(sidebarMapButton_, &QToolButton::clicked, this, [this]() {
        if (sidebarCollapsed_) {
            setSidebarCollapsed(false);
            setSidebarPane(SidebarPane::MapEditor);
            return;
        }
        if (activeSidebarPane_ == SidebarPane::MapEditor) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(SidebarPane::MapEditor);
    });
    activityLayout->addWidget(sidebarMapButton_);

    sidebarConsoleButton_ = new QToolButton(activityBar);
    sidebarConsoleButton_->setIcon(consoleIcon);
    sidebarConsoleButton_->setToolTip(tr("Console"));
    sidebarConsoleButton_->setCheckable(true);
    connect(sidebarConsoleButton_, &QToolButton::clicked, this, [this]() {
        if (sidebarCollapsed_) {
            setSidebarCollapsed(false);
            setSidebarPane(SidebarPane::Console);
            return;
        }
        if (activeSidebarPane_ == SidebarPane::Console) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(SidebarPane::Console);
    });
    activityLayout->addWidget(sidebarConsoleButton_);
    activityLayout->addStretch(1);

    sidebarContentContainer_ = new QWidget(container);
    auto *sidebarContentLayout = new QVBoxLayout(sidebarContentContainer_);
    sidebarContentLayout->setContentsMargins(0, 0, 0, 0);
    sidebarContentLayout->setSpacing(0);

    sidebarPages_ = new QStackedWidget(sidebarContentContainer_);
    sidebarContentLayout->addWidget(sidebarPages_, 1);

    containerLayout->addWidget(activityBar, 0);
    containerLayout->addWidget(sidebarContentContainer_, 1);

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

    auto *mapPageSplitter = new QSplitter(Qt::Vertical, mapObjectsPage);
    mapPageSplitter->setChildrenCollapsible(false);
    mapPageSplitter->addWidget(mapObjectsTree_);

    auto *backgroundContainer = new QWidget(mapPageSplitter);
    auto *backgroundContainerLayout = new QVBoxLayout(backgroundContainer);
    backgroundContainerLayout->setContentsMargins(0, 0, 0, 0);
    backgroundContainerLayout->setSpacing(0);
    buildMapBackgroundPanel(backgroundContainer, backgroundContainerLayout);

    mapPageSplitter->addWidget(backgroundContainer);
    mapPageSplitter->setStretchFactor(0, 3);
    mapPageSplitter->setStretchFactor(1, 2);
    mapPageSplitter->setCollapsible(1, true);

    mapObjectsLayout->addWidget(mapPageSplitter, 1);
    sidebarPages_->addWidget(mapObjectsPage);

    consoleSidebarPage_ = new QWidget(sidebarPages_);
    consoleSidebarPageLayout_ = new QVBoxLayout(consoleSidebarPage_);
    consoleSidebarPageLayout_->setContentsMargins(12, 12, 12, 12);
    consoleSidebarPageLayout_->setSpacing(8);
    sidebarPages_->addWidget(consoleSidebarPage_);

    structureDock_->setWidget(container);
    addDockWidget(Qt::LeftDockWidgetArea, structureDock_);
    updateSidebarCollapseButton();
    setSidebarPane(activeSidebarPane_);
    refreshMapBackgroundPanel();
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
    if (sidebarFilesButton_ != nullptr) {
        sidebarFilesButton_->setChecked(pane == SidebarPane::FileBrowser);
    }
    if (sidebarStructureButton_ != nullptr) {
        sidebarStructureButton_->setChecked(pane == SidebarPane::StructureBrowser);
    }
    if (sidebarMapButton_ != nullptr) {
        sidebarMapButton_->setChecked(pane == SidebarPane::MapEditor);
    }
    if (sidebarConsoleButton_ != nullptr) {
        sidebarConsoleButton_->setChecked(pane == SidebarPane::Console);
    }
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

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (structureDock_ == nullptr) {
        return;
    }

    if (collapsed == sidebarCollapsed_) {
        updateSidebarCollapseButton();
        return;
    }

    sidebarCollapsed_ = collapsed;
    QWidget *contentWidget = sidebarContentContainer_;
    if (collapsed) {
        rememberSidebarWidth();
        if (contentWidget != nullptr) {
            contentWidget->setVisible(false);
        }

        QTimer::singleShot(0, this, [this]() {
            if (structureDock_ == nullptr || !structureDock_->isVisible()) {
                return;
            }

            resizeDocks({structureDock_}, {SidebarCollapsedRailWidth}, Qt::Horizontal);
        });
    } else {
        if (contentWidget != nullptr) {
            contentWidget->setVisible(true);
        }
        restoreSidebarWidth();
    }

    updateSidebarCollapseButton();
}

void MainWindow::updateSidebarCollapseButton()
{
    if (sidebarCollapseButton_ == nullptr || structureDock_ == nullptr) {
        return;
    }

    if (auto *titleBar = dynamic_cast<DockTitleBarWidget *>(structureDock_->titleBarWidget())) {
        titleBar->setCollapsedVisual(sidebarCollapsed_);
    }

    const Qt::DockWidgetArea area = dockWidgetArea(structureDock_);
    Qt::ArrowType arrowType = Qt::LeftArrow;
    if (sidebarCollapsed_) {
        arrowType = area == Qt::RightDockWidgetArea ? Qt::LeftArrow : Qt::RightArrow;
    } else {
        arrowType = area == Qt::RightDockWidgetArea ? Qt::RightArrow : Qt::LeftArrow;
    }

    sidebarCollapseButton_->setArrowType(arrowType);
    sidebarCollapseButton_->setToolTip(sidebarCollapsed_ ? tr("Expand sidebar") : tr("Collapse sidebar"));
}
