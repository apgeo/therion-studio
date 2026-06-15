#include "MainWindow.h"

#include "MainWindowDocumentOpenController.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>

namespace
{
enum ProjectSearchResultRole
{
    SearchFilePathRole = Qt::UserRole + 70,
    SearchLineNumberRole,
    SearchColumnNumberRole,
    SearchIsMatchRole,
};

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
