#include "MainWindow.h"

#include "ApplicationStylePolicy.h"
#include "MainWindowDocumentOpenController.h"
#include "MainWindowDocumentHelpers.h"
#include "LucideIconFactory.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "ui/ApplicationControlMetrics.h"
#include "ui/ApplicationSegmentedControlStyle.h"
#include "../core/TherionFileTypes.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardItemModel>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace
{
enum ProjectSearchResultRole
{
    SearchFilePathRole = Qt::UserRole + 70,
    SearchLineNumberRole,
    SearchColumnNumberRole,
    SearchIsMatchRole,
};

enum StructurePanelPage
{
    StructurePanelFilesPage = 0,
    StructurePanelSurveyPage = 1,
    StructurePanelMapPage = 2
};

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

        if (!QFileInfo(filePath).isFile() || !TherionStudio::isTherionProjectFilePath(filePath)) {
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

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

QString projectRelativeDisplayPath(const QString &projectRootPath, const QString &filePath)
{
    const QString canonicalRoot = canonicalOrAbsolutePath(projectRootPath);
    const QString canonicalFile = canonicalOrAbsolutePath(filePath);
    if (!canonicalRoot.isEmpty()) {
        const QString relativePath = QDir(canonicalRoot).relativeFilePath(canonicalFile);
        if (!relativePath.startsWith(QStringLiteral(".."))) {
            return relativePath;
        }
    }
    return QDir::toNativeSeparators(filePath);
}

int sidebarAutoSnapThreshold(int railWidth)
{
    // Keep a small-but-usable content width below which the sidebar snaps to rail.
    return qMax(240, railWidth + 180);
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

QString defaultFileBaseName(const QString &defaultName, const QString &suffix)
{
    if (suffix.isEmpty()) {
        return defaultName.trimmed();
    }

    const QString extension = QStringLiteral(".") + suffix;
    if (defaultName.endsWith(extension, Qt::CaseInsensitive)) {
        return defaultName.left(defaultName.size() - extension.size()).trimmed();
    }

    return defaultName.trimmed();
}

QString requestProjectFileBaseName(QWidget *parent,
                                   const QString &title,
                                   const QString &prompt,
                                   const QString &defaultName,
                                   const QString &suffix,
                                   bool *accepted)
{
    if (accepted != nullptr) {
        *accepted = false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel(prompt, &dialog);
    layout->addWidget(label);

    auto *row = new QWidget(&dialog);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    auto *nameEdit = new QLineEdit(row);
    nameEdit->setText(defaultFileBaseName(defaultName, suffix));
    nameEdit->selectAll();
    rowLayout->addWidget(nameEdit, 1);

    if (!suffix.isEmpty()) {
        auto *suffixLabel = new QLabel(QStringLiteral(".") + suffix, row);
        suffixLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        rowLayout->addWidget(suffixLabel, 0);
    }

    layout->addWidget(row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    nameEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }

    if (accepted != nullptr) {
        *accepted = true;
    }
    return nameEdit->text().trimmed();
}
}

void MainWindow::buildProjectBrowser()
{
    if (sidebarPages_ == nullptr || projectModel_ == nullptr) {
        return;
    }

    auto *projectPage = new QWidget;
    projectFilesPage_ = projectPage;
    projectPage->setMinimumWidth(0);
    projectPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *projectLayout = new QVBoxLayout(projectPage);
    projectLayout->setContentsMargins(0, 0, 0, 0);
    projectLayout->setSpacing(8);

    projectFilesEmptyState_ = new QWidget(projectPage);
    auto *emptyLayout = new QVBoxLayout(projectFilesEmptyState_);
    emptyLayout->setContentsMargins(0, 24, 0, 0);
    emptyLayout->setSpacing(8);

    auto *emptyTitle = new QLabel(tr("No project open."), projectFilesEmptyState_);
    QFont emptyTitleFont = emptyTitle->font();
    emptyTitleFont.setBold(true);
    emptyTitle->setFont(emptyTitleFont);
    emptyTitle->setWordWrap(true);
    emptyLayout->addWidget(emptyTitle);

    auto *emptyMessage = new QLabel(tr("Open a project to browse its files."), projectFilesEmptyState_);
    emptyMessage->setWordWrap(true);
    emptyLayout->addWidget(emptyMessage);

    projectFilesOpenProjectButton_ = new QPushButton(tr("Open Project..."), projectFilesEmptyState_);
    projectFilesOpenProjectButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(projectFilesOpenProjectButton_, &QPushButton::clicked, this, &MainWindow::openProject);
    emptyLayout->addWidget(projectFilesOpenProjectButton_);
    emptyLayout->addStretch(1);
    projectLayout->addWidget(projectFilesEmptyState_, 1);

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
    projectTree_->header()->hide();
    projectTree_->setItemDelegateForColumn(0, new ProjectTreeItemDelegate(projectModel_, projectTree_));
    connect(projectTree_, &QTreeView::activated, this, &MainWindow::handleProjectTreeActivated);
    connect(projectTree_, &QTreeView::customContextMenuRequested, this, &MainWindow::handleProjectTreeContextMenuRequested);

    projectLayout->addWidget(projectTree_, 1);

    auto *fileBrowserPlaceholder = new QWidget(sidebarPages_);
    fileBrowserPlaceholder->setObjectName(QStringLiteral("fileBrowserSidebarPlaceholder"));
    sidebarPages_->addWidget(fileBrowserPlaceholder);

    resetProjectBrowser();
}

void MainWindow::refreshProjectBrowserView(const QString &focusPath, bool forceReload)
{
    if (projectModel_ == nullptr || projectTree_ == nullptr) {
        return;
    }

    const bool hasOpenProject = !projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists();
    if (projectFilesEmptyState_ != nullptr) {
        projectFilesEmptyState_->setVisible(!hasOpenProject);
    }
    projectTree_->setVisible(hasOpenProject);

    if (!hasOpenProject) {
        projectTree_->setRootIndex(QModelIndex());
        return;
    }

    const QString rootPath = projectRootPath_;
    if (forceReload) {
        auto *previousModel = projectModel_;
        QAbstractItemDelegate *previousNameDelegate = projectTree_->itemDelegateForColumn(0);
        projectModel_ = new QFileSystemModel(this);
        projectTree_->setModel(projectModel_);
        projectTree_->hideColumn(1);
        projectTree_->hideColumn(2);
        projectTree_->hideColumn(3);
        projectTree_->setItemDelegateForColumn(0, new ProjectTreeItemDelegate(projectModel_, projectTree_));
        if (previousNameDelegate != nullptr && previousNameDelegate->parent() == projectTree_) {
            previousNameDelegate->deleteLater();
        }
        previousModel->deleteLater();
    }
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

void MainWindow::buildSearchSidebar()
{
    if (sidebarPages_ == nullptr || searchResultsModel_ == nullptr) {
        return;
    }

    auto *searchPage = new QWidget(sidebarPages_);
    searchPage->setMinimumWidth(0);
    searchPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *searchLayout = new QVBoxLayout(searchPage);
    searchLayout->setContentsMargins(12, 12, 12, 12);
    searchLayout->setSpacing(8);

    auto *searchHeader = new QLabel(tr("Search project files."), searchPage);
    searchHeader->setWordWrap(true);
    searchLayout->addWidget(searchHeader);

    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(6);

    projectSearchEdit_ = new QLineEdit(searchPage);
    projectSearchEdit_->setPlaceholderText(tr("Search"));
    projectSearchEdit_->setClearButtonEnabled(true);
    searchRow->addWidget(projectSearchEdit_, 1);

    projectSearchButton_ = new QPushButton(tr("Search"), searchPage);
    projectSearchButton_->setDefault(false);
    searchRow->addWidget(projectSearchButton_);
    searchLayout->addLayout(searchRow);

    auto *searchOptionsRow = new QHBoxLayout;
    searchOptionsRow->setContentsMargins(0, 0, 0, 0);
    searchOptionsRow->setSpacing(12);
    projectSearchWholeWordCheck_ = new QCheckBox(tr("Whole word"), searchPage);
    projectSearchMatchCaseCheck_ = new QCheckBox(tr("Case sensitive"), searchPage);
    searchOptionsRow->addWidget(projectSearchWholeWordCheck_);
    searchOptionsRow->addWidget(projectSearchMatchCaseCheck_);
    searchOptionsRow->addStretch(1);
    searchLayout->addLayout(searchOptionsRow);

    projectSearchStatusLabel_ = new QLabel(searchPage);
    projectSearchStatusLabel_->setWordWrap(true);
    projectSearchStatusLabel_->setVisible(false);
    searchLayout->addWidget(projectSearchStatusLabel_);

    searchResultsModel_->clear();
    searchResultsModel_->setHorizontalHeaderLabels({tr("Results")});

    searchResultsTree_ = new QTreeView(searchPage);
    searchResultsTree_->setMinimumWidth(0);
    searchResultsTree_->setModel(searchResultsModel_);
    searchResultsTree_->setRootIsDecorated(true);
    searchResultsTree_->setAnimated(true);
    searchResultsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    searchResultsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    searchResultsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    searchResultsTree_->setAlternatingRowColors(true);
    searchResultsTree_->header()->setStretchLastSection(true);
    connect(searchResultsTree_, &QTreeView::activated, this, &MainWindow::openProjectSearchResult);
    searchLayout->addWidget(searchResultsTree_, 1);

    connect(projectSearchEdit_, &QLineEdit::returnPressed, this, &MainWindow::requestProjectSearch);
    connect(projectSearchButton_, &QPushButton::clicked, this, &MainWindow::requestProjectSearch);

    sidebarPages_->addWidget(searchPage);
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
                                 tr("Close open tabs for the selected path before renaming it or deleting a folder."));
        return true;
    };

    auto refreshAfterProjectFileMutation = [this](const QString &focusPath) {
        refreshProjectBrowserView(focusPath, true);
        rebuildStructureSidebar();
        rebuildMapObjectsTree();
    };

    auto createFileAction = [this, creationDirectory, &menu, &refreshAfterProjectFileMutation](const QString &label,
                                                                                               const QString &prompt,
                                                                                               const QString &defaultName,
                                                                                               const QString &suffix) {
        if (creationDirectory.isEmpty()) {
            return;
        }

        menu.addAction(label, this, [this, creationDirectory, prompt, defaultName, suffix, &refreshAfterProjectFileMutation]() {
            bool ok = false;
            QString enteredName = requestProjectFileBaseName(this,
                                                             tr("Create File"),
                                                             prompt,
                                                             defaultName,
                                                             suffix,
                                                             &ok);
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
            const QByteArray initialContents = TherionStudio::initialTherionProjectFileContents(filePath);
            if (!initialContents.isEmpty()) {
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

            refreshAfterProjectFileMutation(filePath);
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

        if (!itemInfo.isFile() || !TherionStudio::isTherionProjectFilePath(itemPath)) {
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
        menu.addAction(tr("Duplicate"), this, [this, itemPath, &refreshAfterProjectFileMutation]() {
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
            refreshAfterProjectFileMutation(targetPath);
        });

        menu.addAction(tr("Rename"), this, [this, itemPath, &warnOpenTabs, &refreshAfterProjectFileMutation]() {
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
            refreshAfterProjectFileMutation(renamedPath);
        });

        menu.addAction(tr("Delete"), this, [this, itemPath, &refreshAfterProjectFileMutation]() {
            const auto answer = QMessageBox::question(this,
                                                      tr("Delete File"),
                                                      tr("Delete %1?\n\nIf the file is open, it will be closed first.")
                                                          .arg(QDir::toNativeSeparators(itemPath)),
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }

            if (!closeOpenDocumentForFilePath(itemPath)) {
                return;
            }

            if (!QFile::remove(itemPath)) {
                QMessageBox::warning(this, tr("Delete File"), tr("Failed to delete file."));
                return;
            }

            statusBar()->showMessage(tr("Deleted %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Deleted %1").arg(itemPath));
            refreshAfterProjectFileMutation(QFileInfo(itemPath).absolutePath());
        });
    } else if (hasItem && itemInfo.isDir()) {
        menu.addAction(tr("Rename Folder"), this, [this, itemPath, &warnOpenTabs, &refreshAfterProjectFileMutation]() {
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
            refreshAfterProjectFileMutation(renamedPath);
        });

        menu.addAction(tr("Delete Folder"), this, [this, itemPath, &warnOpenTabs, &refreshAfterProjectFileMutation]() {
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
            refreshAfterProjectFileMutation(QFileInfo(itemPath).absolutePath());
        });
    }

    if (!creationDirectory.isEmpty() && QDir(creationDirectory).exists()) {
        if (!menu.isEmpty()) {
            menu.addSeparator();
        }

        menu.addAction(tr("New Folder"), this, [this, creationDirectory, &refreshAfterProjectFileMutation]() {
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
            refreshAfterProjectFileMutation(folderPath);
        });

        createFileAction(tr("New .th File"), tr("File name:"), tr("new-file.th"), QStringLiteral("th"));
        createFileAction(tr("New .th2 File"), tr("File name:"), tr("new-map.th2"), QStringLiteral("th2"));
        createFileAction(tr("New .thconfig File"), tr("File name:"), tr("new-config.thconfig"), QStringLiteral("thconfig"));
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
        refreshWorkspaceModeSwitcherGeometry();
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
                refreshViewMenuActions();
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

    const QString structureIconName = QStringLiteral("network");
    const QString searchIconName = QStringLiteral("search");
    const QString validationIconName = QStringLiteral("spell-check");
    const QString consoleIconName = QStringLiteral("cog");
    const QSize activityIconSize = TherionStudio::UiMetrics::squareSize(TherionStudio::UiMetrics::sidebarActivityIconSize());
    const QSize activityButtonSize = TherionStudio::UiMetrics::squareSize(TherionStudio::UiMetrics::sidebarActivityButtonSize());
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
    const auto handleActivityButtonClick = [this](SidebarPane pane) {
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
                refreshViewMenuActions();
            }
            setSidebarPane(pane);
            return;
        }
        const bool projectNavigationPaneActive = pane == SidebarPane::StructureBrowser
            && (activeSidebarPane_ == SidebarPane::FileBrowser
                || activeSidebarPane_ == SidebarPane::StructureBrowser);
        if (projectNavigationPaneActive) {
            setSidebarCollapsed(true);
            return;
        }
        if (activeSidebarPane_ == pane) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(pane);
    };

    sidebarStructureButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarStructureButton_, structureIconName, tr("Structure"));
    connect(sidebarStructureButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::StructureBrowser);
    });
    activityLayout->addWidget(sidebarStructureButton_);

    sidebarSearchButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarSearchButton_, searchIconName, tr("Search"));
    connect(sidebarSearchButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Search);
    });
    activityLayout->addWidget(sidebarSearchButton_);

    sidebarValidationButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarValidationButton_, validationIconName, tr("Validation"));
    connect(sidebarValidationButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Validation);
    });
    activityLayout->addWidget(sidebarValidationButton_);

    sidebarConsoleButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarConsoleButton_, consoleIconName, tr("Compiler"));
    connect(sidebarConsoleButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Console);
    });
    activityLayout->addWidget(sidebarConsoleButton_);

    auto *compileActionSeparator = new QFrame(activityBar);
    compileActionSeparator->setObjectName(QStringLiteral("SidebarActivitySeparator"));
    compileActionSeparator->setFrameShape(QFrame::NoFrame);
    compileActionSeparator->setFixedSize(22, 1);
    activityLayout->addWidget(compileActionSeparator, 0, Qt::AlignHCenter);

    sidebarCompileButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarCompileButton_, QStringLiteral("play"), tr("Compile Project Config"));
    sidebarCompileButton_->setCheckable(false);
    connect(sidebarCompileButton_, &QToolButton::clicked, this, [this]() {
        runTherionProjectConfig();
    });
    activityLayout->addWidget(sidebarCompileButton_);

    const auto applyActivityRailTheme = [activityBar,
                                         structureButton = sidebarStructureButton_,
                                         searchButton = sidebarSearchButton_,
                                         validationButton = sidebarValidationButton_,
                                         compilerButton = sidebarConsoleButton_,
                                         compileButton = sidebarCompileButton_,
                                         structureIconName,
                                         searchIconName,
                                         validationIconName,
                                         consoleIconName,
                                         activityIconSize]() {
        if (activityBar == nullptr) {
            return;
        }

        const QPalette palette = QApplication::palette(activityBar);
        activityBar->setPalette(palette);
        activityBar->setAutoFillBackground(true);
        activityBar->setStyleSheet(TherionStudio::sidebarActivityRailStyleSheet(palette));

        const int extent = activityIconSize.width();
        const qreal devicePixelRatio = activityBar->devicePixelRatioF();
        if (structureButton != nullptr) {
            structureButton->setIcon(TherionStudio::themedLucideIcon(structureIconName, palette, extent, devicePixelRatio));
        }
        if (searchButton != nullptr) {
            searchButton->setIcon(TherionStudio::themedLucideIcon(searchIconName, palette, extent, devicePixelRatio));
        }
        if (validationButton != nullptr) {
            validationButton->setIcon(TherionStudio::themedLucideIcon(validationIconName, palette, extent, devicePixelRatio));
        }
        if (compilerButton != nullptr) {
            compilerButton->setIcon(TherionStudio::themedLucideIcon(consoleIconName, palette, extent, devicePixelRatio));
        }
        if (compileButton != nullptr) {
            compileButton->setIcon(TherionStudio::themedLucideIcon(QStringLiteral("play"), palette, extent, devicePixelRatio));
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
    sidebarContentContainer_->setObjectName(QStringLiteral("mainSidebarSplitterPane"));
    sidebarContentContainer_->setMinimumWidth(0);
    sidebarContentContainer_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    sidebarContentContainer_->setAttribute(Qt::WA_StyledBackground, true);
    sidebarContentContainer_->setStyleSheet(TherionStudio::sidebarContentPaneStyleSheet());
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

    auto *structureViewSelector = new QWidget(structurePage);
    structureViewSelector->setObjectName(QStringLiteral("structureViewSegmentedControl"));
    auto *structureViewSelectorLayout = new QHBoxLayout(structureViewSelector);
    structureViewSelectorLayout->setContentsMargins(0, 0, 0, 0);
    structureViewSelectorLayout->setSpacing(0);
    structureViewModeButtons_ = new QButtonGroup(structureViewSelector);
    structureViewModeButtons_->setExclusive(true);

    auto *filesViewButton = new QPushButton(tr("Files"), structureViewSelector);
    auto *surveyViewButton = new QPushButton(tr("Survey"), structureViewSelector);
    auto *mapViewButton = new QPushButton(QStringLiteral("Map"), structureViewSelector);
    const QList<QPushButton *> structureViewButtons = {filesViewButton, surveyViewButton, mapViewButton};
    for (QPushButton *button : structureViewButtons) {
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("structureSegmentButton"));
        button->setMinimumHeight(TherionStudio::UiMetrics::segmentedControlMinimumHeight());
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        structureViewSelectorLayout->addWidget(button);
    }
    filesViewButton->setProperty("firstSegment", true);
    mapViewButton->setProperty("lastSegment", true);
    structureViewSelector->setStyleSheet(
        TherionStudio::segmentedControlButtonStyleSheet(QStringLiteral("QPushButton#structureSegmentButton")));
    structureViewModeButtons_->addButton(filesViewButton, StructurePanelFilesPage);
    structureViewModeButtons_->addButton(surveyViewButton, StructurePanelSurveyPage);
    structureViewModeButtons_->addButton(mapViewButton, StructurePanelMapPage);
    structureLayout->addWidget(structureViewSelector);

    structureViewStack_ = new QStackedWidget(structurePage);
    structureViewStack_->setObjectName(QStringLiteral("structureViewStack"));
    structureViewStack_->setMinimumWidth(0);
    structureViewStack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    if (projectFilesPage_ == nullptr) {
        projectFilesPage_ = new QWidget(structureViewStack_);
    }
    auto *surveyStructurePage = new QWidget(structureViewStack_);
    structureSurveyLayout_ = new QVBoxLayout(surveyStructurePage);
    structureSurveyLayout_->setContentsMargins(0, 0, 0, 0);
    structureSurveyLayout_->setSpacing(0);
    auto *mapStructurePage = new QWidget(structureViewStack_);
    structureMapLayout_ = new QVBoxLayout(mapStructurePage);
    structureMapLayout_->setContentsMargins(0, 0, 0, 0);
    structureMapLayout_->setSpacing(0);
    structureViewStack_->addWidget(projectFilesPage_);
    structureViewStack_->addWidget(surveyStructurePage);
    structureViewStack_->addWidget(mapStructurePage);
    connect(structureViewModeButtons_, &QButtonGroup::idClicked, this, [this](int pageIndex) {
        setStructurePanelPage(pageIndex);
    });

    structureTree_ = new QTreeView(surveyStructurePage);
    structureTree_->setMinimumWidth(0);
    structureTree_->setModel(structureModel_);
    structureTree_->setRootIsDecorated(true);
    structureTree_->setAnimated(true);
    structureTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    structureTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    structureTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    structureTree_->setAlternatingRowColors(true);
    structureTree_->header()->hide();
    connect(structureTree_, &QTreeView::activated, this, [this](const QModelIndex &index) {
        handleStructureItemActivated(index, structureTree_);
    });

    structureSurveyLayout_->addWidget(structureTree_, 1);
    structureLayout->addWidget(structureViewStack_, 1);
    sidebarPages_->addWidget(structurePage);
    structureViewStack_->setCurrentIndex(StructurePanelSurveyPage);
    surveyViewButton->setChecked(true);

    buildSearchSidebar();
    buildValidationSidebar();

    consoleSidebarPage_ = new QWidget(sidebarPages_);
    consoleSidebarPageLayout_ = new QVBoxLayout(consoleSidebarPage_);
    consoleSidebarPageLayout_->setContentsMargins(12, 12, 12, 12);
    consoleSidebarPageLayout_->setSpacing(8);
    sidebarPages_->addWidget(consoleSidebarPage_);

    updateSidebarCollapseButton();
    setSidebarPane(activeSidebarPane_);
}

