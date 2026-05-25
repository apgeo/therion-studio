#include "MainWindow.h"

#include <QAction>
#include <QColor>
#include <QDesktopServices>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QFont>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QHash>
#include <QSizePolicy>
#include <QSplitter>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QKeySequence>
#include <QSvgRenderer>
#include <QWidget>
#include <QResizeEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>
#include <functional>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "MainWindowDocumentHelpers.h"
#include "MainWindowHelpDialog.h"

namespace
{
QPixmap renderLucidePixmap(const QString &iconName, const QColor &color, int extent, qreal devicePixelRatio)
{
    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return QPixmap();
    }

    QPixmap pixmap(QSize(extent, extent) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, extent, extent));
    return pixmap;
}

QIcon themedLucideIcon(const QString &iconName, const QPalette &palette, int extent, qreal devicePixelRatio)
{
    QIcon icon;
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::ButtonText), extent, devicePixelRatio), QIcon::Normal);
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::ButtonText), extent, devicePixelRatio), QIcon::Disabled);
    return icon;
}

QString workspaceCommandBarStyleSheet(const QColor &backgroundColor)
{
    return QStringLiteral(
               "QWidget#workspaceCommandBar {"
               " border-bottom: none;"
               " border-top: none;"
               " border-left: none;"
               " border-right: none;"
               " background-color: %1;"
               "}"
               "QFrame#workspaceToolbarSeparator {"
               " color: palette(mid);"
               " margin-left: 4px;"
               " margin-right: 4px;"
               "}"
               "QWidget#workspaceCommandBar QToolButton {"
               " min-width: 26px;"
               " max-width: 26px;"
               " min-height: 26px;"
               " max-height: 26px;"
               " border: 1px solid palette(mid);"
               " border-radius: 6px;"
               " padding: 0px;"
               " background-color: palette(button);"
               "}"
               "QWidget#workspaceCommandBar QPushButton {"
               " min-height: 26px;"
               " border: 1px solid palette(mid);"
               " border-radius: 6px;"
               " padding: 0 10px;"
               " background-color: palette(button);"
               "}"
               "QWidget#workspaceCommandBar QToolButton:hover,"
               "QWidget#workspaceCommandBar QPushButton:hover {"
               " background-color: palette(light);"
               "}"
               "QWidget#workspaceCommandBar QToolButton:pressed,"
               "QWidget#workspaceCommandBar QToolButton:checked,"
               "QWidget#workspaceCommandBar QPushButton:pressed,"
               "QWidget#workspaceCommandBar QPushButton:checked {"
               " background-color: palette(midlight);"
               "}"
               "QWidget#workspaceCommandBar QToolButton:disabled,"
               "QWidget#workspaceCommandBar QPushButton:disabled {"
               " color: palette(mid);"
               " border-color: palette(mid);"
               "}")
        .arg(backgroundColor.name(QColor::HexRgb));
}

bool isSupportedTextEditorFilePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    const QString fileName = info.fileName();
    if (fileName.compare(QStringLiteral("thconfig"), Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("th")
        || suffix == QStringLiteral("thconfig")
        || suffix == QStringLiteral("txt")
        || suffix == QStringLiteral("log")) {
        return true;
    }

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);
    return mimeType.isValid() && mimeType.inherits(QStringLiteral("text/plain"));
}

bool openFileExternally(QWidget *parent, const QString &filePath)
{
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        return true;
    }

    QMessageBox::warning(parent,
                         QObject::tr("Open in External App"),
                         QObject::tr("Failed to open %1 with the system default application.")
                             .arg(QDir::toNativeSeparators(filePath)));
    return false;
}

void showUnsupportedFilePrompt(QWidget *parent, const QString &filePath)
{
    QMessageBox messageBox(parent);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle(QObject::tr("Unsupported File"));
    messageBox.setText(QObject::tr("Unsupported file."));
    messageBox.setInformativeText(
        QObject::tr("Therion Studio cannot open this file type in the internal editor:\n%1")
            .arg(QDir::toNativeSeparators(filePath)));
    auto *openExternalButton = messageBox.addButton(QObject::tr("Open in External App"), QMessageBox::ActionRole);
    messageBox.addButton(QMessageBox::Cancel);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    messageBox.exec();

    if (messageBox.clickedButton() == openExternalButton) {
        openFileExternally(parent, filePath);
    }
}

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

constexpr auto kWelcomeTabPropertyName = "therionStudioWelcomeTab";

int findWelcomeTabIndex(const QTabWidget *tabs)
{
    if (tabs == nullptr) {
        return -1;
    }

    for (int index = 0; index < tabs->count(); ++index) {
        QWidget *widget = tabs->widget(index);
        if (widget != nullptr && widget->property(kWelcomeTabPropertyName).toBool()) {
            return index;
        }
    }

    return -1;
}

QWidget *createCenteredMessage(const QString &title,
                               const QString &body,
                               const QString &buttonText = QString(),
                               std::function<void()> onButtonClick = {})
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
    if (!buttonText.isEmpty()) {
        auto *button = new QPushButton(buttonText);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QObject::connect(button, &QPushButton::clicked, widget, [onButtonClick]() {
            if (onButtonClick) {
                onButtonClick();
            }
        });
        layout->addWidget(button);
    }

    layout->addStretch(1);

    return widget;
}

