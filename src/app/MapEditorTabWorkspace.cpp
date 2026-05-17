#include "MapEditorTab.h"

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QShortcut>
#include <QScopedValueRollback>
#include <QSplitter>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <algorithm>
#include <functional>

#include "TextEditorTab.h"
#include "../core/SessionStore.h"

namespace TherionStudio
{
namespace
{
constexpr int kToolbarIconExtent = 14;
constexpr int kToolbarButtonExtent = 22;
const char *kLucideIconNameProperty = "lucideIconName";

QString rgbaCss(const QColor &color, qreal alpha)
{
    const qreal clampedAlpha = std::clamp(alpha, 0.0, 1.0);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(clampedAlpha, 0, 'f', 2);
}

QString mapToolbarStyleSheet(const QPalette &palette)
{
    const QColor panel = palette.color(QPalette::Window);
    const QColor border = palette.color(QPalette::Mid);
    const QColor hover = palette.color(QPalette::Highlight);
    const QColor text = palette.color(QPalette::ButtonText);

    return QStringLiteral(
               "#mapToolbarOverlay {"
               "background: %1;"
               "border: 1px solid %2;"
               "border-radius: 10px;"
               "}"
               "#mapToolbarOverlay QToolButton {"
               "background: transparent;"
               "border: none;"
               "border-radius: 5px;"
               "padding: 0px;"
               "color: %3;"
               "}"
               "#mapToolbarOverlay QToolButton:hover {"
               "background: %4;"
               "}"
               "#mapToolbarOverlay QToolButton:checked {"
               "background: %5;"
               "}"
               "#mapToolbarOverlay QLabel {"
               "color: %3;"
               "}")
        .arg(rgbaCss(panel, 0.92),
             border.name(QColor::HexRgb),
             text.name(QColor::HexRgb),
             rgbaCss(hover, 0.16),
             rgbaCss(hover, 0.24));
}

QPalette mapToolbarPalette()
{
    return qApp != nullptr ? qApp->palette() : QPalette();
}

QPixmap renderLucidePixmap(const QString &iconName, const QColor &color)
{
    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        return {};
    }

    QPixmap pixmap(kToolbarIconExtent, kToolbarIconExtent);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0.0, 0.0, kToolbarIconExtent, kToolbarIconExtent));
    return pixmap;
}

QIcon lucideIcon(const QString &iconName, const QPalette &palette)
{
    QIcon icon;
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::ButtonText)), QIcon::Normal);
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::ButtonText)), QIcon::Disabled);
    return icon;
}