void MainWindow::showSidebarPane(SidebarPane pane)
{
    if (sidebarContainer_ == nullptr) {
        setSidebarPane(pane);
        return;
    }

    sidebarContainer_->setVisible(true);
    if (sidebarContentContainer_ != nullptr) {
        sidebarContentContainer_->setVisible(true);
    }

    if (sidebarCollapsed_) {
        setSidebarCollapsed(false);
    } else {
        restoreSidebarWidth();
    }

    setSidebarPane(pane);
}

void MainWindow::setStructurePanelPage(int pageIndex)
{
    if (structureViewStack_ == nullptr) {
        return;
    }

    const int boundedPageIndex = qBound(static_cast<int>(StructurePanelFilesPage),
                                        pageIndex,
                                        static_cast<int>(StructurePanelMapPage));
    structureViewStack_->setCurrentIndex(boundedPageIndex);
    if (QAbstractButton *button = structureViewModeButtons_ != nullptr
            ? structureViewModeButtons_->button(boundedPageIndex)
            : nullptr) {
        button->setChecked(true);
    }

    if (boundedPageIndex == StructurePanelFilesPage) {
        activeSidebarPane_ = SidebarPane::FileBrowser;
        if (sidebarStructureButton_ != nullptr) {
            sidebarStructureButton_->setChecked(true);
        }
        return;
    }

    activeSidebarPane_ = SidebarPane::StructureBrowser;
    if (sidebarStructureButton_ != nullptr) {
        sidebarStructureButton_->setChecked(true);
    }

    const StructureViewMode nextMode = boundedPageIndex == StructurePanelMapPage
        ? StructureViewMode::Map
        : StructureViewMode::Survey;
    if (structureTree_ != nullptr) {
        if (nextMode == StructureViewMode::Map && structureMapLayout_ != nullptr) {
            structureMapLayout_->addWidget(structureTree_, 1);
        } else if (nextMode == StructureViewMode::Survey && structureSurveyLayout_ != nullptr) {
            structureSurveyLayout_->addWidget(structureTree_, 1);
        }
    }
    if (structureViewMode_ == nextMode) {
        return;
    }

    storeCurrentStructureExpansionState();
    structureViewMode_ = nextMode;
    hasAppliedStructureSidebarIndex_ = false;
    lastAppliedStructureSidebarSignature_.clear();
    if (!lastStructureSidebarProjectIndex_.projectRootPath.isEmpty()) {
        applyStructureSidebarIndex(lastStructureSidebarProjectIndex_);
    }
}

