#include "MapEditorTab.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include "TextEditorTab.h"
#include "../core/SessionStore.h"

namespace TherionStudio
{
MapEditorTab::MapEditorTab(QWidget *parent)
    : QWidget(parent)
{
    undoStack_ = new QUndoStack(this);
    workspaceMode_ = workspaceModeFromSetting(SessionStore::therionMapWorkspaceMode());
    touchFriendlyControlsEnabled_ = SessionStore::therionMapTouchFriendlyControlsEnabled();
    buildUi();
    setWorkspaceMode(workspaceMode_);
    setTouchFriendlyControlsEnabled(touchFriendlyControlsEnabled_);
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

    auto *modeLabel = new QLabel(tr("Workspace"), toolbar);

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

    workspaceModeCombo_ = new QComboBox(toolbar);
    workspaceModeCombo_->addItem(tr("Text Only"));
    workspaceModeCombo_->addItem(tr("Map Only"));
    workspaceModeCombo_->addItem(tr("Split"));
    workspaceModeCombo_->setCurrentIndex(workspaceModeToIndex(workspaceMode_));
    connect(workspaceModeCombo_, &QComboBox::currentIndexChanged, this, &MapEditorTab::handleWorkspaceModeChanged);

    detachButton_ = new QPushButton(tr("Open Dedicated Window"), toolbar);
    detachButton_->setEnabled(false);

    summaryLabel_ = new QLabel(tr("TH2 map workspace scaffolding"), toolbar);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    summaryLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    toolbarStatusNote_ = summaryLabel_->text();

    toolbarLayout->addWidget(modeLabel);
    toolbarLayout->addWidget(workspaceModeCombo_);
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

    mapView_ = new QGraphicsView(mapHelpSplitter_);
    mapView_->setFrameShape(QFrame::NoFrame);
    mapView_->setDragMode(QGraphicsView::NoDrag);
    mapView_->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    mapView_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    mapView_->setRenderHint(QPainter::Antialiasing, true);
    mapView_->setBackgroundBrush(QColor(QStringLiteral("#1e1f24")));
    mapView_->setFocusPolicy(Qt::StrongFocus);
    mapView_->installEventFilter(this);
    if (mapView_->viewport() != nullptr) {
        mapView_->viewport()->setFocusPolicy(Qt::StrongFocus);
        mapView_->viewport()->installEventFilter(this);
    }

    buildMapScene();
    buildHelpPanel();

    splitter_->addWidget(textEditor_);
    mapHelpSplitter_->addWidget(mapView_);
    mapHelpSplitter_->addWidget(helpPanel_);
    mapHelpSplitter_->setStretchFactor(0, 1);
    mapHelpSplitter_->setStretchFactor(1, 0);
    mapHelpSplitter_->setCollapsible(1, true);
    mapHelpSplitter_->setSizes(QList<int>{1, helpPanelHeight_});
    installHelpBorderToggle();
    splitter_->addWidget(mapHelpSplitter_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);

    layout->addWidget(splitter_, 1);

    statusRow_ = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusRow_);
    statusLayout->setContentsMargins(8, 0, 8, 8);
    statusLayout->setSpacing(12);

    statusPathLabel_ = new QLabel(tr("No file open"), statusRow_);
    statusPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusPathLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusPathLabel_->setMinimumWidth(0);
    statusEncodingLabel_ = new QLabel(tr("Encoding: UTF-8"), statusRow_);
    statusEncodingLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    statusLayout->addWidget(statusPathLabel_, 1);
    statusLayout->addWidget(statusEncodingLabel_);
    layout->addWidget(statusRow_);

    refreshMapScene();
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
    if (workspaceMode_ == WorkspaceMode::MapOnly) {
        setWorkspaceMode(WorkspaceMode::Split);
    }

    textEditor_->showFindBar(replaceMode);
}

void MapEditorTab::hideFindBar()
{
    textEditor_->hideFindBar();
}

