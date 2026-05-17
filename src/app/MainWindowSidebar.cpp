#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardItemModel>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace
{
bool isTherionProjectFilePath(const QString &filePath);

QIcon therionBadgedFileIcon(const QIcon &baseIcon)
{
    QIcon composed = baseIcon;
    const QList<int> iconSizes = {16, 18, 20, 22, 24, 32};
    for (const int size : iconSizes) {
        QPixmap pixmap = baseIcon.pixmap(size, size);
        if (pixmap.isNull()) {
            pixmap = QPixmap(size, size);
            pixmap.fill(Qt::transparent);
        }

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const int badgeWidth = qMax(9, (size / 2) + 2);
        const int badgeHeight = qMax(8, (size / 2) + 1);
        const QRect badgeRect(size - badgeWidth, size - badgeHeight, badgeWidth, badgeHeight);

        painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
        painter.setBrush(QColor(QStringLiteral("#0b7ac8")));
        painter.drawRoundedRect(badgeRect.adjusted(0, 0, -1, -1), 3.0, 3.0);

        QFont badgeFont = painter.font();
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(qMax(5.0, badgeHeight * 0.52));
        painter.setFont(badgeFont);
        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("TH"));
        painter.end();

        composed.addPixmap(pixmap, QIcon::Normal, QIcon::Off);
    }

    return composed;
}

class ProjectTreeItemDelegate final : public QStyledItemDelegate
{
public:
    explicit ProjectTreeItemDelegate(const QFileSystemModel *model, QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , model_(model)
    {
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (model_ == nullptr || option == nullptr || index.column() != 0) {
            return;
        }

        if (model_->isDir(index)) {
            return;
        }

        const QString filePath = model_->filePath(index);
        if (filePath.isEmpty()) {
            return;
        }

        if (!isTherionProjectFilePath(filePath)) {
            return;
        }

        ensureTherionIcon(option->widget);
        option->icon = therionFileIcon_;
    }

private:
    void ensureTherionIcon(const QWidget *widget) const
    {
        if (therionIconInitialized_) {
            return;
        }

        const QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
        const QIcon baseFileIcon = style != nullptr ? style->standardIcon(QStyle::SP_FileIcon) : QIcon();
        therionFileIcon_ = therionBadgedFileIcon(baseFileIcon);
        therionIconInitialized_ = true;
    }

    const QFileSystemModel *model_ = nullptr;
    mutable bool therionIconInitialized_ = false;
    mutable QIcon therionFileIcon_;
};

class PaletteEventFilter final : public QObject
{
public:
    explicit PaletteEventFilter(std::function<void()> callback, QObject *parent = nullptr)
        : QObject(parent)
        , callback_(std::move(callback))
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == qApp
            && event != nullptr
            && (event->type() == QEvent::ApplicationPaletteChange
                || event->type() == QEvent::PaletteChange
                || event->type() == QEvent::StyleChange)) {
            if (callback_) {
                callback_();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> callback_;
};

bool isTherionProjectFilePath(const QString &filePath)
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
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2")
        || suffix == QStringLiteral("thconfig");
}

QString duplicateFilePath(const QString &sourcePath)
{
    const QFileInfo sourceInfo(sourcePath);
    const QString directoryPath = sourceInfo.dir().absolutePath();
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();
    const QString extension = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;

    for (int counter = 1; counter < 1000; ++counter) {
        const QString candidateName = counter == 1
            ? QStringLiteral("%1 copy%2").arg(baseName, extension)
            : QStringLiteral("%1 copy %2%3").arg(baseName).arg(counter).arg(extension);
        const QString candidatePath = QDir(directoryPath).absoluteFilePath(candidateName);
        if (!QFileInfo::exists(candidatePath)) {
            return candidatePath;
        }
    }

    return QString();
}

QString ensureSuffix(const QString &name, const QString &suffix)
{
    QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QString dotSuffix = QStringLiteral(".") + suffix.toLower();
    if (!normalized.toLower().endsWith(dotSuffix)) {
        normalized += dotSuffix;
    }

    return normalized;
}

int sidebarAutoSnapThreshold(int railWidth)
{
    // Keep a small-but-usable content width below which the sidebar snaps to rail.
    return qMax(240, railWidth + 180);
}

QString rgbaColorCss(const QColor &color, qreal alpha)
{
    const qreal clampedAlpha = std::clamp(alpha, 0.0, 1.0);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(clampedAlpha, 0, 'f', 3);
}

QPixmap renderLucidePixmap(const QString &iconName, const QColor &color, int extent)
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