void MainWindow::setSidebarPane(SidebarPane pane)
{
    if (sidebarPages_ == nullptr) {
        return;
    }

    activeSidebarPane_ = pane;
    const int sidebarPageIndex = pane == SidebarPane::FileBrowser
        ? static_cast<int>(SidebarPane::StructureBrowser)
        : static_cast<int>(pane);
    sidebarPages_->setCurrentIndex(sidebarPageIndex);
    if (structureViewStack_ != nullptr) {
        if (pane == SidebarPane::FileBrowser) {
            setStructurePanelPage(StructurePanelFilesPage);
        }
    }
    if (sidebarStructureButton_ != nullptr) {
        sidebarStructureButton_->setChecked(pane == SidebarPane::FileBrowser || pane == SidebarPane::StructureBrowser);
    }
    if (sidebarSearchButton_ != nullptr) {
        sidebarSearchButton_->setChecked(pane == SidebarPane::Search);
    }
    if (sidebarValidationButton_ != nullptr) {
        sidebarValidationButton_->setChecked(pane == SidebarPane::Validation);
    }
    if (sidebarConsoleButton_ != nullptr) {
        sidebarConsoleButton_->setChecked(pane == SidebarPane::Console);
    }
    if (sidebarCompileButton_ != nullptr) {
        sidebarCompileButton_->setChecked(false);
    }
}