QToolButton *createMapToolbarButton(QWidget *parent,
                                    const QString &objectName,
                                    const QString &label,
                                    const QString &iconName,
                                    const QString &toolTip = {})
{
    auto *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(label);
    button->setAccessibleName(label);
    button->setToolTip(toolTip.isEmpty() ? label : toolTip);
    button->setProperty(kLucideIconNameProperty, iconName);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(kToolbarIconExtent, kToolbarIconExtent));
    button->setFixedSize(QSize(kToolbarButtonExtent, kToolbarButtonExtent));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::StrongFocus);
    return button;
}

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
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
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
    toolbar->setObjectName(QStringLiteral("mapToolbarOverlay"));
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(7, 3, 7, 3);
    toolbarLayout->setSpacing(2);

    selectButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarSelectButton"), tr("Select"), QStringLiteral("mouse-pointer-2"));
    pointButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarPointButton"), tr("Point"), QStringLiteral("map-pin"));
    lineButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarLineButton"), tr("Line"), QStringLiteral("spline"));
    freehandLineButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarFreehandButton"), tr("Freehand"), QStringLiteral("pencil-line"));
    smartTraceLineButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarSmartTraceButton"), tr("Smart Trace"), QStringLiteral("wand-sparkles"));
    areaButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarAreaButton"), tr("Area"), QStringLiteral("pentagon"));
    insertScrapButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarInsertScrapButton"), tr("Insert Scrap"), QStringLiteral("square-plus"));
    completeDraftButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarCompleteDraftButton"), tr("Complete Draft"), QStringLiteral("check"));

    connect(selectButton_, &QToolButton::clicked, this, &MapEditorTab::handleSelectModeTriggered);
    connect(pointButton_, &QToolButton::clicked, this, &MapEditorTab::handleAddPointTriggered);
    connect(lineButton_, &QToolButton::clicked, this, &MapEditorTab::handleAddLineTriggered);
    connect(freehandLineButton_, &QToolButton::clicked, this, &MapEditorTab::handleAddFreehandLineTriggered);
    connect(smartTraceLineButton_, &QToolButton::clicked, this, &MapEditorTab::handleAddSmartTraceLineTriggered);
    connect(areaButton_, &QToolButton::clicked, this, &MapEditorTab::handleAddAreaTriggered);
    connect(insertScrapButton_, &QToolButton::clicked, this, &MapEditorTab::handleInsertScrapTriggered);
    connect(completeDraftButton_, &QToolButton::clicked, this, &MapEditorTab::handleCompleteDraftTriggered);

    undoButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarUndoButton"), tr("Undo"), QStringLiteral("undo-2"));
    redoButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarRedoButton"), tr("Redo"), QStringLiteral("redo-2"));
    zoomOutButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarZoomOutButton"), tr("Zoom Out"), QStringLiteral("zoom-out"));
    zoomInButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarZoomInButton"), tr("Zoom In"), QStringLiteral("zoom-in"));
    fitButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarFitButton"), tr("Fit"), QStringLiteral("scan"));
    fitBackgroundButton_ = createMapToolbarButton(toolbar, QStringLiteral("mapToolbarFitBackgroundButton"), tr("Fit With Background"), QStringLiteral("image"));
    touchControlsButton_ = createMapToolbarButton(toolbar,
                                                  QStringLiteral("mapToolbarTouchControlsButton"),
                                                  tr("Touch Controls"),
                                                  QStringLiteral("hand"),
                                                  tr("Enable touch-friendly map controls for pen-first workflows."));
    touchControlsButton_->setCheckable(true);
    zoomLabel_ = new QLabel(tr("100%"), toolbar);
    zoomLabel_->setMinimumWidth(42);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(undoButton_, &QToolButton::clicked, this, &MapEditorTab::handleUndoTriggered);
    connect(redoButton_, &QToolButton::clicked, this, &MapEditorTab::handleRedoTriggered);
    connect(zoomOutButton_, &QToolButton::clicked, this, &MapEditorTab::handleZoomOutTriggered);
    connect(zoomInButton_, &QToolButton::clicked, this, &MapEditorTab::handleZoomInTriggered);
    connect(fitButton_, &QToolButton::clicked, this, &MapEditorTab::handleFitTriggered);
    connect(fitBackgroundButton_, &QToolButton::clicked, this, &MapEditorTab::handleFitWithBackgroundTriggered);
    connect(touchControlsButton_, &QToolButton::toggled, this, &MapEditorTab::handleTouchFriendlyControlsToggled);

    detachButton_ = createMapToolbarButton(toolbar,
                                           QStringLiteral("mapToolbarDetachButton"),
                                           tr("Open Map in Window"),
                                           QStringLiteral("external-link"));
    detachButton_->setEnabled(true);
    connect(detachButton_, &QToolButton::clicked, this, &MapEditorTab::handleDetachPaneTriggered);

    summaryLabel_ = new QLabel(tr("Ready"), toolbar);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    summaryLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    summaryLabel_->setVisible(false);
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
    toolbar->setFixedHeight(kToolbarButtonExtent + 8);
    toolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    toolbar->setPalette(mapToolbarPalette());
    toolbar->setStyleSheet(mapToolbarStyleSheet(mapToolbarPalette()));
    mapToolbar_ = toolbar;

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(6);

    textEditor_ = new TextEditorTab(splitter_);
    textEditor_->setInlineStatusVisible(false);
    connect(textEditor_, &TextEditorTab::titleChanged, this, &MapEditorTab::titleChanged);
    connect(textEditor_, &TextEditorTab::dirtyStateChanged, this, &MapEditorTab::dirtyStateChanged);
    connect(textEditor_, &TextEditorTab::currentLineChanged, this, &MapEditorTab::handleTextEditorCurrentLineChanged);
    connect(textEditor_, &TextEditorTab::cursorPositionChanged, this, &MapEditorTab::handleTextEditorCursorPositionChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::refreshMapScene);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::documentTextChanged);

    connect(undoStack_, &QUndoStack::canUndoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::canRedoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::indexChanged, this, &MapEditorTab::updateCommandSurfaceState);

    mapPaneContainer_ = new QWidget(splitter_);
    mapPaneContainer_->setMinimumWidth(420);
    auto *mapPaneLayout = new QVBoxLayout(mapPaneContainer_);
    mapPaneLayout->setContentsMargins(4, 0, 4, 4);
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

    cancelDrawShortcut_ = new QShortcut(this);
    cancelDrawShortcut_->setKey(QKeySequence(Qt::Key_Escape));
    cancelDrawShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cancelDrawShortcut_, &QShortcut::activated, this, [this]() {
        cancelInteractiveDrawingToSelectMode();
    });

    commitDrawShortcut_ = new QShortcut(this);
    commitDrawShortcut_->setKeys(QList<QKeySequence>{QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
    commitDrawShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(commitDrawShortcut_, &QShortcut::activated, this, [this]() {
        commitInteractiveDrawSession();
    });

    mapPaneLayout->addWidget(mapView_, 1);
    toolbar->setParent(mapView_);
    toolbar->show();
    positionMapToolbarOverlay();
    splitter_->addWidget(mapPaneContainer_);
    splitter_->addWidget(textEditor_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes(QList<int>{700, 900});

    layout->addWidget(splitter_, 1);

    refreshMapScene();
    refreshDetachButtonState();
    refreshToolbarIcons();
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

QString MapEditorTab::statusModeText() const
{
    return interactiveDrawMode_ == InteractiveDrawMode::None
        ? tr("Map mode: Select")
        : tr("Map mode: Insert");
}

bool MapEditorTab::isInsertModeActive() const
{
    return interactiveDrawMode_ != InteractiveDrawMode::None;
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
    if (!mapSelectionDrivenTextNavigationInProgress_) {
        syncMapSelectionFromTextCursor(lineNumber, textEditor_ != nullptr ? textEditor_->currentColumnNumber() : 1);
    }
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleTextEditorCursorPositionChanged(int lineNumber, int columnNumber)
{
    if (mapSelectionDrivenTextNavigationInProgress_) {
        return;
    }

    if (lineNumber == lastCursorSyncedLine_ && columnNumber == lastCursorSyncedColumn_) {
        return;
    }

    syncMapSelectionFromTextCursor(lineNumber, columnNumber);
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

void MapEditorTab::updateWorkspaceVisibility()
{
    if (mapPaneDetached_) {
        textEditor_->show();
        refreshStatus();
        return;
    }

    textEditor_->show();
    if (mapPaneContainer_ != nullptr) {
        mapPaneContainer_->show();
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
    if (mapPaneDetached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr) {
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
    if (!mapPaneDetached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr || reattachingMapPane_) {
        return;
    }

    reattachingMapPane_ = true;

    if (detachedMapPaneWindow_ != nullptr && detachedMapPaneWindow_->centralWidget() == mapPaneContainer_) {
        detachedMapPaneWindow_->takeCentralWidget();
    }

    mapPaneContainer_->setParent(splitter_);
    splitter_->insertWidget(0, mapPaneContainer_);

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

void MapEditorTab::refreshToolbarIcons()
{
    const QPalette toolbarPalette = mapToolbarPalette();
    if (mapToolbar_ != nullptr) {
        mapToolbar_->setPalette(toolbarPalette);
        mapToolbar_->setStyleSheet(mapToolbarStyleSheet(toolbarPalette));
    }

    const QList<QToolButton *> buttons = findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button == nullptr) {
            continue;
        }

        const QString iconName = button->property(kLucideIconNameProperty).toString();
        if (iconName.isEmpty()) {
            continue;
        }

        button->setPalette(toolbarPalette);
        button->setIcon(lucideIcon(iconName, toolbarPalette));
    }

    if (zoomLabel_ != nullptr) {
        zoomLabel_->setPalette(toolbarPalette);
    }

    positionMapToolbarOverlay();
}

void MapEditorTab::positionMapToolbarOverlay()
{
    if (mapToolbar_ == nullptr || mapView_ == nullptr) {
        return;
    }

    const QWidget *viewport = mapView_->viewport();
    if (viewport == nullptr) {
        return;
    }

    if (mapToolbar_->parentWidget() != mapView_) {
        mapToolbar_->setParent(mapView_);
    }

    mapToolbar_->adjustSize();
    const QSize hint = mapToolbar_->sizeHint();
    constexpr int kToolbarMargin = 8;
    const QRect viewportGeometry = viewport->geometry();
    const int availableWidth = std::max(0, viewportGeometry.width() - (kToolbarMargin * 2));
    const int toolbarWidth = std::min(hint.width(), availableWidth);
    const int toolbarHeight = hint.height();
    const int x = viewportGeometry.x() + std::max(kToolbarMargin, (viewportGeometry.width() - toolbarWidth) / 2);
    const int y = viewportGeometry.y() + kToolbarMargin;

    mapToolbar_->setGeometry(x, y, toolbarWidth, toolbarHeight);
    mapToolbar_->raise();
    mapToolbar_->show();
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