QToolButton *createWorkspaceIconButton(QWidget *parent,
                                       const QString &toolTip,
                                       const QString &iconName)
{
    auto *button = new QToolButton(parent);
    button->setAutoRaise(false);
    button->setIconSize(QSize(14, 14));
    button->setIcon(themedLucideIcon(iconName, button->palette(), 14, button->devicePixelRatioF()));
    button->setProperty("lucideIconName", iconName);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setFixedSize(QSize(26, 26));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QFrame *createWorkspaceToolbarSeparator(QWidget *parent)
{
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName(QStringLiteral("workspaceToolbarSeparator"));
    return separator;
}

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setFrameShape(QFrame::NoFrame);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
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

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(TherionStudio::SessionSettingsStore(), parent)
{
}

MainWindow::MainWindow(TherionStudio::SessionSettingsStore sessionStore, QWidget *parent)
    : QMainWindow(parent)
    , editorTabs_(new QTabWidget(this))
    , projectModel_(new QFileSystemModel(this))
    , structureModel_(new QStandardItemModel(this))
    , mapObjectsModel_(new QStandardItemModel(this))
    , sessionStore_(std::move(sessionStore))
    , structureSidebarScanner_(new TherionStudio::ProjectStructureScanner(this))
{
    setWindowTitle(tr("Therion Studio"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AnimatedDocks);

    projectModel_->setRootPath(QDir::rootPath());

    connect(structureSidebarScanner_, &TherionStudio::ProjectStructureScanner::scanFinished,
            this, &MainWindow::handleStructureSidebarScanFinished);

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
    editorTabs_->setObjectName(QStringLiteral("mainEditorTabs"));
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
        refreshWorkspaceModeSwitcher();
        refreshWorkspaceModeSwitcherGeometry();
    });

    auto *mainContentHost = new QWidget(this);
    mainContentLayout_ = new QHBoxLayout(mainContentHost);
    mainContentLayout_->setContentsMargins(0, 0, 0, 0);
    mainContentLayout_->setSpacing(0);
    mainContentSplitter_ = new QSplitter(Qt::Horizontal, mainContentHost);
    applyThinSplitterStyle(mainContentSplitter_, QStringLiteral("mainContentSplitter"));
    mainContentSplitter_->setChildrenCollapsible(true);
    setCentralWidget(mainContentHost);

    editorAreaHost_ = new QWidget(mainContentSplitter_);
    editorAreaHost_->setObjectName(QStringLiteral("mainEditorArea"));
    editorAreaHost_->setAttribute(Qt::WA_StyledBackground, true);
    editorAreaHost_->setStyleSheet(QStringLiteral(
        "QWidget#mainEditorArea {"
        " background-color: palette(base);"
        " border: none;"
        "}"));
    auto *editorAreaHostLayout = new QHBoxLayout(editorAreaHost_);
    editorAreaHostLayout->setContentsMargins(0, 0, 0, 0);
    editorAreaHostLayout->setSpacing(0);

    auto *editorAreaDivider = new QFrame(editorAreaHost_);
    editorAreaDivider->setObjectName(QStringLiteral("mainEditorAreaLeftDivider"));
    editorAreaDivider->setFixedWidth(1);
    editorAreaDivider->setFrameShape(QFrame::NoFrame);
    editorAreaDivider->setStyleSheet(QStringLiteral(
        "QFrame#mainEditorAreaLeftDivider {"
        " background-color: palette(mid);"
        " border: none;"
        "}"));

    editorAreaColumn_ = new QWidget(editorAreaHost_);
    editorAreaColumn_->setObjectName(QStringLiteral("mainEditorAreaColumn"));
    editorAreaColumn_->setAttribute(Qt::WA_StyledBackground, true);
    editorAreaColumn_->setStyleSheet(QStringLiteral(
        "QWidget#mainEditorAreaColumn {"
        " background-color: palette(base);"
        " border: none;"
        "}"));
    editorAreaHostLayout->addWidget(editorAreaDivider);
    editorAreaHostLayout->addWidget(editorAreaColumn_, 1);

    editorAreaLayout_ = new QVBoxLayout(editorAreaColumn_);
    editorAreaLayout_->setContentsMargins(0, 0, 0, 0);
    editorAreaLayout_->setSpacing(0);
    editorAreaLayout_->addWidget(editorTabs_, 1);

    buildStructureSidebar();
    mainContentSplitter_->addWidget(editorAreaHost_);
    mainContentLayout_->addWidget(mainContentSplitter_, 1);
    mainContentSplitter_->setCollapsible(0, true);
    mainContentSplitter_->setCollapsible(1, false);
    mainContentSplitter_->setStretchFactor(0, 0);
    mainContentSplitter_->setStretchFactor(1, 1);
    buildConsole();
    buildMenus();
    initializeWorkspaceModeSwitcher();

    initializeDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
    refreshWorkspaceModeSwitcherGeometry();
    QTimer::singleShot(0, this, [this]() {
        refreshWorkspaceModeSwitcherGeometry();
    });
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::initializeWorkspaceModeSwitcher()
{
    if (editorTabs_ != nullptr) {
        editorTabs_->setObjectName(QStringLiteral("mainEditorTabs"));
        if (!editorTabs_->property("baseStyleSheet").isValid()) {
            editorTabs_->setProperty("baseStyleSheet", editorTabs_->styleSheet());
        }
    }

    QWidget *workspaceHost = editorAreaColumn_ != nullptr ? editorAreaColumn_ : editorAreaHost_;
    if (workspaceHost == nullptr) {
        workspaceHost = centralWidget();
    }
    if (workspaceHost == nullptr) {
        workspaceHost = editorTabs_;
    }
    workspaceModeSwitcher_ = new QWidget(workspaceHost);
    workspaceModeSwitcher_->setObjectName(QStringLiteral("workspaceCommandBar"));
    workspaceModeSwitcher_->setAttribute(Qt::WA_StyledBackground, true);
    workspaceModeSwitcher_->setStyleSheet(workspaceCommandBarStyleSheet(palette().color(QPalette::Base)));
    auto *hostLayout = new QHBoxLayout(workspaceModeSwitcher_);
    hostLayout->setContentsMargins(4, 4, 8, 4);
    hostLayout->setSpacing(4);

    workspaceOpenProjectButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Open Project"), QStringLiteral("folder-open"));
    workspaceCloseProjectButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Close Project"), QStringLiteral("folder-x"));
    workspaceProjectSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    workspaceSaveButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Save"), QStringLiteral("save"));
    workspaceUndoButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Undo"), QStringLiteral("undo-2"));
    workspaceRedoButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Redo"), QStringLiteral("redo-2"));
    workspaceCompileCurrentConfigButton_ =
        createWorkspaceIconButton(workspaceModeSwitcher_, tr("Compile Current Config"), QStringLiteral("play"));
    hostLayout->addWidget(workspaceOpenProjectButton_);
    hostLayout->addWidget(workspaceCloseProjectButton_);
    hostLayout->addWidget(workspaceProjectSeparator_);
    hostLayout->addWidget(workspaceSaveButton_);
    hostLayout->addWidget(createWorkspaceToolbarSeparator(workspaceModeSwitcher_));
    hostLayout->addWidget(workspaceUndoButton_);
    hostLayout->addWidget(workspaceRedoButton_);
    workspaceCompileSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceCompileSeparator_);
    hostLayout->addWidget(workspaceCompileCurrentConfigButton_);
    workspaceHistorySeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceHistorySeparator_);
    workspaceZoomGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *zoomLayout = new QHBoxLayout(workspaceZoomGroup_);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->setSpacing(4);
    workspaceZoomInButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Zoom In"), QStringLiteral("zoom-in"));
    workspaceZoomOutButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Zoom Out"), QStringLiteral("zoom-out"));
    workspaceFitButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Fit"), QStringLiteral("scan"));
    workspaceFitBackgroundButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Fit With Background"), QStringLiteral("image"));
    zoomLayout->addWidget(workspaceZoomInButton_);
    zoomLayout->addWidget(workspaceZoomOutButton_);
    zoomLayout->addWidget(workspaceFitButton_);
    zoomLayout->addWidget(workspaceFitBackgroundButton_);
    hostLayout->addWidget(workspaceZoomGroup_);
    workspaceZoomSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceZoomSeparator_);
    workspaceMapToolsGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *mapToolsLayout = new QHBoxLayout(workspaceMapToolsGroup_);
    mapToolsLayout->setContentsMargins(0, 0, 0, 0);
    mapToolsLayout->setSpacing(4);
    workspaceSelectButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Select"), QStringLiteral("mouse-pointer-2"));
    workspaceCompleteDraftButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Complete Draft"), QStringLiteral("check"));
    workspaceInsertScrapButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Insert Scrap"), QStringLiteral("puzzle"));
    workspacePointButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Point"), QStringLiteral("locate-fixed"));
    workspaceLineButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Line"), QStringLiteral("spline"));
    workspaceFreehandLineButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Freehand"), QStringLiteral("pencil-line"));
    workspaceAreaButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Area"), QStringLiteral("pentagon"));
    workspaceSelectButton_->setCheckable(true);
    workspacePointButton_->setCheckable(true);
    workspaceLineButton_->setCheckable(true);
    workspaceFreehandLineButton_->setCheckable(true);
    workspaceAreaButton_->setCheckable(true);
    mapToolsLayout->addWidget(workspaceSelectButton_);
    mapToolsLayout->addWidget(workspaceCompleteDraftButton_);
    mapToolsLayout->addWidget(createWorkspaceToolbarSeparator(workspaceMapToolsGroup_));
    mapToolsLayout->addWidget(workspaceInsertScrapButton_);
    mapToolsLayout->addWidget(workspacePointButton_);
    mapToolsLayout->addWidget(workspaceLineButton_);
    mapToolsLayout->addWidget(workspaceFreehandLineButton_);
    mapToolsLayout->addWidget(workspaceAreaButton_);
    hostLayout->addWidget(workspaceMapToolsGroup_);
    hostLayout->addStretch(1);

    workspaceMapModeSwitcher_ = new QWidget(workspaceModeSwitcher_);
    auto *mapLayout = new QHBoxLayout(workspaceMapModeSwitcher_);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(4);
    workspaceVisualModeButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Visual"), QStringLiteral("pen-tool"));
    workspaceRawModeButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Raw"), QStringLiteral("code"));
    workspaceVisualModeButton_->setCheckable(true);
    workspaceRawModeButton_->setCheckable(true);
    workspaceMapPaneWindowButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Separate Map"), QStringLiteral("screen-share"));
    workspaceMapPaneWindowButton_->setObjectName(QStringLiteral("workspaceMapPaneWindowButton"));
    mapLayout->addWidget(workspaceVisualModeButton_);
    mapLayout->addWidget(workspaceRawModeButton_);
    mapLayout->addWidget(workspaceMapPaneWindowButton_);

    workspaceTextModeSwitcher_ = new QWidget(workspaceModeSwitcher_);
    auto *textLayout = new QHBoxLayout(workspaceTextModeSwitcher_);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    workspaceTextRawModeButton_ = createWorkspaceIconButton(workspaceTextModeSwitcher_, tr("Raw"), QStringLiteral("code"));
    workspaceBlocksModeButton_ = createWorkspaceIconButton(workspaceTextModeSwitcher_, tr("Blocks"), QStringLiteral("toy-brick"));
    workspaceTextRawModeButton_->setCheckable(true);
    workspaceBlocksModeButton_->setCheckable(true);
    workspaceBlocksModeButton_->setToolTip(tr("Structured block canvas for .th and .thconfig files."));
    workspaceBlocksModeButton_->setAccessibleName(tr("Blocks"));
    textLayout->addWidget(workspaceTextRawModeButton_);
    textLayout->addWidget(workspaceBlocksModeButton_);

    hostLayout->addWidget(workspaceMapModeSwitcher_);
    hostLayout->addWidget(workspaceTextModeSwitcher_);

    connect(workspaceOpenProjectButton_, &QToolButton::clicked, this, [this]() {
        if (openProjectAction_ != nullptr) {
            openProjectAction_->trigger();
        } else {
            openProject();
        }
    });
    connect(workspaceCloseProjectButton_, &QToolButton::clicked, this, [this]() {
        if (closeProjectAction_ != nullptr) {
            closeProjectAction_->trigger();
        } else {
            closeProject();
        }
    });
    connect(workspaceSaveButton_, &QToolButton::clicked, this, &MainWindow::saveActiveDocument);
    connect(workspaceUndoButton_, &QToolButton::clicked, this, &MainWindow::triggerUndoForActiveDocument);
    connect(workspaceRedoButton_, &QToolButton::clicked, this, &MainWindow::triggerRedoForActiveDocument);
    connect(workspaceCompileCurrentConfigButton_, &QToolButton::clicked, this, &MainWindow::triggerCompileCurrentConfigForActiveDocument);
    connect(workspaceZoomInButton_, &QToolButton::clicked, this, &MainWindow::triggerZoomInForActiveDocument);
    connect(workspaceZoomOutButton_, &QToolButton::clicked, this, &MainWindow::triggerZoomOutForActiveDocument);
    connect(workspaceFitButton_, &QToolButton::clicked, this, &MainWindow::triggerFitForActiveDocument);
    connect(workspaceFitBackgroundButton_, &QToolButton::clicked, this, &MainWindow::triggerFitWithBackgroundForActiveDocument);
    connect(workspaceSelectButton_, &QToolButton::clicked, this, &MainWindow::triggerSelectForActiveDocument);
    connect(workspaceCompleteDraftButton_, &QToolButton::clicked, this, &MainWindow::triggerCompleteDraftForActiveDocument);
    connect(workspaceInsertScrapButton_, &QToolButton::clicked, this, &MainWindow::triggerInsertScrapForActiveDocument);
    connect(workspacePointButton_, &QToolButton::clicked, this, &MainWindow::triggerPointForActiveDocument);
    connect(workspaceLineButton_, &QToolButton::clicked, this, &MainWindow::triggerLineForActiveDocument);
    connect(workspaceFreehandLineButton_, &QToolButton::clicked, this, &MainWindow::triggerFreehandLineForActiveDocument);
    connect(workspaceAreaButton_, &QToolButton::clicked, this, &MainWindow::triggerAreaForActiveDocument);
    connect(workspaceVisualModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        }
    });
    connect(workspaceRawModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        }
    });
    connect(workspaceMapPaneWindowButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->toggleMapPaneWindow();
        }
    });
    connect(workspaceTextRawModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *textTab = currentTextTab(); textTab != nullptr) {
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
        }
    });
    connect(workspaceBlocksModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *textTab = currentTextTab(); textTab != nullptr) {
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
        }
    });

    workspaceModeSwitcher_->raise();
    workspaceMapModeSwitcher_->setVisible(false);
    workspaceTextModeSwitcher_->setVisible(false);
    workspaceZoomGroup_->setVisible(false);
    workspaceMapToolsGroup_->setVisible(false);
    workspaceHistorySeparator_->setVisible(false);
    workspaceCompileSeparator_->setVisible(false);
    workspaceCompileCurrentConfigButton_->setVisible(false);
    workspaceZoomSeparator_->setVisible(false);
    workspaceModeSwitcher_->setVisible(true);
    if (editorAreaLayout_ != nullptr) {
        editorAreaLayout_->insertWidget(0, workspaceModeSwitcher_);
    }
}