void MainWindow::updateValidationRailIndicator()
{
    if (sidebarValidationButton_ == nullptr) {
        return;
    }

    const bool hasProblems = validationProblemCount_ > 0;
    sidebarValidationButton_->setProperty("validationProblems", hasProblems);
    const bool hasErrors = hasProblems &&
        validationHighestSeverity_ == TherionStudio::TherionSourceDiagnosticSeverity::Error;
    sidebarValidationButton_->setProperty(
        "validationSeverity",
        hasErrors ? QStringLiteral("error") : (hasProblems ? QStringLiteral("warning") : QStringLiteral("none")));
    if (hasErrors) {
        sidebarValidationButton_->setToolTip(tr("Validation: %1 problem(s), errors present")
                                                 .arg(validationProblemCount_));
    } else if (hasProblems) {
        sidebarValidationButton_->setToolTip(tr("Validation: %1 warning(s)").arg(validationProblemCount_));
    } else {
        sidebarValidationButton_->setToolTip(tr("Validation"));
    }
    sidebarValidationButton_->style()->unpolish(sidebarValidationButton_);
    sidebarValidationButton_->style()->polish(sidebarValidationButton_);
    sidebarValidationButton_->update();
}

void MainWindow::clearValidationRailIndicator()
{
    validationProblemCount_ = 0;
    validationHighestSeverity_ = TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    updateValidationRailIndicator();
}