    QPixmap pixmap(extent, extent);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0.0, 0.0, extent, extent));
    return pixmap;
}

QIcon themedLucideIcon(const QString &iconName, const QPalette &palette, int extent)
{
    QIcon icon;
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::ButtonText), extent), QIcon::Normal);
    icon.addPixmap(renderLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::ButtonText), extent), QIcon::Disabled);
    return icon;
}

void prepareSidebarContentPane(QWidget *contentWidget)
{
    if (contentWidget == nullptr) {
        return;
    }
    contentWidget->setVisible(true);
    contentWidget->setMinimumWidth(0);
    contentWidget->setMaximumWidth(QWIDGETSIZE_MAX);
}

int splitterTotalWidth(QSplitter *splitter)
{
    if (splitter == nullptr) {
        return 0;
    }

    int totalWidth = 0;
    const QList<int> sizes = splitter->sizes();
    for (const int size : sizes) {
        totalWidth += size;
    }
    if (totalWidth <= 0) {
        totalWidth = splitter->width();
    }
    return totalWidth;
}
}

void MainWindow::buildProjectBrowser()
{
    if (sidebarPages_ == nullptr || projectModel_ == nullptr) {
        return;
    }

    auto *projectPage = new QWidget(sidebarPages_);
    projectPage->setMinimumWidth(0);
    projectPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *projectLayout = new QVBoxLayout(projectPage);
    projectLayout->setContentsMargins(12, 12, 12, 12);
    projectLayout->setSpacing(8);

    auto *projectDescription = new QLabel(tr("Browse the files in the current project."), projectPage);
    projectDescription->setWordWrap(true);
    projectLayout->addWidget(projectDescription);

    projectTree_ = new QTreeView(projectPage);
    projectTree_->setMinimumWidth(0);
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
    projectTree_->setItemDelegateForColumn(0, new ProjectTreeItemDelegate(projectModel_, projectTree_));
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
    const QString itemPath = index.isValid() ? projectModel_->filePath(index) : QString();
    const QFileInfo itemInfo(itemPath);
    const bool hasItem = index.isValid() && !itemPath.isEmpty() && itemInfo.exists();

    QString creationDirectory = projectRootPath_;
    if (creationDirectory.isEmpty() && hasItem) {
        creationDirectory = itemInfo.isDir() ? itemInfo.absoluteFilePath() : itemInfo.absolutePath();
    } else if (hasItem) {
        creationDirectory = itemInfo.isDir() ? itemInfo.absoluteFilePath() : itemInfo.absolutePath();
    }
    if (creationDirectory.isEmpty() || !QDir(creationDirectory).exists()) {
        creationDirectory = projectRootPath_;
    }

    QMenu menu(projectTree_);

    auto openTabsForPathPrefix = [this](const QString &pathPrefix) {
        QVector<int> indices;
        if (pathPrefix.isEmpty()) {
            return indices;
        }

        const QString canonicalPrefix = QFileInfo(pathPrefix).canonicalFilePath().isEmpty()
            ? QFileInfo(pathPrefix).absoluteFilePath()
            : QFileInfo(pathPrefix).canonicalFilePath();
        const QString normalizedPrefix = canonicalPrefix.endsWith(QLatin1Char('/'))
            ? canonicalPrefix
            : canonicalPrefix + QLatin1Char('/');

        for (int tabIndex = 0; tabIndex < editorTabs_->count(); ++tabIndex) {
            QWidget *tabWidget = editorTabs_->widget(tabIndex);
            const QString documentPath = documentPathForWidget(tabWidget);
            if (documentPath.isEmpty()) {
                continue;
            }

            const QString canonicalDocumentPath = QFileInfo(documentPath).canonicalFilePath().isEmpty()
                ? QFileInfo(documentPath).absoluteFilePath()
                : QFileInfo(documentPath).canonicalFilePath();
            if (canonicalDocumentPath == canonicalPrefix || canonicalDocumentPath.startsWith(normalizedPrefix)) {
                indices.append(tabIndex);
            }
        }

        return indices;
    };

    auto warnOpenTabs = [this, &openTabsForPathPrefix](const QString &path) {
        const QVector<int> openTabs = openTabsForPathPrefix(path);
        if (openTabs.isEmpty()) {
            return false;
        }

        QMessageBox::information(this,
                                 tr("Project Browser"),
                                 tr("Close open tabs for the selected path before renaming or deleting it."));
        return true;
    };

    auto createFileAction = [this, creationDirectory, &menu](const QString &label,
                                                              const QString &prompt,
                                                              const QString &defaultName,
                                                              const QString &suffix) {
        if (creationDirectory.isEmpty()) {
            return;
        }

        menu.addAction(label, this, [this, creationDirectory, prompt, defaultName, suffix]() {
            bool ok = false;
            QString enteredName = QInputDialog::getText(this,
                                                        tr("Create File"),
                                                        prompt,
                                                        QLineEdit::Normal,
                                                        defaultName,
                                                        &ok).trimmed();
            if (!ok) {
                return;
            }
            if (enteredName.isEmpty()) {
                QMessageBox::warning(this, tr("Create File"), tr("File name cannot be empty."));
                return;
            }

            if (!suffix.isEmpty()) {
                enteredName = ensureSuffix(enteredName, suffix);
            }

            const QString filePath = QDir(creationDirectory).absoluteFilePath(enteredName);
            if (QFileInfo::exists(filePath)) {
                QMessageBox::warning(this, tr("Create File"), tr("File already exists: %1").arg(QDir::toNativeSeparators(filePath)));
                return;
            }

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMessageBox::warning(this,
                                     tr("Create File"),
                                     tr("Failed to create file: %1").arg(QDir::toNativeSeparators(filePath)));
                return;
            }
            if (isTherionProjectFilePath(filePath)) {
                const QByteArray initialContents("encoding utf-8\n");
                if (file.write(initialContents) != initialContents.size()) {
                    file.close();
                    QMessageBox::warning(this,
                                         tr("Create File"),
                                         tr("Failed to initialize file: %1").arg(QDir::toNativeSeparators(filePath)));
                    QFile::remove(filePath);
                    return;
                }
            }
            file.close();

            refreshProjectBrowserView(filePath);
            statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(filePath)), 3000);
            appendConsoleLine(tr("Created %1").arg(filePath));
        });
    };

    if (hasItem && itemInfo.isFile()) {
        menu.addAction(tr("Open"), this, [this, index]() {
            handleProjectTreeActivated(index);
        });

        if (itemInfo.suffix().compare(QStringLiteral("th2"), Qt::CaseInsensitive) == 0) {
            menu.addAction(tr("Open in Map Editor"), this, [this, itemPath]() {
                openMapEditorTab(itemPath);
            });
        }

        if (!isTherionProjectFilePath(itemPath)) {
            menu.addAction(tr("Open Externally"), this, [this, itemPath]() {
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(itemPath))) {
                    QMessageBox::warning(this,
                                         tr("Open Externally"),
                                             tr("Failed to open %1 with the system default application.")
                                             .arg(QDir::toNativeSeparators(itemPath)));
                }
            });
        }

        menu.addSeparator();
        menu.addAction(tr("Duplicate"), this, [this, itemPath]() {
            const QString targetPath = duplicateFilePath(itemPath);
            if (targetPath.isEmpty()) {
                QMessageBox::warning(this, tr("Duplicate"), tr("Could not resolve a duplicate file name."));
                return;
            }

            if (!QFile::copy(itemPath, targetPath)) {
                QMessageBox::warning(this,
                                     tr("Duplicate"),
                                     tr("Failed to duplicate file to %1.").arg(QDir::toNativeSeparators(targetPath)));
                return;
            }

            statusBar()->showMessage(tr("Duplicated %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Duplicated %1 -> %2").arg(itemPath, targetPath));
            refreshProjectBrowserView(targetPath);
        });

        menu.addAction(tr("Rename"), this, [this, itemPath, &warnOpenTabs]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const QFileInfo currentInfo(itemPath);
            bool ok = false;
            const QString newName = QInputDialog::getText(this,
                                                          tr("Rename"),
                                                          tr("New name:"),
                                                          QLineEdit::Normal,
                                                          currentInfo.fileName(),
                                                          &ok).trimmed();
            if (!ok) {
                return;
            }
            if (newName.isEmpty()) {
                QMessageBox::warning(this, tr("Rename"), tr("Name cannot be empty."));
                return;
            }
            if (newName == currentInfo.fileName()) {
                return;
            }

            const QString renamedPath = currentInfo.dir().absoluteFilePath(newName);
            if (QFileInfo::exists(renamedPath)) {
                QMessageBox::warning(this, tr("Rename"), tr("Target already exists: %1").arg(QDir::toNativeSeparators(renamedPath)));
                return;
            }

            if (!QFile::rename(itemPath, renamedPath)) {
                QMessageBox::warning(this, tr("Rename"), tr("Failed to rename file."));
                return;
            }

            statusBar()->showMessage(tr("Renamed to %1").arg(QDir::toNativeSeparators(renamedPath)), 3000);
            appendConsoleLine(tr("Renamed %1 -> %2").arg(itemPath, renamedPath));
            refreshProjectBrowserView(renamedPath);
            rebuildStructureSidebar();
            rebuildMapObjectsTree();
        });

        menu.addAction(tr("Delete"), this, [this, itemPath, &warnOpenTabs]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const auto answer = QMessageBox::question(this,
                                                      tr("Delete File"),
                                                      tr("Delete %1?").arg(QDir::toNativeSeparators(itemPath)),
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }

            if (!QFile::remove(itemPath)) {
                QMessageBox::warning(this, tr("Delete File"), tr("Failed to delete file."));
                return;
            }

            statusBar()->showMessage(tr("Deleted %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Deleted %1").arg(itemPath));
            refreshProjectBrowserView(QFileInfo(itemPath).absolutePath());
            rebuildStructureSidebar();
            rebuildMapObjectsTree();
        });
    } else if (hasItem && itemInfo.isDir()) {
        menu.addAction(tr("Rename Folder"), this, [this, itemPath, &warnOpenTabs]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const QFileInfo currentInfo(itemPath);
            bool ok = false;
            const QString newName = QInputDialog::getText(this,
                                                          tr("Rename Folder"),
                                                          tr("New folder name:"),
                                                          QLineEdit::Normal,
                                                          currentInfo.fileName(),
                                                          &ok).trimmed();
            if (!ok) {
                return;
            }
            if (newName.isEmpty()) {
                QMessageBox::warning(this, tr("Rename Folder"), tr("Folder name cannot be empty."));
                return;
            }
            if (newName == currentInfo.fileName()) {
                return;
            }

            const QString renamedPath = currentInfo.dir().absoluteFilePath(newName);
            if (QFileInfo::exists(renamedPath)) {
                QMessageBox::warning(this, tr("Rename Folder"), tr("Target already exists: %1").arg(QDir::toNativeSeparators(renamedPath)));
                return;
            }

            if (!QDir().rename(itemPath, renamedPath)) {
                QMessageBox::warning(this, tr("Rename Folder"), tr("Failed to rename folder."));
                return;
            }

            statusBar()->showMessage(tr("Renamed folder to %1").arg(QDir::toNativeSeparators(renamedPath)), 3000);
            appendConsoleLine(tr("Renamed folder %1 -> %2").arg(itemPath, renamedPath));
            refreshProjectBrowserView(renamedPath);
            rebuildStructureSidebar();
            rebuildMapObjectsTree();
        });

        menu.addAction(tr("Delete Folder"), this, [this, itemPath, &warnOpenTabs]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const auto answer = QMessageBox::question(this,
                                                      tr("Delete Folder"),
                                                      tr("Delete folder %1 and all its contents?")
                                                          .arg(QDir::toNativeSeparators(itemPath)),
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }

            QDir directory(itemPath);
            if (!directory.removeRecursively()) {
                QMessageBox::warning(this, tr("Delete Folder"), tr("Failed to delete folder."));
                return;
            }

            statusBar()->showMessage(tr("Deleted folder %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Deleted folder %1").arg(itemPath));
            refreshProjectBrowserView(QFileInfo(itemPath).absolutePath());
            rebuildStructureSidebar();
            rebuildMapObjectsTree();
        });
    }

    if (!creationDirectory.isEmpty() && QDir(creationDirectory).exists()) {
        if (!menu.isEmpty()) {
            menu.addSeparator();
        }

        menu.addAction(tr("New Folder"), this, [this, creationDirectory]() {
            bool ok = false;
            const QString folderName = QInputDialog::getText(this,
                                                             tr("Create Folder"),
                                                             tr("Folder name:"),
                                                             QLineEdit::Normal,
                                                             tr("New Folder"),
                                                             &ok).trimmed();
            if (!ok) {
                return;
            }
            if (folderName.isEmpty()) {
                QMessageBox::warning(this, tr("Create Folder"), tr("Folder name cannot be empty."));
                return;
            }

            const QString folderPath = QDir(creationDirectory).absoluteFilePath(folderName);
            if (QFileInfo::exists(folderPath)) {
                QMessageBox::warning(this, tr("Create Folder"), tr("Folder already exists: %1").arg(QDir::toNativeSeparators(folderPath)));
                return;
            }

            if (!QDir().mkpath(folderPath)) {
                QMessageBox::warning(this, tr("Create Folder"), tr("Failed to create folder."));
                return;
            }

            statusBar()->showMessage(tr("Created folder %1").arg(QDir::toNativeSeparators(folderPath)), 3000);
            appendConsoleLine(tr("Created folder %1").arg(folderPath));
            refreshProjectBrowserView(folderPath);
        });

        createFileAction(tr("New .th File"), tr("File name:"), tr("new-file.th"), QStringLiteral("th"));
        createFileAction(tr("New .th2 File"), tr("File name:"), tr("new-map.th2"), QStringLiteral("th2"));
        createFileAction(tr("New thconfig"), tr("File name:"), tr("thconfig"), QString());
    }

    if (menu.isEmpty()) {
        return;
    }

    menu.exec(projectTree_->viewport()->mapToGlobal(position));
}