void MapEditorTab::goToLine(int lineNumber)
{
    if (workspaceMode_ == WorkspaceMode::MapOnly) {
        setWorkspaceMode(WorkspaceMode::Split);
    }

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

MapEditorTab::WorkspaceMode MapEditorTab::workspaceMode() const
{
    return workspaceMode_;
}

void MapEditorTab::setWorkspaceMode(WorkspaceMode mode)
{
    workspaceMode_ = mode;
    if (workspaceModeCombo_ != nullptr) {
        workspaceModeCombo_->blockSignals(true);
        workspaceModeCombo_->setCurrentIndex(workspaceModeToIndex(mode));
        workspaceModeCombo_->blockSignals(false);
    }

    SessionStore::setTherionMapWorkspaceMode(workspaceModeToSetting(mode));
    updateWorkspaceVisibility();
    refreshMapScene();
}

void MapEditorTab::handleWorkspaceModeChanged(int index)
{
    setWorkspaceMode(workspaceModeFromIndex(index));
}

void MapEditorTab::handleTextEditorCurrentLineChanged(int lineNumber)
{
    selectMapLine(lineNumber);
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleUndoTriggered()
{
    if (undoStack_ != nullptr) {
        undoStack_->undo();
    }
}

void MapEditorTab::handleRedoTriggered()
{
    if (undoStack_ != nullptr) {
        undoStack_->redo();
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
    refreshHelpBorderToggle();
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
    if (workspaceMode_ == WorkspaceMode::TextOnly) {
        textEditor_->show();
        if (mapHelpSplitter_ != nullptr) {
            mapHelpSplitter_->hide();
        }
    } else if (workspaceMode_ == WorkspaceMode::MapOnly) {
        textEditor_->hide();
        if (mapHelpSplitter_ != nullptr) {
            mapHelpSplitter_->show();
        }
    } else {
        textEditor_->show();
        if (mapHelpSplitter_ != nullptr) {
            mapHelpSplitter_->show();
        }
        splitter_->setSizes(QList<int>{1, 1});
    }

    refreshStatus();
}

void MapEditorTab::refreshTitle()
{
    emit titleChanged();
}

void MapEditorTab::refreshStatus()
{
    if (statusPathLabel_ == nullptr || statusEncodingLabel_ == nullptr) {
        return;
    }

    const QString documentLabel = textEditor_ != nullptr ? textEditor_->statusPathText() : tr("No file open");
    const QString encodingLabel = textEditor_ != nullptr ? textEditor_->statusEncodingText() : tr("UTF-8");
    statusPathLabel_->setText(documentLabel);
    statusEncodingLabel_->setText(tr("Encoding: %1").arg(encodingLabel));
}

QString MapEditorTab::displayPath() const
{
    return textEditor_->filePath();
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceModeFromIndex(int index)
{
    switch (index) {
    case 0:
        return WorkspaceMode::TextOnly;
    case 1:
        return WorkspaceMode::MapOnly;
    default:
        return WorkspaceMode::Split;
    }
}

int MapEditorTab::workspaceModeToIndex(WorkspaceMode mode)
{
    switch (mode) {
    case WorkspaceMode::TextOnly:
        return 0;
    case WorkspaceMode::MapOnly:
        return 1;
    case WorkspaceMode::Split:
        return 2;
    }

    return 2;
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceModeFromSetting(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("text-only")) {
        return WorkspaceMode::TextOnly;
    }
    if (normalized == QStringLiteral("map-only")) {
        return WorkspaceMode::MapOnly;
    }

    return WorkspaceMode::Split;
}

QString MapEditorTab::workspaceModeToSetting(WorkspaceMode mode)
{
    switch (mode) {
    case WorkspaceMode::TextOnly:
        return QStringLiteral("text-only");
    case WorkspaceMode::MapOnly:
        return QStringLiteral("map-only");
    case WorkspaceMode::Split:
        return QStringLiteral("split");
    }

    return QStringLiteral("split");
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