void MainWindow::requestProjectSearch()
{
    if (projectSearchScanner_ == nullptr) {
        return;
    }

    const QString query = projectSearchEdit_ != nullptr ? projectSearchEdit_->text().trimmed() : QString();
    if (query.isEmpty()) {
        if (projectSearchStatusLabel_ != nullptr) {
            projectSearchStatusLabel_->clear();
            projectSearchStatusLabel_->setVisible(false);
        }
        if (searchResultsModel_ != nullptr) {
            searchResultsModel_->clear();
            searchResultsModel_->setHorizontalHeaderLabels({tr("Results")});
        }
        return;
    }

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        if (projectSearchStatusLabel_ != nullptr) {
            projectSearchStatusLabel_->setText(tr("Open a project before searching."));
            projectSearchStatusLabel_->setVisible(true);
        }
        return;
    }

    QHash<QString, QString> inMemoryProjectContentsByPath;
    auto captureInMemorySource = [&inMemoryProjectContentsByPath](QWidget *widget) {
        if (widget == nullptr) {
            return;
        }

        QString sourceFile;
        QString sourceText;
        if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
            sourceFile = textTab->filePath();
            sourceText = textTab->text();
        } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
            sourceFile = mapTab->filePath();
            sourceText = mapTab->text();
        } else {
            return;
        }

        const QString normalizedPath = canonicalOrAbsolutePath(sourceFile);
        if (!normalizedPath.isEmpty()) {
            inMemoryProjectContentsByPath.insert(normalizedPath, sourceText);
        }
    };

    for (int index = 0; editorTabs_ != nullptr && index < editorTabs_->count(); ++index) {
        captureInMemorySource(editorTabs_->widget(index));
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        captureInMemorySource(detachedTab);
    }

    if (projectSearchStatusLabel_ != nullptr) {
        projectSearchStatusLabel_->setText(tr("Searching..."));
        projectSearchStatusLabel_->setVisible(true);
    }
    if (projectSearchButton_ != nullptr) {
        projectSearchButton_->setEnabled(false);
    }
    if (searchResultsModel_ != nullptr) {
        searchResultsModel_->clear();
        searchResultsModel_->setHorizontalHeaderLabels({tr("Results")});
    }

    const bool wholeWord = projectSearchWholeWordCheck_ != nullptr
        && projectSearchWholeWordCheck_->isChecked();
    const bool matchCase = projectSearchMatchCaseCheck_ != nullptr
        && projectSearchMatchCaseCheck_->isChecked();
    projectSearchScanner_->requestSearch(projectRootPath_,
                                         query,
                                         wholeWord,
                                         matchCase,
                                         inMemoryProjectContentsByPath);
}