void MainWindow::refreshWorkspaceModeSwitcher()
{
    if (workspaceModeSwitcher_ == nullptr
        || workspaceMapModeSwitcher_ == nullptr
        || workspaceTextModeSwitcher_ == nullptr
        || workspaceOpenProjectButton_ == nullptr
        || workspaceCloseProjectButton_ == nullptr
        || workspaceProjectSeparator_ == nullptr
        || workspaceVisualModeButton_ == nullptr
        || workspaceRawModeButton_ == nullptr
        || workspaceMapPaneWindowButton_ == nullptr
        || workspaceSaveButton_ == nullptr
        || workspaceUndoButton_ == nullptr
        || workspaceRedoButton_ == nullptr
        || workspaceCompileCurrentConfigButton_ == nullptr
        || workspaceZoomGroup_ == nullptr
        || workspaceZoomInButton_ == nullptr
        || workspaceZoomOutButton_ == nullptr
        || workspaceFitButton_ == nullptr
        || workspaceFitBackgroundButton_ == nullptr
        || workspaceMapToolsGroup_ == nullptr
        || workspaceSelectButton_ == nullptr
        || workspaceCompleteDraftButton_ == nullptr
        || workspaceInsertScrapButton_ == nullptr
        || workspacePointButton_ == nullptr
        || workspaceLineButton_ == nullptr
        || workspaceFreehandLineButton_ == nullptr
        || workspaceAreaButton_ == nullptr
        || workspaceHistorySeparator_ == nullptr
        || workspaceCompileSeparator_ == nullptr
        || workspaceZoomSeparator_ == nullptr
        || workspaceTextRawModeButton_ == nullptr
        || workspaceBlocksModeButton_ == nullptr) {
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    refreshWorkspaceIconTheme();
    auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget);
    auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget);
    const bool showMapModes = mapTab != nullptr;
    const bool showTextModes = textTab != nullptr;
    const bool showCompileCurrentConfig = showTextModes && !currentDocumentTherionConfigPath().isEmpty();
    const bool mapPaneDetached = mapTab != nullptr && mapTab->isMapPaneDetached();
    const bool showZoomTools = showMapModes && !mapPaneDetached;
    const bool showMapTools = showMapModes && !mapPaneDetached;
    QColor commandBarBackground = palette().color(QPalette::Base);
    if (showTextModes && textTab != nullptr) {
        const QColor sourceSurface = textTab->sourceSurfaceColor();
        if (sourceSurface.isValid()) {
            commandBarBackground = sourceSurface;
        }
    }
    workspaceModeSwitcher_->setStyleSheet(workspaceCommandBarStyleSheet(commandBarBackground));

    workspaceModeSwitcher_->setVisible(true);
    workspaceMapModeSwitcher_->setVisible(showMapModes);
    workspaceTextModeSwitcher_->setVisible(showTextModes);
    workspaceOpenProjectButton_->setVisible(true);
    workspaceCloseProjectButton_->setVisible(true);
    workspaceProjectSeparator_->setVisible(true);
    workspaceHistorySeparator_->setVisible(showZoomTools);
    workspaceZoomGroup_->setVisible(showZoomTools);
    workspaceZoomSeparator_->setVisible(showZoomTools);
    workspaceMapToolsGroup_->setVisible(showMapTools);
    workspaceSaveButton_->setEnabled(tabWidget != nullptr);
    const bool canUndo = documentCanUndoForWidget(tabWidget);
    const bool canRedo = documentCanRedoForWidget(tabWidget);
    workspaceUndoButton_->setEnabled(canUndo);
    workspaceRedoButton_->setEnabled(canRedo);
    if (undoAction_ != nullptr) {
        undoAction_->setEnabled(canUndo);
    }
    if (redoAction_ != nullptr) {
        redoAction_->setEnabled(canRedo);
    }
    workspaceCompileSeparator_->setVisible(showCompileCurrentConfig);
    workspaceCompileCurrentConfigButton_->setVisible(showCompileCurrentConfig);
    workspaceCompileCurrentConfigButton_->setEnabled(showCompileCurrentConfig);
    workspaceZoomInButton_->setEnabled(showZoomTools);
    workspaceZoomOutButton_->setEnabled(showZoomTools);
    workspaceFitButton_->setEnabled(showZoomTools);
    workspaceFitBackgroundButton_->setEnabled(showZoomTools && mapTab != nullptr && mapTab->backgroundLayerCount() > 0);
    workspaceSelectButton_->setEnabled(showMapTools);
    workspaceCompleteDraftButton_->setEnabled(showMapTools && mapTab != nullptr && mapTab->canCompleteDraftAction());
    workspaceInsertScrapButton_->setEnabled(showMapTools);
    workspacePointButton_->setEnabled(showMapTools);
    workspaceLineButton_->setEnabled(showMapTools);
    workspaceFreehandLineButton_->setEnabled(showMapTools);
    workspaceAreaButton_->setEnabled(showMapTools);

    workspaceModeSwitcherSyncInProgress_ = true;
    if (showMapModes) {
        const QSignalBlocker selectBlocker(workspaceSelectButton_);
        const QSignalBlocker pointBlocker(workspacePointButton_);
        const QSignalBlocker lineBlocker(workspaceLineButton_);
        const QSignalBlocker freehandBlocker(workspaceFreehandLineButton_);
        const QSignalBlocker areaBlocker(workspaceAreaButton_);
        const QSignalBlocker visualBlocker(workspaceVisualModeButton_);
        const QSignalBlocker rawBlocker(workspaceRawModeButton_);
        const QSignalBlocker mapWindowBlocker(workspaceMapPaneWindowButton_);
        const TherionStudio::MapEditorTab::InteractiveDrawMode drawMode = mapTab->interactiveDrawMode();
        workspaceSelectButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::None);
        workspacePointButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Point);
        workspaceLineButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Line);
        workspaceFreehandLineButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Freehand);
        workspaceAreaButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Area);
        workspaceVisualModeButton_->setChecked(mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        workspaceRawModeButton_->setChecked(mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        workspaceVisualModeButton_->setEnabled(!mapPaneDetached);
        workspaceRawModeButton_->setEnabled(!mapPaneDetached);
        workspaceMapPaneWindowButton_->setEnabled(true);
        workspaceMapPaneWindowButton_->setProperty("lucideIconName",
                                                   mapPaneDetached
                                                       ? QStringLiteral("monitor-x")
                                                       : QStringLiteral("screen-share"));
        refreshWorkspaceIconTheme();
        workspaceMapPaneWindowButton_->setToolTip(mapTab->mapPaneWindowActionToolTip());
        workspaceMapPaneWindowButton_->setAccessibleName(mapTab->mapPaneWindowActionText());
        workspaceModeSwitcher_->setToolTip(mapPaneDetached
                                               ? tr("Map pane is detached: raw editor remains in this tab while visual map stays in the detached window.")
                                               : QString());
    } else if (showTextModes) {
        const QSignalBlocker rawTextBlocker(workspaceTextRawModeButton_);
        const QSignalBlocker blocksBlocker(workspaceBlocksModeButton_);
        const bool blocksActive = textTab->editorMode() == TherionStudio::TextEditorTab::EditorMode::Blocks;
        workspaceTextRawModeButton_->setChecked(!blocksActive);
        workspaceBlocksModeButton_->setChecked(blocksActive);
        workspaceTextRawModeButton_->setEnabled(true);
        workspaceBlocksModeButton_->setEnabled(textTab->isBlocksModeAvailable());
        workspaceModeSwitcher_->setToolTip(QString());
    } else {
        workspaceModeSwitcher_->setToolTip(QString());
    }
    workspaceModeSwitcherSyncInProgress_ = false;
    refreshWorkspaceModeSwitcherGeometry();
}