void MainWindow::buildStructureSidebar()
{
    if (mainContentLayout_ == nullptr || mainContentSplitter_ == nullptr) {
        return;
    }

    sidebarCollapseButton_ = nullptr;

    connect(mainContentSplitter_, &QSplitter::splitterMoved, this, [this](int position, int) {
        if (updatingSidebarSplitter_) {
            return;
        }
        if (sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || !sidebarContainer_->isVisible()) {
            return;
        }

        const int width = qMax(0, position);
        const int snapThreshold = sidebarAutoSnapThreshold(sidebarRailWidth_);
        if (sidebarCollapsed_) {
            if (width > snapThreshold) {
                sidebarCollapsed_ = false;
                sidebarExpandedWidth_ = qMax(width, sidebarRailWidth_ + 240);
                prepareSidebarContentPane(sidebarContentContainer_);
                updateSidebarCollapseButton();
            }
            return;
        }

        if (width <= snapThreshold) {
            setSidebarCollapsed(true);
        } else {
            sidebarExpandedWidth_ = width;
        }
    });

    auto *activityBar = new QFrame;
    sidebarContainer_ = activityBar;
    activityBar->setObjectName(QStringLiteral("SidebarActivityRail"));
    activityBar->setFrameShape(QFrame::NoFrame);
    activityBar->setFixedWidth(sidebarRailWidth_);
    activityBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto *activityLayout = new QVBoxLayout(activityBar);
    activityLayout->setContentsMargins(6, 10, 6, 10);
    activityLayout->setSpacing(7);

    const QString filesIconName = QStringLiteral("folder-open");
    const QString structureIconName = QStringLiteral("network");
    const QString mapIconName = QStringLiteral("map");
    const QString consoleIconName = QStringLiteral("cog");
    const QSize activityIconSize(20, 20);
    const QSize activityButtonSize(34, 34);
    const auto configureActivityButton = [&](QToolButton *button, const QString &iconName, const QString &toolTip) {
        if (button == nullptr) {
            return;
        }
        button->setToolTip(toolTip);
        button->setIconSize(activityIconSize);
        button->setFixedSize(activityButtonSize);
        button->setAutoRaise(true);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
    };
    const auto isSidebarEffectivelyCollapsed = [this]() -> bool {
        if (sidebarCollapsed_) {
            return true;
        }
        if (sidebarContentContainer_ != nullptr && sidebarContentContainer_->isVisible()) {
            return sidebarContentContainer_->width() <= sidebarAutoSnapThreshold(sidebarRailWidth_);
        }
        return false;
    };
    const auto handleActivityButtonClick = [this, isSidebarEffectivelyCollapsed](SidebarPane pane) {
        if (isSidebarEffectivelyCollapsed()) {
            if (sidebarContainer_ != nullptr && !sidebarContainer_->isVisible()) {
                sidebarContainer_->setVisible(true);
            }

            if (sidebarCollapsed_) {
                setSidebarCollapsed(false);
            } else {
                if (sidebarExpandedWidth_ <= (sidebarRailWidth_ + 8)) {
                    sidebarExpandedWidth_ = qMax(320, sidebarRailWidth_ + 240);
                }
                sidebarCollapsed_ = false;
                prepareSidebarContentPane(sidebarContentContainer_);
                restoreSidebarWidth();
                updateSidebarCollapseButton();
            }
            setSidebarPane(pane);
            return;
        }
        if (activeSidebarPane_ == pane) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(pane);
    };

    sidebarFilesButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarFilesButton_, filesIconName, tr("Files"));
    connect(sidebarFilesButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::FileBrowser);
    });
    activityLayout->addWidget(sidebarFilesButton_);

    sidebarStructureButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarStructureButton_, structureIconName, tr("Structure"));
    connect(sidebarStructureButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::StructureBrowser);
    });
    activityLayout->addWidget(sidebarStructureButton_);

    sidebarMapButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarMapButton_, mapIconName, tr("Map"));
    connect(sidebarMapButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::MapEditor);
    });
    activityLayout->addWidget(sidebarMapButton_);

    sidebarConsoleButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarConsoleButton_, consoleIconName, tr("Compiler"));
    connect(sidebarConsoleButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Console);
    });
    activityLayout->addWidget(sidebarConsoleButton_);

    const auto applyActivityRailTheme = [activityBar,
                                         filesButton = sidebarFilesButton_,
                                         structureButton = sidebarStructureButton_,
                                         mapButton = sidebarMapButton_,
                                         compilerButton = sidebarConsoleButton_,
                                         filesIconName,
                                         structureIconName,
                                         mapIconName,
                                         consoleIconName,
                                         activityIconSize]() {
        if (activityBar == nullptr) {
            return;
        }

        const QPalette palette = activityBar->palette();
        const QColor railBase = palette.color(QPalette::Window);
        const QColor railBorder = palette.color(QPalette::Mid);
        const QColor railHover = palette.color(QPalette::Highlight);
        const QColor railChecked = palette.color(QPalette::Highlight);
        activityBar->setStyleSheet(QStringLiteral(
                                       "#SidebarActivityRail {"
                                       "background-color: %1;"
                                       "border-right: 1px solid %2;"
                                       "}"
                                       "#SidebarActivityRail QToolButton {"
                                       "border: none;"
                                       "background: transparent;"
                                       "border-radius: 9px;"
                                       "padding: 0px;"
                                       "}"
                                       "#SidebarActivityRail QToolButton:hover {"
                                       "background-color: %3;"
                                       "}"
                                       "#SidebarActivityRail QToolButton:checked {"
                                       "background-color: %4;"
                                       "}")
                                       .arg(rgbaColorCss(railBase, 0.78))
                                       .arg(rgbaColorCss(railBorder, 0.55))
                                       .arg(rgbaColorCss(railHover, 0.24))
                                       .arg(rgbaColorCss(railChecked, 0.34)));

        const int extent = activityIconSize.width();
        if (filesButton != nullptr) {
            filesButton->setIcon(themedLucideIcon(filesIconName, palette, extent));
        }
        if (structureButton != nullptr) {
            structureButton->setIcon(themedLucideIcon(structureIconName, palette, extent));
        }
        if (mapButton != nullptr) {
            mapButton->setIcon(themedLucideIcon(mapIconName, palette, extent));
        }
        if (compilerButton != nullptr) {
            compilerButton->setIcon(themedLucideIcon(consoleIconName, palette, extent));
        }
    };
    applyActivityRailTheme();
    if (qApp != nullptr) {
        auto *paletteEventFilter = new PaletteEventFilter(applyActivityRailTheme, activityBar);
        qApp->installEventFilter(paletteEventFilter);
    }

    activityLayout->addStretch(1);

    // Keep the activity rail width driven by icon/button metrics to avoid extra blank gutter.
    sidebarRailWidth_ = qMax(40, activityBar->sizeHint().width());
    activityBar->setFixedWidth(sidebarRailWidth_);
    sidebarContainer_->setMinimumWidth(sidebarRailWidth_);
    sidebarContainer_->setMaximumWidth(sidebarRailWidth_);
    mainContentLayout_->addWidget(sidebarContainer_, 0);

    sidebarContentContainer_ = new QWidget(mainContentSplitter_);
    sidebarContentContainer_->setMinimumWidth(0);
    sidebarContentContainer_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *sidebarContentLayout = new QVBoxLayout(sidebarContentContainer_);
    sidebarContentLayout->setContentsMargins(0, 0, 0, 0);
    sidebarContentLayout->setSpacing(0);

    sidebarPages_ = new QStackedWidget(sidebarContentContainer_);
    sidebarPages_->setMinimumWidth(0);
    sidebarPages_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    sidebarContentLayout->addWidget(sidebarPages_, 1);

    mainContentSplitter_->addWidget(sidebarContentContainer_);

    buildProjectBrowser();

    auto *structurePage = new QWidget(sidebarPages_);
    auto *structureLayout = new QVBoxLayout(structurePage);
    structureLayout->setContentsMargins(12, 12, 12, 12);
    structureLayout->setSpacing(8);

    structureTree_ = new QTreeView(structurePage);
    structureTree_->setMinimumWidth(0);
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

    structureLayout->addWidget(structureTree_, 1);
    sidebarPages_->addWidget(structurePage);

    auto *mapObjectsPage = new QWidget(sidebarPages_);
    auto *mapObjectsLayout = new QVBoxLayout(mapObjectsPage);
    mapObjectsLayout->setContentsMargins(12, 12, 12, 12);
    mapObjectsLayout->setSpacing(8);

    auto *mapObjectsDescription = new QLabel(tr("Objects in the active TH2 file are grouped by scrap."), mapObjectsPage);
    mapObjectsDescription->setWordWrap(true);
    mapObjectsLayout->addWidget(mapObjectsDescription);

    mapObjectsTree_ = new QTreeView(mapObjectsPage);
    mapObjectsTree_->setMinimumWidth(0);
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
    if (sidebarContentContainer_ == nullptr || !sidebarContentContainer_->isVisible()) {
        return;
    }

    const int width = sidebarContentContainer_->width();
    if (width <= sidebarAutoSnapThreshold(sidebarRailWidth_)) {
        return;
    }

    sidebarExpandedWidth_ = qMax(sidebarRailWidth_ + 120, width);
}