void MainWindow::handleProjectSearchFinished(const TherionStudio::ProjectSearchScanner::Result &result)
{
    if (projectSearchButton_ != nullptr) {
        projectSearchButton_->setEnabled(true);
    }

    const QString currentQuery = projectSearchEdit_ != nullptr ? projectSearchEdit_->text().trimmed() : QString();
    const bool currentWholeWord = projectSearchWholeWordCheck_ != nullptr
        && projectSearchWholeWordCheck_->isChecked();
    const bool currentMatchCase = projectSearchMatchCaseCheck_ != nullptr
        && projectSearchMatchCaseCheck_->isChecked();
    if (result.query != currentQuery
        || result.wholeWord != currentWholeWord
        || result.matchCase != currentMatchCase) {
        return;
    }
    if (searchResultsModel_ == nullptr) {
        return;
    }

    searchResultsModel_->clear();
    searchResultsModel_->setHorizontalHeaderLabels({tr("Results")});

    if (!result.errorMessage.isEmpty()) {
        if (projectSearchStatusLabel_ != nullptr) {
            projectSearchStatusLabel_->setText(result.errorMessage);
            projectSearchStatusLabel_->setVisible(true);
        }
        return;
    }

    if (result.matches.isEmpty()) {
        if (projectSearchStatusLabel_ != nullptr) {
            projectSearchStatusLabel_->setText(tr("No results in %1 searched file(s).")
                                                   .arg(result.searchedFileCount));
            projectSearchStatusLabel_->setVisible(true);
        }
        return;
    }

    QHash<QString, QStandardItem *> fileItemsByPath;
    for (const TherionStudio::ProjectSearchScanner::Match &match : result.matches) {
        QStandardItem *fileItem = fileItemsByPath.value(match.filePath, nullptr);
        if (fileItem == nullptr) {
            fileItem = new QStandardItem(projectRelativeDisplayPath(projectRootPath_, match.filePath));
            fileItem->setEditable(false);
            fileItem->setData(match.filePath, SearchFilePathRole);
            fileItem->setData(false, SearchIsMatchRole);
            searchResultsModel_->appendRow(fileItem);
            fileItemsByPath.insert(match.filePath, fileItem);
        }

        const QString lineText = match.lineText.isEmpty()
            ? tr("(empty line)")
            : match.lineText;
        auto *matchItem = new QStandardItem(QStringLiteral("%1:%2  %3")
                                                .arg(match.lineNumber)
                                                .arg(match.columnNumber)
                                                .arg(lineText));
        matchItem->setEditable(false);
        matchItem->setData(match.filePath, SearchFilePathRole);
        matchItem->setData(match.lineNumber, SearchLineNumberRole);
        matchItem->setData(match.columnNumber, SearchColumnNumberRole);
        matchItem->setData(true, SearchIsMatchRole);
        fileItem->appendRow(matchItem);
    }

    if (searchResultsTree_ != nullptr) {
        searchResultsTree_->expandAll();
        searchResultsTree_->resizeColumnToContents(0);
    }

    if (projectSearchStatusLabel_ != nullptr) {
        QString status = tr("%1 result(s) in %2 file(s).")
                             .arg(result.matches.size())
                             .arg(fileItemsByPath.size());
        if (result.limitReached) {
            status += QLatin1Char(' ');
            status += tr("Showing the first %1 result(s).").arg(result.matches.size());
        }
        projectSearchStatusLabel_->setText(status);
        projectSearchStatusLabel_->setVisible(true);
    }
}