void MainWindow::refreshWorkspaceIconTheme()
{
    if (workspaceModeSwitcher_ == nullptr) {
        return;
    }

    const QPalette palette = workspaceModeSwitcher_->palette();
    const qreal devicePixelRatio = workspaceModeSwitcher_->devicePixelRatioF();
    const QList<QToolButton *> buttons = workspaceModeSwitcher_->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button == nullptr) {
            continue;
        }

        const QString iconName = button->property("lucideIconName").toString();
        if (iconName.isEmpty()) {
            continue;
        }

        button->setIcon(themedLucideIcon(iconName, palette, button->iconSize().width(), devicePixelRatio));
    }
}

void MainWindow::refreshWorkspaceModeSwitcherGeometry()
{
    if (workspaceModeSwitcher_ == nullptr
        || workspaceModeSwitcher_->parentWidget() == nullptr
        || editorTabs_ == nullptr
        || editorTabs_->tabBar() == nullptr) {
        return;
    }

    workspaceModeSwitcher_->updateGeometry();
}

void MainWindow::triggerUndoForActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    if (documentUndoForWidget(tabWidget)) {
        refreshWorkspaceModeSwitcher();
    }
}

void MainWindow::triggerRedoForActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    if (documentRedoForWidget(tabWidget)) {
        refreshWorkspaceModeSwitcher();
    }
}