void MainWindow::restoreSidebarWidth()
{
    if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || sidebarExpandedWidth_ <= 0) {
        return;
    }

    const auto applyRestore = [this]() {
        if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || !sidebarContainer_->isVisible()) {
            return;
        }
        sidebarContentContainer_->setVisible(true);

        const int totalWidth = splitterTotalWidth(mainContentSplitter_);
        if (totalWidth <= 0) {
            return;
        }

        const int targetContentWidth = qBound(0, sidebarExpandedWidth_, qMax(0, totalWidth - 240));
        const int editorWidth = qMax(240, totalWidth - targetContentWidth);
        updatingSidebarSplitter_ = true;
        mainContentSplitter_->setSizes({targetContentWidth, editorWidth});
        updatingSidebarSplitter_ = false;
    };

    if (mainContentSplitter_->width() <= 0) {
        QTimer::singleShot(0, this, applyRestore);
        return;
    }
    applyRestore();
}

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr) {
        return;
    }

    if (collapsed == sidebarCollapsed_ && (!collapsed || sidebarContentContainer_ == nullptr || sidebarContentContainer_->width() == 0)) {
        updateSidebarCollapseButton();
        return;
    }

    sidebarCollapsed_ = collapsed;
    QWidget *contentWidget = sidebarContentContainer_;
    if (collapsed) {
        rememberSidebarWidth();
        sidebarContainer_->setMinimumWidth(sidebarRailWidth_);
        prepareSidebarContentPane(contentWidget);

        const auto applyCollapse = [this]() {
            if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || !sidebarContainer_->isVisible()) {
                return;
            }

            const int totalWidth = splitterTotalWidth(mainContentSplitter_);
            if (totalWidth <= 0) {
                return;
            }
            updatingSidebarSplitter_ = true;
            mainContentSplitter_->setSizes({0, qMax(240, totalWidth)});
            updatingSidebarSplitter_ = false;
        };

        if (mainContentSplitter_->width() <= 0) {
            QTimer::singleShot(0, this, applyCollapse);
        } else {
            applyCollapse();
        }
    } else {
        sidebarContainer_->setMinimumWidth(sidebarRailWidth_);
        prepareSidebarContentPane(contentWidget);
        restoreSidebarWidth();
    }

    updateSidebarCollapseButton();
}

void MainWindow::updateSidebarCollapseButton()
{
    if (sidebarCollapseButton_ == nullptr || sidebarContainer_ == nullptr) {
        return;
    }

    const Qt::ArrowType arrowType = sidebarCollapsed_ ? Qt::RightArrow : Qt::LeftArrow;
    sidebarCollapseButton_->setArrowType(arrowType);
    sidebarCollapseButton_->setToolTip(sidebarCollapsed_ ? tr("Expand sidebar") : tr("Collapse sidebar"));
}