void MainWindow::openProjectSearchResult(const QModelIndex &index)
{
    if (searchResultsModel_ == nullptr || !index.isValid()) {
        return;
    }

    QModelIndex matchIndex = index;
    if (!matchIndex.data(SearchIsMatchRole).toBool() && searchResultsModel_->hasChildren(matchIndex)) {
        matchIndex = searchResultsModel_->index(0, 0, matchIndex);
    }
    if (!matchIndex.isValid() || !matchIndex.data(SearchIsMatchRole).toBool()) {
        return;
    }

    const QString filePath = matchIndex.data(SearchFilePathRole).toString();
    const int lineNumber = matchIndex.data(SearchLineNumberRole).toInt();
    const int columnNumber = matchIndex.data(SearchColumnNumberRole).toInt();
    const QString query = projectSearchEdit_ != nullptr ? projectSearchEdit_->text().trimmed() : QString();
    if (filePath.isEmpty() || lineNumber <= 0) {
        return;
    }

    const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenProjectSearchResult(filePath);
    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenMapDocument) {
        auto *mapTab = openMapEditorTab(filePath);
        if (mapTab == nullptr) {
            return;
        }

        mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        mapTab->goToLine(lineNumber);
        if (!query.isEmpty()) {
            const bool wholeWord = projectSearchWholeWordCheck_ != nullptr
                && projectSearchWholeWordCheck_->isChecked();
            const bool matchCase = projectSearchMatchCaseCheck_ != nullptr
                && projectSearchMatchCaseCheck_->isChecked();
            mapTab->showFindBarWithText(query, false, wholeWord, matchCase);
        }
        return;
    }

    auto *textTab = openTextTab(filePath);
    if (textTab == nullptr) {
        return;
    }

    textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
    textTab->goToLineColumn(lineNumber, qMax(1, columnNumber));
    if (!query.isEmpty()) {
        const bool wholeWord = projectSearchWholeWordCheck_ != nullptr
            && projectSearchWholeWordCheck_->isChecked();
        const bool matchCase = projectSearchMatchCaseCheck_ != nullptr
            && projectSearchMatchCaseCheck_->isChecked();
        textTab->showFindBarWithText(query, false, wholeWord, matchCase);
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
        refreshWorkspaceModeSwitcherGeometry();
    };

    if (mainContentSplitter_->width() <= 0) {
        QTimer::singleShot(0, this, applyRestore);
        return;
    }
    applyRestore();
}