void MainWindow::triggerCompileCurrentConfigForActiveDocument()
{
    runTherionCurrentConfig();
}

void MainWindow::triggerZoomInForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerZoomIn();
    }
}

void MainWindow::triggerZoomOutForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerZoomOut();
    }
}

void MainWindow::triggerFitForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerFit();
    }
}

void MainWindow::triggerFitWithBackgroundForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerFitWithBackground();
    }
}

void MainWindow::triggerSelectForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerSelectMode();
    }
}

void MainWindow::triggerCompleteDraftForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerCompleteDraft();
    }
}

void MainWindow::triggerInsertScrapForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerInsertScrap();
    }
}

void MainWindow::triggerPointForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddPoint();
    }
}

void MainWindow::triggerLineForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddLine();
    }
}

void MainWindow::triggerFreehandLineForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddFreehandLine();
    }
}

void MainWindow::triggerAreaForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddArea();
    }
}

void MainWindow::initializeDocumentStatusWidgets()
{
    if (statusBar() == nullptr
        || statusMapZoomLabel_ != nullptr
        || statusMapModeLabel_ != nullptr
        || statusCompilerButton_ != nullptr
        || statusDocumentEncodingLabel_ != nullptr) {
        return;
    }

    statusDocumentEncodingLabel_ = new QLabel(statusBar());
    statusDocumentEncodingLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusDocumentEncodingLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusDocumentEncodingLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    statusMapZoomLabel_ = new QLabel(statusBar());
    statusMapZoomLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapZoomLabel_->setAlignment(Qt::AlignCenter);
    statusMapZoomLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapZoomLabel_->setVisible(false);

    statusMapModeLabel_ = new QLabel(statusBar());
    statusMapModeLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapModeLabel_->setAlignment(Qt::AlignCenter);
    statusMapModeLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapModeLabel_->setVisible(false);

    statusCompilerButton_ = new QToolButton(statusBar());
    statusCompilerButton_->setAutoRaise(true);
    statusCompilerButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    statusCompilerButton_->setCursor(Qt::PointingHandCursor);
    statusCompilerButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusCompilerButton_->setToolTip(tr("Open Compiler output"));
    connect(statusCompilerButton_, &QToolButton::clicked, this, [this]() {
        const bool compilerSidebarVisible = sidebarContainer_ != nullptr
            && sidebarContainer_->isVisible()
            && sidebarContentContainer_ != nullptr
            && sidebarContentContainer_->isVisible()
            && !sidebarCollapsed_
            && activeSidebarPane_ == SidebarPane::Console;
        if (compilerSidebarVisible) {
            setSidebarCollapsed(true);
            return;
        }

        showSidebarPane(SidebarPane::Console);
    });

    statusBar()->addPermanentWidget(statusMapZoomLabel_, 0);
    statusBar()->addPermanentWidget(statusMapModeLabel_, 0);
    statusBar()->addPermanentWidget(statusCompilerButton_, 0);
    statusBar()->addPermanentWidget(statusDocumentEncodingLabel_, 0);
    setCompilerStatusIdle();
    refreshDocumentStatusWidgets();
}

