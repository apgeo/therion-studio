#include "MapEditorTab.h"

#include <QGuiApplication>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QSplitter>
#include <QStyleHints>
#include <QTextBrowser>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <functional>

#include "TextEditorTab.h"
#include "../core/SessionStore.h"

namespace TherionStudio
{
namespace
{
class DetachedMapPaneWindow final : public QMainWindow
{
public:
    explicit DetachedMapPaneWindow(QWidget *parent = nullptr)
        : QMainWindow(parent, Qt::Window)
    {
        setAttribute(Qt::WA_DeleteOnClose, true);
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
    std::function<void()> closeCallback_;
};
}

MapEditorTab::MapEditorTab(QWidget *parent)
    : QWidget(parent)
{
    undoStack_ = new QUndoStack(this);
    workspaceMode_ = WorkspaceMode::Split;
    touchFriendlyControlsEnabled_ = SessionStore::therionMapTouchFriendlyControlsEnabled();
    buildUi();
    updateWorkspaceVisibility();
    setTouchFriendlyControlsEnabled(touchFriendlyControlsEnabled_);

    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            handleApplicationAppearanceChanged();
        });
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

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 12, 12, 8);
    toolbarLayout->setSpacing(8);

    selectButton_ = new QPushButton(tr("Select"), toolbar);
    pointButton_ = new QPushButton(tr("Point"), toolbar);
    lineButton_ = new QPushButton(tr("Line"), toolbar);
    freehandLineButton_ = new QPushButton(tr("Freehand"), toolbar);
    smartTraceLineButton_ = new QPushButton(tr("Smart Trace"), toolbar);
    areaButton_ = new QPushButton(tr("Area"), toolbar);
    insertScrapButton_ = new QPushButton(tr("Insert Scrap"), toolbar);
    completeDraftButton_ = new QPushButton(tr("Complete Draft"), toolbar);

    connect(selectButton_, &QPushButton::clicked, this, &MapEditorTab::handleSelectModeTriggered);
    connect(pointButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddPointTriggered);
    connect(lineButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddLineTriggered);
    connect(freehandLineButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddFreehandLineTriggered);
    connect(smartTraceLineButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddSmartTraceLineTriggered);
    connect(areaButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddAreaTriggered);
    connect(insertScrapButton_, &QPushButton::clicked, this, &MapEditorTab::handleInsertScrapTriggered);
    connect(completeDraftButton_, &QPushButton::clicked, this, &MapEditorTab::handleCompleteDraftTriggered);

    undoButton_ = new QPushButton(tr("Undo"), toolbar);
    redoButton_ = new QPushButton(tr("Redo"), toolbar);
    zoomOutButton_ = new QPushButton(tr("Zoom -"), toolbar);
    zoomInButton_ = new QPushButton(tr("Zoom +"), toolbar);
    fitButton_ = new QPushButton(tr("Fit"), toolbar);
    fitBackgroundButton_ = new QPushButton(tr("Fit + BG"), toolbar);
    touchControlsButton_ = new QPushButton(tr("Touch Controls"), toolbar);
    touchControlsButton_->setCheckable(true);
    touchControlsButton_->setToolTip(tr("Enable touch-friendly map controls for pen-first workflows."));
    zoomLabel_ = new QLabel(tr("100%"), toolbar);
    zoomLabel_->setMinimumWidth(52);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(undoButton_, &QPushButton::clicked, this, &MapEditorTab::handleUndoTriggered);
    connect(redoButton_, &QPushButton::clicked, this, &MapEditorTab::handleRedoTriggered);
    connect(zoomOutButton_, &QPushButton::clicked, this, &MapEditorTab::handleZoomOutTriggered);
    connect(zoomInButton_, &QPushButton::clicked, this, &MapEditorTab::handleZoomInTriggered);
    connect(fitButton_, &QPushButton::clicked, this, &MapEditorTab::handleFitTriggered);
    connect(fitBackgroundButton_, &QPushButton::clicked, this, &MapEditorTab::handleFitWithBackgroundTriggered);
    connect(touchControlsButton_, &QPushButton::toggled, this, &MapEditorTab::handleTouchFriendlyControlsToggled);

    detachButton_ = new QPushButton(tr("Open Map in Window"), toolbar);
    detachButton_->setEnabled(true);
    connect(detachButton_, &QPushButton::clicked, this, &MapEditorTab::handleDetachPaneTriggered);

    summaryLabel_ = new QLabel(tr("Ready"), toolbar);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    summaryLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    toolbarStatusNote_ = summaryLabel_->text();

    toolbarLayout->addWidget(selectButton_);
    toolbarLayout->addWidget(pointButton_);
    toolbarLayout->addWidget(lineButton_);
    toolbarLayout->addWidget(freehandLineButton_);
    toolbarLayout->addWidget(smartTraceLineButton_);
    toolbarLayout->addWidget(areaButton_);
    toolbarLayout->addWidget(insertScrapButton_);
    toolbarLayout->addWidget(completeDraftButton_);
    toolbarLayout->addWidget(undoButton_);
    toolbarLayout->addWidget(redoButton_);
    toolbarLayout->addWidget(zoomOutButton_);
    toolbarLayout->addWidget(zoomInButton_);
    toolbarLayout->addWidget(fitButton_);
    toolbarLayout->addWidget(fitBackgroundButton_);
    toolbarLayout->addWidget(touchControlsButton_);
    toolbarLayout->addWidget(zoomLabel_);
    toolbarLayout->addWidget(detachButton_);
    toolbarLayout->addWidget(summaryLabel_, 1);

    auto *toolbarScrollArea = new QScrollArea(this);
    toolbarScrollArea->setFrameShape(QFrame::NoFrame);
    toolbarScrollArea->setWidgetResizable(false);
    toolbarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    toolbarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolbarScrollArea->setWidget(toolbar);
    layout->addWidget(toolbarScrollArea);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(6);

    textEditor_ = new TextEditorTab(splitter_);
    textEditor_->setInlineStatusVisible(false);
    connect(textEditor_, &TextEditorTab::titleChanged, this, &MapEditorTab::titleChanged);
    connect(textEditor_, &TextEditorTab::dirtyStateChanged, this, &MapEditorTab::dirtyStateChanged);
    connect(textEditor_, &TextEditorTab::currentLineChanged, this, &MapEditorTab::handleTextEditorCurrentLineChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::refreshMapScene);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::documentTextChanged);

    connect(undoStack_, &QUndoStack::canUndoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::canRedoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::indexChanged, this, &MapEditorTab::updateCommandSurfaceState);

    mapHelpSplitter_ = new QSplitter(Qt::Vertical, splitter_);
    mapHelpSplitter_->setChildrenCollapsible(false);
    mapHelpSplitter_->setHandleWidth(12);

    mapPaneContainer_ = new QWidget(mapHelpSplitter_);
    auto *mapPaneLayout = new QVBoxLayout(mapPaneContainer_);
    mapPaneLayout->setContentsMargins(0, 0, 0, 0);
    mapPaneLayout->setSpacing(0);

    mapView_ = new QGraphicsView(mapPaneContainer_);
    mapView_->setFrameShape(QFrame::NoFrame);
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
    buildHelpPanel();

    splitter_->addWidget(textEditor_);
    mapPaneLayout->addWidget(mapView_);
    mapHelpSplitter_->addWidget(mapPaneContainer_);
    mapHelpSplitter_->addWidget(helpPanel_);
    mapHelpSplitter_->setStretchFactor(0, 1);
    mapHelpSplitter_->setStretchFactor(1, 0);
    mapHelpSplitter_->setCollapsible(1, true);
    mapHelpSplitter_->setSizes(QList<int>{1, helpPanelHeight_});
    splitter_->addWidget(mapHelpSplitter_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);

    layout->addWidget(splitter_, 1);

    refreshMapScene();
    refreshDetachButtonState();
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