bool MainWindow::isSidebarEffectivelyCollapsed() const
{
    if (sidebarCollapsed_) {
        return true;
    }
    if (sidebarContentContainer_ == nullptr || !sidebarContentContainer_->isVisible()) {
        return true;
    }
    return sidebarContentContainer_->width() <= sidebarAutoSnapThreshold(sidebarRailWidth_);
}

void MainWindow::scheduleSidebarCollapseLayoutSync()
{
    if (sidebarCollapseSyncPending_) {
        return;
    }

    sidebarCollapseSyncPending_ = true;
    QTimer::singleShot(0, this, [this]() {
        sidebarCollapseSyncPending_ = false;
        if (sidebarCollapsed_) {
            setSidebarCollapsed(true);
            return;
        }

        updateSidebarCollapseButton();
        refreshViewMenuActions();
    });
}

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr) {
        return;
    }

    if (collapsed == sidebarCollapsed_ && collapsed == isSidebarEffectivelyCollapsed()) {
        updateSidebarCollapseButton();
        refreshViewMenuActions();
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
            refreshWorkspaceModeSwitcherGeometry();
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
    refreshViewMenuActions();
}

void MainWindow::updateSidebarCollapseButton()
{
    if (sidebarCollapseButton_ == nullptr || sidebarContainer_ == nullptr) {
        return;
    }

    const bool collapsed = isSidebarEffectivelyCollapsed();
    const Qt::ArrowType arrowType = collapsed ? Qt::RightArrow : Qt::LeftArrow;
    sidebarCollapseButton_->setArrowType(arrowType);
    sidebarCollapseButton_->setToolTip(collapsed ? tr("Expand sidebar") : tr("Collapse sidebar"));
}