void MainWindow::refreshDocumentStatusWidgets()
{
    if (statusMapZoomLabel_ == nullptr
        || statusMapModeLabel_ == nullptr
        || statusDocumentEncodingLabel_ == nullptr) {
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    QString encodingText;
    QString mapModeText;
    QString mapZoomText;
    bool mapModeVisible = false;
    bool mapModeInsertActive = false;

    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        encodingText = textTab->statusEncodingText();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        encodingText = mapTab->statusEncodingText();
        if (!mapTab->isMapPaneDetached()) {
            mapModeText = mapTab->statusModeText();
            mapZoomText = tr("%1%").arg(mapTab->zoomPercent());
            mapModeVisible = true;
            mapModeInsertActive = mapTab->isInsertModeActive();
        }
    }

    if (encodingText.trimmed().isEmpty()) {
        statusDocumentEncodingLabel_->clear();
    } else {
        statusDocumentEncodingLabel_->setText(tr("Encoding: %1").arg(encodingText));
    }

    if (!mapModeVisible) {
        statusMapZoomLabel_->clear();
        statusMapZoomLabel_->setToolTip(QString());
        statusMapZoomLabel_->setVisible(false);
        statusMapModeLabel_->clear();
        statusMapModeLabel_->setToolTip(QString());
        statusMapModeLabel_->setStyleSheet(QString());
        statusMapModeLabel_->setVisible(false);
    } else {
        statusMapZoomLabel_->setText(mapZoomText);
        statusMapZoomLabel_->setToolTip(tr("Map zoom"));
        statusMapZoomLabel_->setVisible(true);

        const QString badgeText = mapModeInsertActive ? tr("Insert") : tr("Select");
        const QString background = mapModeInsertActive
            ? QStringLiteral("#d34a4a")
            : QStringLiteral("#2e9f5c");
        statusMapModeLabel_->setText(badgeText);
        statusMapModeLabel_->setToolTip(mapModeText);
        statusMapModeLabel_->setStyleSheet(QStringLiteral(
                                               "QLabel {"
                                               " color: white;"
                                               " font-weight: 700;"
                                               " background-color: %1;"
                                               " border-radius: 4px;"
                                               " padding: 1px 8px;"
                                               " min-height: 18px;"
                                               "}").arg(background));
        statusMapModeLabel_->setVisible(true);
    }
}

void MainWindow::setCompilerStatusIdle()
{
    updateCompilerStatusButton(tr("Compiler: Idle"),
                               tr("Therion compiler is idle. Click to open Compiler output."),
                               QStringLiteral("#6c757d"));
}

void MainWindow::setCompilerStatusRunning(const QString &configPath)
{
    const QString toolTip = configPath.isEmpty()
        ? tr("Therion is running. Click to open Compiler output.")
        : tr("Therion is running %1. Click to open Compiler output.").arg(QDir::toNativeSeparators(configPath));
    updateCompilerStatusButton(tr("Compiler: Running..."), toolTip, QStringLiteral("#2f80ed"));
}

void MainWindow::setCompilerStatusResult(bool success, const QString &details)
{
    const QString finishedAt = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    const QString statusText = success
        ? tr("Compiler: OK %1").arg(finishedAt)
        : tr("Compiler: Failed %1").arg(finishedAt);
    const QString toolTip = details.trimmed().isEmpty()
        ? tr("Last Therion run finished at %1. Click to open Compiler output.").arg(finishedAt)
        : tr("%1 Click to open Compiler output.").arg(details);
    updateCompilerStatusButton(statusText,
                               toolTip,
                               success ? QStringLiteral("#2e9f5c") : QStringLiteral("#c0392b"));
}

void MainWindow::updateCompilerStatusButton(const QString &text,
                                            const QString &toolTip,
                                            const QString &accentColor)
{
    if (statusCompilerButton_ == nullptr) {
        return;
    }

    statusCompilerButton_->setText(text);
    statusCompilerButton_->setToolTip(toolTip);
    statusCompilerButton_->setStyleSheet(QStringLiteral(
                                             "QToolButton {"
                                             " color: white;"
                                             " font-weight: 700;"
                                             " background-color: %1;"
                                             " border: none;"
                                             " border-radius: 4px;"
                                             " padding: 1px 8px;"
                                             " min-height: 18px;"
                                             "}").arg(accentColor));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcherGeometry();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        refreshWorkspaceIconTheme();
        rebuildStructureSidebar();
        rebuildMapObjectsTree();
        refreshMapBackgroundPanel();
        break;
    default:
        break;
    }
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *newWindowAction = fileMenu->addAction(tr("New &Window"));
    newWindowAction->setShortcut(QKeySequence::New);
    connect(newWindowAction, &QAction::triggered, this, &MainWindow::createNewWindow);

    openProjectAction_ = fileMenu->addAction(tr("&Open Project..."));
    openProjectAction_->setShortcut(QKeySequence::Open);
    connect(openProjectAction_, &QAction::triggered, this, &MainWindow::openProject);

    closeProjectAction_ = fileMenu->addAction(tr("&Close Project"));
    connect(closeProjectAction_, &QAction::triggered, this, &MainWindow::closeProject);
    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveActiveDocument);

    QAction *saveAllAction = fileMenu->addAction(tr("Save &All"));
    saveAllAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAllAction, &QAction::triggered, this, &MainWindow::saveAllDocuments);

    QAction *closeTabAction = fileMenu->addAction(tr("&Close"));
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeActiveTab);

    QAction *closeAllTabsAction = fileMenu->addAction(tr("Close All Tabs"));
    closeAllTabsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(closeAllTabsAction, &QAction::triggered, this, &MainWindow::closeAllTabs);

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = editMenu->addAction(tr("&Undo"));
    undoAction_->setShortcuts(QKeySequence::Undo);
    undoAction_->setStatusTip(tr("Undo the last change in the active document."));
    connect(undoAction_, &QAction::triggered, this, &MainWindow::triggerUndoForActiveDocument);

    redoAction_ = editMenu->addAction(tr("&Redo"));
    redoAction_->setShortcuts(QKeySequence::Redo);
    redoAction_->setStatusTip(tr("Redo the last undone change in the active document."));
    connect(redoAction_, &QAction::triggered, this, &MainWindow::triggerRedoForActiveDocument);
    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("&Find"));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() { showFindBar(false); });

    QAction *replaceAction = editMenu->addAction(tr("Find and &Replace"));
    replaceAction->setShortcut(QKeySequence::Replace);
    connect(replaceAction, &QAction::triggered, this, [this]() { showFindBar(true); });

    QMenu *mapMenu = menuBar()->addMenu(tr("&Map"));
    openMapEditorAction_ = mapMenu->addAction(tr("Open Current Document in &Map Editor"));
    openMapEditorAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_G));
    connect(openMapEditorAction_, &QAction::triggered, this, &MainWindow::openCurrentDocumentInMapEditor);
    openMapEditorAction_->setEnabled(false);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    showSidebarAction_ = viewMenu->addAction(tr("Show Sidebar"));
    showSidebarAction_->setCheckable(true);
    showSidebarAction_->setChecked(true);
    showSidebarAction_->setStatusTip(tr("Show or hide the sidebar panel"));
    connect(showSidebarAction_, &QAction::toggled, this, [this](bool visible) {
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
    QAction *showConsolePaneAction = viewMenu->addAction(tr("Show Compiler"));
    showConsolePaneAction->setStatusTip(tr("Switch the sidebar to the compiler pane"));
    connect(showConsolePaneAction, &QAction::triggered, this, [this]() {
        showSidebarPane(SidebarPane::Console);
    });

    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
    QAction *windowNewWindowAction = windowMenu->addAction(tr("New &Window"));
    connect(windowNewWindowAction, &QAction::triggered, this, &MainWindow::createNewWindow);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *quickManualAction = helpMenu->addAction(tr("Quick User Manual"));
    connect(quickManualAction, &QAction::triggered, this, [this]() {
        TherionStudio::showQuickUserManualDialog(this);
    });
    QAction *fullManualAction = helpMenu->addAction(tr("User Manual (Full)"));
    connect(fullManualAction, &QAction::triggered, this, [this]() {
        TherionStudio::showFullUserManualDialog(this);
    });
    helpMenu->addSeparator();
    QAction *aboutAction = helpMenu->addAction(tr("About Therion Studio"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this,
                                 tr("About Therion Studio"),
                                 tr("Therion Studio is a Qt-based editor for Therion projects."));
    });

    updateProjectActionState();
}

