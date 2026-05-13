#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "MainWindowStructureRoles.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QItemSelectionModel>
#include <QLabel>
#include <QModelIndex>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>

#include <functional>

#include "TextEditorTab.h"
#include "MapEditorTab.h"
#include "../core/ProjectStructureIndex.h"

namespace
{
QString inspectorObjectKindLabel(const QString &category)
{
    if (category == QStringLiteral("Stations")) {
        return QObject::tr("Station Point");
    }
    if (category == QStringLiteral("Points")) {
        return QObject::tr("Point");
    }
    if (category == QStringLiteral("Lines")) {
        return QObject::tr("Line");
    }
    if (category == QStringLiteral("Areas")) {
        return QObject::tr("Area");
    }
    if (category == QStringLiteral("Scraps")) {
        return QObject::tr("Scrap");
    }
    if (category == QStringLiteral("Maps")) {
        return QObject::tr("Map");
    }
    if (category == QStringLiteral("Surveys")) {
        return QObject::tr("Survey");
    }
    if (category == QStringLiteral("Centrelines")) {
        return QObject::tr("Centreline");
    }

    return category;
}

QString formatProjectStructureSummary(const QHash<QString, int> &categoryCounts, int totalItems, int rootSurveyCount)
{
    const int surveyCount = categoryCounts.value(QStringLiteral("Surveys"));
    const int centrelineCount = categoryCounts.value(QStringLiteral("Centrelines"));
    const int mapCount = categoryCounts.value(QStringLiteral("Maps"));
    const int scrapCount = categoryCounts.value(QStringLiteral("Scraps"));
    const int stationCount = categoryCounts.value(QStringLiteral("Stations"));
    const int pointCount = categoryCounts.value(QStringLiteral("Points"));
    const int lineCount = categoryCounts.value(QStringLiteral("Lines"));
    const int areaCount = categoryCounts.value(QStringLiteral("Areas"));

    return QObject::tr("%1 structure item(s) across %2 survey root(s): %3 survey, %4 centreline, %5 map, %6 scrap, %7 station, %8 point, %9 line, %10 area")
        .arg(totalItems)
        .arg(rootSurveyCount)
        .arg(surveyCount)
        .arg(centrelineCount)
        .arg(mapCount)
        .arg(scrapCount)
        .arg(stationCount)
        .arg(pointCount)
        .arg(lineCount)
        .arg(areaCount);
}

QStandardItem *createIndexedItem(const QString &text, const QString &sourceFile, int lineNumber, const QString &category, const QString &name)
{
    auto *item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(sourceFile, SourceFileRole);
    item->setData(lineNumber, LineNumberRole);
    item->setData(category, CategoryRole);
    item->setData(name, NameRole);
    return item;
}

QString structureBrowserItemText(const TherionStudio::ProjectStructureEntry &entry)
{
    if (entry.category == QStringLiteral("Surveys") && !entry.createsNamespace) {
        return QObject::tr("%1 (namespace off)").arg(entry.name);
    }

    return entry.name;
}

QString mapObjectItemText(const TherionStudio::ProjectStructureEntry &entry)
{
    if (entry.category == QStringLiteral("Scraps")) {
        return entry.name;
    }

    return QStringLiteral("%1: %2").arg(inspectorObjectKindLabel(entry.category), entry.name);
}
}

void MainWindow::rebuildStructureSidebar()
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Structure")});

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        auto *rootItem = new QStandardItem(tr("Open a project to populate the survey hierarchy"));
        rootItem->setEditable(false);
        structureModel_->appendRow(rootItem);
        projectStructureSummary_ = tr("Open a project to view its survey hierarchy summary.");
        inspectorSummary_->setText(projectStructureSummary_);
        structureTree_->expandAll();
        return;
    }

    QString errorMessage;
    const QVector<TherionStudio::ProjectStructureEntry> entries = TherionStudio::ProjectStructureIndex::scanProject(projectRootPath_, &errorMessage);
    if (!errorMessage.isEmpty()) {
        appendConsoleLine(errorMessage);
    }

    QHash<QString, int> categoryCounts;
    int totalItems = 0;
    int rootSurveyCount = 0;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        categoryCounts[entry.category] += 1;
        ++totalItems;
        if (entry.category == QStringLiteral("Surveys") && entry.depth == 0) {
            ++rootSurveyCount;
        }
    }

    projectStructureSummary_ = tr("Project structure summary: %1")
                                   .arg(formatProjectStructureSummary(categoryCounts, totalItems, rootSurveyCount));

    auto *projectItem = new QStandardItem(QFileInfo(projectRootPath_).fileName().isEmpty()
                                              ? projectRootPath_
                                              : QFileInfo(projectRootPath_).fileName());
    projectItem->setEditable(false);
    projectItem->setData(projectRootPath_, SourceFileRole);
    projectItem->setData(0, LineNumberRole);
    projectItem->setData(QStringLiteral("Project"), CategoryRole);
    projectItem->setData(projectItem->text(), NameRole);
    structureModel_->appendRow(projectItem);

    auto *summaryItem = new QStandardItem(projectStructureSummary_);
    summaryItem->setEditable(false);
    projectItem->appendRow(summaryItem);

    if (entries.isEmpty()) {
        auto *emptyItem = new QStandardItem(tr("No survey hierarchy was found in the selected project"));
        emptyItem->setEditable(false);
        projectItem->appendRow(emptyItem);
        inspectorSummary_->setText(projectStructureSummary_);
    } else {
        QVector<QStandardItem *> parentStack;
        for (const TherionStudio::ProjectStructureEntry &entry : entries) {
            while (parentStack.size() > entry.depth) {
                parentStack.removeLast();
            }

            QString entryName = entry.name;
            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = structureOverrideKey(entry.sourceFile, entry.lineNumber);
                entryName = structureNameOverrides_.value(overrideKey, entry.name);
            }

            TherionStudio::ProjectStructureEntry displayEntry = entry;
            displayEntry.name = entryName;

            auto *entryItem = createIndexedItem(structureBrowserItemText(displayEntry),
                                                entry.sourceFile,
                                                entry.lineNumber,
                                                entry.category,
                                                entryName);

            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = structureOverrideKey(entry.sourceFile, entry.lineNumber);
                entryItem->setData(overrideKey, Qt::UserRole + 5);
            }

            QStandardItem *parentItem = parentStack.isEmpty() ? projectItem : parentStack.last();
            parentItem->appendRow(entryItem);
            parentStack.append(entryItem);
        }

        inspectorSummary_->setText(projectStructureSummary_);
    }

    structureTree_->expandAll();
}