MapEditorTab::WorkspaceMode MapEditorTab::workspaceMode() const
{
    return workspaceMode_;
}

void MapEditorTab::setWorkspaceMode(WorkspaceMode mode)
{
    Q_UNUSED(mode);
    workspaceMode_ = WorkspaceMode::Split;
    updateWorkspaceVisibility();
    refreshMapScene();
}

void MapEditorTab::handleTextEditorCurrentLineChanged(int lineNumber)
{
    selectMapLine(lineNumber);
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleUndoTriggered()
{
    if (undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->undo();
        flushPendingMapSceneRefreshAfterCommand();
    }
}

void MapEditorTab::handleRedoTriggered()
{
    if (undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->redo();
        flushPendingMapSceneRefreshAfterCommand();
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

void MapEditorTab::setHelpCollapsed(bool collapsed)
{
    helpCollapsed_ = collapsed;
    if (helpBrowser_ != nullptr) {
        helpBrowser_->setVisible(!collapsed);
    }
    if (mapHelpSplitter_ != nullptr && helpPanel_ != nullptr) {
        if (!collapsed && helpPanelHeight_ > 0) {
            const QList<int> sizes = mapHelpSplitter_->sizes();
            mapHelpSplitter_->setSizes(QList<int>{sizes.value(0, 1), helpPanelHeight_});
        }

        if (collapsed) {
            const QList<int> sizes = mapHelpSplitter_->sizes();
            if (sizes.size() >= 2) {
                helpPanelHeight_ = qMax(sizes.at(1), helpPanel_->minimumSizeHint().height());
            }

            mapHelpSplitter_->setSizes(QList<int>{1, 0});
        }
    }
}

void MapEditorTab::updateWorkspaceVisibility()
{
    if (mapPaneDetached_) {
        textEditor_->show();
        if (mapHelpSplitter_ != nullptr) {
            mapHelpSplitter_->hide();
        }
        refreshStatus();
        return;
    }

    textEditor_->show();
    if (mapHelpSplitter_ != nullptr) {
        mapHelpSplitter_->show();
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
    }
}

QString MapEditorTab::displayPath() const
{
    return textEditor_->filePath();
}

void MapEditorTab::handleDetachPaneTriggered()
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
    if (mapPaneDetached_ || mapPaneContainer_ == nullptr || mapHelpSplitter_ == nullptr) {
        return;
    }

    mapPaneContainer_->setParent(nullptr);

    auto *window = new DetachedMapPaneWindow(this);
    window->setWindowTitle(tr("%1 — Map").arg(displayName()));
    window->setWindowFilePath(filePath());
    window->setCentralWidget(mapPaneContainer_);
    window->setCloseCallback([this]() { reattachMapPaneFromWindow(); });

    detachedMapPaneWindow_ = window;
    mapPaneDetached_ = true;
    refreshDetachButtonState();
    updateWorkspaceVisibility();

    const QSize mapSize = mapView_ != nullptr ? mapView_->size() : QSize();
    window->resize(mapSize.isValid() ? mapSize.expandedTo(QSize(900, 700)) : QSize(1200, 800));
    window->show();
    window->raise();
    window->activateWindow();
}

void MapEditorTab::reattachMapPaneFromWindow()
{
    if (!mapPaneDetached_ || mapPaneContainer_ == nullptr || mapHelpSplitter_ == nullptr || reattachingMapPane_) {
        return;
    }

    reattachingMapPane_ = true;

    if (detachedMapPaneWindow_ != nullptr && detachedMapPaneWindow_->centralWidget() == mapPaneContainer_) {
        detachedMapPaneWindow_->takeCentralWidget();
    }

    mapPaneContainer_->setParent(mapHelpSplitter_);
    mapHelpSplitter_->insertWidget(0, mapPaneContainer_);

    mapPaneDetached_ = false;
    detachedMapPaneWindow_ = nullptr;
    refreshDetachButtonState();
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

void MapEditorTab::refreshDetachButtonState()
{
    if (detachButton_ == nullptr) {
        return;
    }

    if (mapPaneDetached_) {
        detachButton_->setText(tr("Return Map Pane"));
        detachButton_->setToolTip(tr("Return the map pane from the detached window into this tab."));
    } else {
        detachButton_->setText(tr("Open Map in Window"));
        detachButton_->setToolTip(tr("Open the map pane in a separate window (for multi-monitor workflows)."));
    }
}

void MapEditorTab::setTouchFriendlyControlsEnabled(bool enabled)
{
    touchFriendlyControlsEnabled_ = enabled;
    SessionStore::setTherionMapTouchFriendlyControlsEnabled(enabled);
    if (touchControlsButton_ != nullptr) {
        touchControlsButton_->blockSignals(true);
        touchControlsButton_->setChecked(enabled);
        touchControlsButton_->blockSignals(false);
    }
    updateCommandSurfaceState();
}
}