void MainWindow::restoreSessionState()
{
    const QByteArray geometry = sessionStore_.mainWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray state = sessionStore_.mainWindowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    const QString lastProjectPath = sessionStore_.lastProjectPath();
    if (!lastProjectPath.isEmpty() && QDir(lastProjectPath).exists() && !isProtectedMacUserFolder(lastProjectPath)) {
        projectRootPath_ = lastProjectPath;
        projectModel_->setRootPath(projectRootPath_);
        projectTree_->setRootIndex(projectModel_->index(projectRootPath_));
        appendConsoleLine(tr("Restored project root %1").arg(projectRootPath_));
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
    sessionStore_.setMainWindowGeometry(saveGeometry());
    sessionStore_.setMainWindowState(saveState());
    sessionStore_.setLastProjectPath(projectRootPath_);
    sessionStore_.setTherionExecutablePath(therionExecutableEdit_ != nullptr ? therionExecutableEdit_->text().trimmed() : QString());
    sessionStore_.setTherionWorkingDirectory(therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString());
    sessionStore_.setTherionArguments(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString());
    sessionStore_.setTherionRunTargetMode(therionRunTargetMode());
    sessionStore_.setTherionTargetConfigPath(therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text().trimmed() : QString());
    saveStructureNameOverrides();
    persistOpenDocuments();
}

void MainWindow::restoreOpenDocuments()
{
    const QStringList openDocumentPaths = sessionStore_.openDocumentPaths();
    if (openDocumentPaths.isEmpty()) {
        return;
    }

    const QString activeDocumentPath = sessionStore_.activeDocumentPath();
    bool skippedUnsupportedDocument = false;

    for (const QString &documentPath : openDocumentPaths) {
        if (documentPath.isEmpty() || isProtectedMacUserFolder(documentPath)) {
            continue;
        }

        if (QFileInfo(documentPath).suffix().toLower() == QStringLiteral("th2")) {
            openMapEditorTab(documentPath);
        } else {
            if (openTextTab(documentPath, false) == nullptr) {
                skippedUnsupportedDocument = true;
                appendConsoleLine(tr("Skipped unsupported document during session restore: %1")
                                      .arg(QDir::toNativeSeparators(documentPath)));
            }
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

    if (skippedUnsupportedDocument) {
        persistOpenDocuments();
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

    sessionStore_.setOpenDocumentPaths(documentPaths);

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

    sessionStore_.setActiveDocumentPath(activeDocumentPath);
}

void MainWindow::addWelcomeTab()
{
    QWidget *welcomeWidget = nullptr;
    if (projectRootPath_.isEmpty()) {
        welcomeWidget = createCenteredMessage(tr("Therion Studio"),
                                              tr("Open a project to begin working with Therion documents, maps, and structure views."),
                                              tr("Open Project..."),
                                              [this]() {
                                                  openProject();
                                              });
    } else {
        welcomeWidget = createCenteredMessage(tr("Therion Studio"),
                                              tr("Open file from sidebar to begin editing this project."));
    }
    welcomeWidget->setProperty(kWelcomeTabPropertyName, true);

    const int existingWelcomeIndex = findWelcomeTabIndex(editorTabs_);
    if (existingWelcomeIndex >= 0) {
        const bool wasCurrent = editorTabs_->currentIndex() == existingWelcomeIndex;
        QWidget *existingWidget = editorTabs_->widget(existingWelcomeIndex);
        editorTabs_->removeTab(existingWelcomeIndex);
        if (existingWidget != nullptr) {
            existingWidget->deleteLater();
        }
        editorTabs_->insertTab(existingWelcomeIndex, welcomeWidget, tr("Welcome"));
        if (wasCurrent) {
            editorTabs_->setCurrentIndex(existingWelcomeIndex);
        }
    } else {
        editorTabs_->addTab(welcomeWidget, tr("Welcome"));
    }

    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
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
    loadStructureNameOverrides();
    syncOpenDocumentsToProjectRoot();
    sessionStore_.setLastProjectPath(projectRootPath_);
    rebuildStructureSidebar();
    refreshTherionConfigDisplay();
    updateProjectActionState();
    if (editorTabs_->count() == 0 || findWelcomeTabIndex(editorTabs_) >= 0) {
        addWelcomeTab();
    }
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

    const QString closedProjectPath = projectRootPath_;
    projectRootPath_.clear();

    clearDocumentTabs();
    resetProjectBrowser();
    sessionStore_.setLastProjectPath(QString());
    persistOpenDocuments();
    rebuildStructureSidebar();
    statusBar()->showMessage(tr("Project closed"), 3000);
    appendConsoleLine(tr("Closed project %1").arg(closedProjectPath));
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
    } else if (isSupportedTextEditorFilePath(filePath)) {
        openTextTab(filePath);
    } else {
        showUnsupportedFilePrompt(this, filePath);
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

void MainWindow::refreshProjectBrowserView(const QString &focusPath)
{
    if (projectModel_ == nullptr || projectTree_ == nullptr) {
        return;
    }

    const QString rootPath = projectRootPath_.isEmpty() ? QDir::rootPath() : projectRootPath_;
    projectModel_->setRootPath(rootPath);
    projectTree_->setRootIndex(projectModel_->index(rootPath));

    const QString normalizedFocusPath = focusPath.trimmed();
    if (normalizedFocusPath.isEmpty()) {
        return;
    }

    const QString absoluteFocusPath = QFileInfo(normalizedFocusPath).absoluteFilePath();
    QTimer::singleShot(0, this, [this, absoluteFocusPath]() {
        if (projectModel_ == nullptr || projectTree_ == nullptr) {
            return;
        }

        QModelIndex targetIndex = projectModel_->index(absoluteFocusPath);
        if (!targetIndex.isValid()) {
            return;
        }

        QModelIndex parentIndex = targetIndex.parent();
        while (parentIndex.isValid() && parentIndex != projectTree_->rootIndex()) {
            projectTree_->expand(parentIndex);
            parentIndex = parentIndex.parent();
        }

        projectTree_->scrollTo(targetIndex);
        projectTree_->setCurrentIndex(targetIndex);
    });
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
    structureNameOverrides_ = loadStructureNameOverridesFromJson(sessionStore_.structureNameOverrides());
}

void MainWindow::saveStructureNameOverrides()
{
    sessionStore_.setStructureNameOverrides(structureNameOverridesToJson(structureNameOverrides_));
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

TherionStudio::TextEditorTab *MainWindow::openTextTab(const QString &filePath, bool showUnsupportedPrompt)
{
    const QString canonicalPath = canonicalDocumentPath(filePath);
    if (!isSupportedTextEditorFilePath(canonicalPath)) {
        if (showUnsupportedPrompt) {
            showUnsupportedFilePrompt(this, canonicalPath);
        }
        return nullptr;
    }

    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(index));
        if (existingTab != nullptr && existingTab->filePath() == canonicalPath) {
            existingTab->setProjectRootPath(projectRootPath_);
            existingTab->setModeSelectorVisible(false);
            editorTabs_->setCurrentIndex(index);
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
            return existingTab;
        }
    }

    auto *tab = new TherionStudio::TextEditorTab;
    tab->setModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    const int welcomeTabIndex = findWelcomeTabIndex(editorTabs_);
    if (welcomeTabIndex >= 0) {
        QWidget *welcomeWidget = editorTabs_->widget(welcomeTabIndex);
        editorTabs_->removeTab(welcomeTabIndex);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
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
            refreshWorkspaceModeSwitcher();
        }
    });

    connect(tab, &TherionStudio::TextEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::TextEditorTab::documentTextChanged, this, [this, tab]() {
        if (!projectRootPath_.isEmpty()) {
            rebuildStructureSidebar();
        }
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::TextEditorTab::editorModeChanged, this, [this, tab](TherionStudio::TextEditorTab::EditorMode) {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
        }
    });

    handleTextEditorCurrentLineChanged(tab->filePath(), tab->currentLineNumber());

    updateTabTitle(tab);
    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
    persistOpenDocuments();
    appendConsoleLine(tr("Opened %1").arg(canonicalPath));
    return tab;
}

TherionStudio::MapEditorTab *MainWindow::openMapEditorTab(const QString &filePath)
{
    const QString canonicalPath = canonicalDocumentPath(filePath);

    if (auto *detachedTab = detachedMapEditorTabForPath(canonicalPath); detachedTab != nullptr) {
        detachedTab->setProjectRootPath(projectRootPath_);
        detachedTab->setInlineWorkspaceModeSelectorVisible(true);
        focusDetachedMapEditorTab(canonicalPath);
        refreshDocumentStatusWidgets();
        refreshWorkspaceModeSwitcher();
        return detachedTab;
    }

    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index));
        if (existingTab != nullptr && existingTab->filePath() == canonicalPath) {
            existingTab->setProjectRootPath(projectRootPath_);
            existingTab->setInlineWorkspaceModeSelectorVisible(false);
            editorTabs_->setCurrentIndex(index);
            connect(existingTab,
                    &TherionStudio::MapEditorTab::openDedicatedWindowRequested,
                    this,
                    &MainWindow::handleMapEditorDetachRequested,
                    Qt::UniqueConnection);
            connect(existingTab,
                    &TherionStudio::MapEditorTab::workspaceModeChanged,
                    this,
                    [this, existingTab](TherionStudio::MapEditorTab::WorkspaceMode) {
                        if (currentDocumentWidget() == existingTab) {
                            refreshWorkspaceModeSwitcher();
                        }
                    },
                    Qt::UniqueConnection);
            connect(existingTab,
                    &TherionStudio::MapEditorTab::mapPaneDetachStateChanged,
                    this,
                    [this, existingTab](bool) {
                        if (currentDocumentWidget() == existingTab) {
                            refreshDocumentStatusWidgets();
                            refreshWorkspaceModeSwitcher();
                        }
                    },
                    Qt::UniqueConnection);
            connect(existingTab,
                    &TherionStudio::MapEditorTab::commandSurfaceStateChanged,
                    this,
                    [this, existingTab]() {
                        if (currentDocumentWidget() == existingTab) {
                            refreshWorkspaceModeSwitcher();
                        }
                    },
                    Qt::UniqueConnection);
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
            return existingTab;
        }
    }

    auto *tab = new TherionStudio::MapEditorTab(sessionStore_);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    const int welcomeTabIndex = findWelcomeTabIndex(editorTabs_);
    if (welcomeTabIndex >= 0) {
        QWidget *welcomeWidget = editorTabs_->widget(welcomeTabIndex);
        editorTabs_->removeTab(welcomeTabIndex);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
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
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
    });
    connect(tab, &TherionStudio::MapEditorTab::documentTextChanged, this, [this, tab]() {
        if (!projectRootPath_.isEmpty()) {
            rebuildStructureSidebar();
        }
        if (currentDocumentWidget() == tab) {
            rebuildMapObjectsTree();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayersChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::modeStatusChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::zoomStatusChanged, this, [this, tab](int) {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::workspaceModeChanged, this, [this, tab](TherionStudio::MapEditorTab::WorkspaceMode) {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::mapPaneDetachStateChanged, this, [this, tab](bool) {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::commandSurfaceStateChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
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
    refreshWorkspaceModeSwitcher();
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
        refreshWorkspaceModeSwitcher();
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
    tab->setInlineWorkspaceModeSelectorVisible(true);
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

    refreshWorkspaceModeSwitcher();
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
    connect(tab,
            &TherionStudio::MapEditorTab::workspaceModeChanged,
            this,
            [this, tab](TherionStudio::MapEditorTab::WorkspaceMode) {
                if (currentDocumentWidget() == tab) {
                    refreshWorkspaceModeSwitcher();
                }
            },
            Qt::UniqueConnection);
    connect(tab,
            &TherionStudio::MapEditorTab::mapPaneDetachStateChanged,
            this,
            [this, tab](bool) {
                if (currentDocumentWidget() == tab) {
                    refreshDocumentStatusWidgets();
                    refreshWorkspaceModeSwitcher();
                }
            },
            Qt::UniqueConnection);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    updateTabTitle(tab);
    refreshWorkspaceModeSwitcher();
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