void MainWindow::rebuildMapObjectsTree()
{
    if (mapObjectsModel_ == nullptr) {
        return;
    }

    mapObjectsModel_->clear();
    mapObjectsModel_->setHorizontalHeaderLabels({tr("Objects")});

    QWidget *tabWidget = currentDocumentWidget();
    const QString filePath = tabWidget != nullptr ? documentPathForWidget(tabWidget) : QString();
    if (tabWidget == nullptr || !filePath.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        auto *placeholderItem = new QStandardItem(tr("Open a TH2 document to browse its objects by scrap"));
        placeholderItem->setEditable(false);
        mapObjectsModel_->appendRow(placeholderItem);
        refreshMapBackgroundPanel();
        return;
    }

    QString text;
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        text = textTab->text();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        text = mapTab->text();
    }

    const QVector<TherionStudio::ProjectStructureEntry> entries = TherionStudio::ProjectStructureIndex::scanTh2Objects(filePath, text);
    if (entries.isEmpty()) {
        auto *placeholderItem = new QStandardItem(tr("No TH2 scraps, points, lines, or areas were found in the current document"));
        placeholderItem->setEditable(false);
        mapObjectsModel_->appendRow(placeholderItem);
        refreshMapBackgroundPanel();
        return;
    }

    QVector<QStandardItem *> parentStack;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        while (parentStack.size() > entry.depth) {
            parentStack.removeLast();
        }

        auto *entryItem = createIndexedItem(mapObjectItemText(entry), entry.sourceFile, entry.lineNumber, entry.category, entry.name);
        QStandardItem *parentItem = parentStack.isEmpty() ? mapObjectsModel_->invisibleRootItem() : parentStack.last();
        parentItem->appendRow(entryItem);
        parentStack.append(entryItem);
    }

    mapObjectsTree_->expandAll();
    updateMapObjectSelectionFromEditorLocation(filePath, documentCurrentLineNumberForWidget(tabWidget));
    refreshMapBackgroundPanel();
}

void MainWindow::handleStructureSelectionChanged(const QModelIndex &current, const QModelIndex &, QTreeView *)
{
    updateInspectorFromStructureItem(current);

    if (!current.isValid()) {
        return;
    }

    if (isStructureContainerIndex(current)) {
        return;
    }

    const QString sourceFile = current.data(SourceFileRole).toString();
    const int lineNumber = current.data(LineNumberRole).toInt();
    if (sourceFile.isEmpty()) {
        return;
    }

    QWidget *tabWidget = QFileInfo(sourceFile).suffix().toLower() == QStringLiteral("th2")
        ? static_cast<QWidget *>(openMapEditorTab(sourceFile))
        : static_cast<QWidget *>(openTextTab(sourceFile));
    if (tabWidget != nullptr && lineNumber > 0) {
        documentGoToLineForWidget(tabWidget, lineNumber);
    }
}

void MainWindow::handleStructureItemActivated(const QModelIndex &index, QTreeView *sourceTree)
{
    QTreeView *tree = sourceTree != nullptr ? sourceTree : structureTree_;
    if (!index.isValid()) {
        return;
    }

    if (isStructureContainerIndex(index)) {
        if (tree != nullptr && tree->isExpanded(index)) {
            tree->collapse(index);
        } else {
            if (tree != nullptr) {
                tree->expand(index);
            }
        }

        const QModelIndex sourceIndex = firstStructureSourceIndex(index);
        if (sourceIndex.isValid()) {
            if (tree != nullptr) {
                tree->setCurrentIndex(sourceIndex);
            }
        }
        return;
    }

    handleStructureSelectionChanged(index, QModelIndex(), tree);
}

void MainWindow::updateMapEditorActionState()
{
    const bool hasTh2Document = currentDocumentSupportsMapPane();

    if (openMapEditorAction_ != nullptr) {
        openMapEditorAction_->setEnabled(hasTh2Document);
    }

    if (sidebarMapButton_ != nullptr) {
        sidebarMapButton_->setEnabled(hasTh2Document);
        if (!hasTh2Document && activeSidebarPane_ == SidebarPane::MapEditor) {
            setSidebarPane(lastNonMapSidebarPane_);
            return;
        }
    }

    refreshMapBackgroundPanel();
}

bool MainWindow::currentDocumentSupportsMapPane() const
{
    QWidget *tabWidget = currentDocumentWidget();
    return tabWidget != nullptr && documentPathForWidget(tabWidget).endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive);
}

QModelIndex MainWindow::firstStructureSourceIndex(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() == nullptr) {
        return QModelIndex();
    }

    if (!index.data(SourceFileRole).toString().isEmpty() && index.data(LineNumberRole).toInt() > 0) {
        return index;
    }

    const QAbstractItemModel *model = index.model();
    const int rowCount = model->rowCount(index);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = model->index(row, 0, index);
        const QModelIndex sourceIndex = firstStructureSourceIndex(childIndex);
        if (sourceIndex.isValid()) {
            return sourceIndex;
        }
    }

    return QModelIndex();
}

bool MainWindow::isStructureContainerIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    const QString category = index.data(CategoryRole).toString();
    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();
    return category == QStringLiteral("Project") || sourceFile.isEmpty() || lineNumber <= 0;
}

QModelIndex MainWindow::findStructureIndexForSourceLocation(const QString &filePath, int lineNumber) const
{
    if (structureModel_ == nullptr || filePath.isEmpty() || lineNumber <= 0) {
        return QModelIndex();
    }

    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    QModelIndex bestIndex;
    int bestLineNumber = -1;

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = structureModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = structureModel_->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const QString childSourceFile = childIndex.data(SourceFileRole).toString();
            const QString candidateSourceFile = QFileInfo(childSourceFile).canonicalFilePath().isEmpty()
                                                    ? QFileInfo(childSourceFile).absoluteFilePath()
                                                    : QFileInfo(childSourceFile).canonicalFilePath();
            const int candidateLineNumber = childIndex.data(LineNumberRole).toInt();

            if (candidateSourceFile == canonicalPath && candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };

    visitNode(QModelIndex());
    return bestIndex;
}

void MainWindow::updateStructureSelectionFromEditorLocation(const QString &filePath, int lineNumber)
{
    const QModelIndex targetIndex = findStructureIndexForSourceLocation(filePath, lineNumber);
    if (!targetIndex.isValid() || targetIndex == structureTree_->currentIndex()) {
        return;
    }

    if (structureTree_->selectionModel() != nullptr) {
        QSignalBlocker blocker(structureTree_->selectionModel());
        structureTree_->setCurrentIndex(targetIndex);
    } else {
        structureTree_->setCurrentIndex(targetIndex);
    }

    structureTree_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
}

QModelIndex MainWindow::findMapObjectIndexForSourceLocation(const QString &filePath, int lineNumber) const
{
    if (mapObjectsModel_ == nullptr || filePath.isEmpty() || lineNumber <= 0) {
        return QModelIndex();
    }

    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    QModelIndex bestIndex;
    int bestLineNumber = -1;

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = mapObjectsModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = mapObjectsModel_->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const QString childSourceFile = childIndex.data(SourceFileRole).toString();
            const QString candidateSourceFile = QFileInfo(childSourceFile).canonicalFilePath().isEmpty()
                                                    ? QFileInfo(childSourceFile).absoluteFilePath()
                                                    : QFileInfo(childSourceFile).canonicalFilePath();
            const int candidateLineNumber = childIndex.data(LineNumberRole).toInt();

            if (candidateSourceFile == canonicalPath && candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };

    visitNode(QModelIndex());
    return bestIndex;
}

void MainWindow::updateMapObjectSelectionFromEditorLocation(const QString &filePath, int lineNumber)
{
    if (mapObjectsTree_ == nullptr) {
        return;
    }

    const QModelIndex targetIndex = findMapObjectIndexForSourceLocation(filePath, lineNumber);
    if (!targetIndex.isValid() || targetIndex == mapObjectsTree_->currentIndex()) {
        return;
    }

    if (mapObjectsTree_->selectionModel() != nullptr) {
        QSignalBlocker blocker(mapObjectsTree_->selectionModel());
        mapObjectsTree_->setCurrentIndex(targetIndex);
    } else {
        mapObjectsTree_->setCurrentIndex(targetIndex);
    }

    mapObjectsTree_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
    updateInspectorFromStructureItem(targetIndex);
}
