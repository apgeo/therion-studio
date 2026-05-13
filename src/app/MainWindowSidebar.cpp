#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QDockWidget>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QPushButton>
#include <QTreeView>

#include <functional>

#include "TextEditorTab.h"
#include "MapEditorTab.h"
#include "../core/DocumentFile.h"
#include "../core/ProjectStructureIndex.h"
#include "../core/SessionStore.h"
#include "../core/TherionDocumentEditor.h"

namespace
{
constexpr int SourceFileRole = Qt::UserRole + 1;
constexpr int LineNumberRole = Qt::UserRole + 2;
constexpr int CategoryRole = Qt::UserRole + 3;
constexpr int NameRole = Qt::UserRole + 4;

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

QString inspectorNameLabelForCategory(const QString &category)
{
    if (category == QStringLiteral("Stations")) {
        return QObject::tr("Station Name");
    }
    if (category == QStringLiteral("Points")) {
        return QObject::tr("Point Name");
    }
    if (category == QStringLiteral("Lines")) {
        return QObject::tr("Line Name");
    }
    if (category == QStringLiteral("Areas")) {
        return QObject::tr("Area Name");
    }
    if (category == QStringLiteral("Scraps")) {
        return QObject::tr("Scrap Name");
    }
    if (category == QStringLiteral("Maps")) {
        return QObject::tr("Map Name");
    }
    if (category == QStringLiteral("Surveys")) {
        return QObject::tr("Survey Name");
    }

    return QObject::tr("Name");
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

void MainWindow::updateInspectorFromStructureItem(const QModelIndex &index)
{
    if (!index.isValid()) {
        inspectorSummary_->setText(projectStructureSummary_.isEmpty() ? tr("Open a project to view structure details.") : projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Category"));
        inspectorNameLabel_->setText(tr("Name"));
        inspectorCategoryValue_->setText(tr("-"));
        inspectorNameEdit_->setText(QString());
        inspectorNameEdit_->setEnabled(false);
        inspectorProjectValue_->setText(tr("-"));
        inspectorRelativePathValue_->setText(tr("-"));
        inspectorObjectKindValue_->setText(tr("-"));
        inspectorEditabilityValue_->setText(tr("-"));
        inspectorSourceValue_->setText(tr("-"));
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("Select a structure item to edit its metadata."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setEnabled(false);
        selectedStructureIndex_ = QPersistentModelIndex();
        selectedStructureSourceFile_.clear();
        selectedStructureName_.clear();
        selectedStructureLine_ = 0;
        return;
    }

    const QString category = index.data(CategoryRole).toString();
    const QString name = index.data(NameRole).toString();
    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();

    const bool isProjectNode = category == QStringLiteral("Project");
    const bool isContainerNode = sourceFile.isEmpty() || lineNumber <= 0 || isProjectNode;

    if (isProjectNode) {
        selectedStructureIndex_ = QPersistentModelIndex(index);
        inspectorSummary_->setText(projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Node Type"));
        inspectorNameLabel_->setText(tr("Project"));
        inspectorCategoryValue_->setText(tr("Project"));
        inspectorNameEdit_->setEnabled(false);
        inspectorNameEdit_->setText(name);
        inspectorSourceValue_->setText(QDir(projectRootPath_).absolutePath());
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("Project nodes summarize the current project and are not directly editable."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setText(tr("Open First Item"));
        inspectorOpenSourceButton_->setEnabled(selectedStructureIndex_.isValid() ? firstStructureSourceIndex(selectedStructureIndex_).isValid() : false);
        return;
    }

    if (category.isEmpty() || name.isEmpty()) {
        inspectorSummary_->setText(projectStructureSummary_.isEmpty() ? tr("Open a project to view structure details.") : projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Category"));
        inspectorNameLabel_->setText(tr("Name"));
        inspectorCategoryValue_->setText(tr("-"));
        inspectorNameEdit_->setText(QString());
        inspectorNameEdit_->setEnabled(false);
        inspectorProjectValue_->setText(tr("-"));
        inspectorRelativePathValue_->setText(tr("-"));
        inspectorObjectKindValue_->setText(tr("-"));
        inspectorEditabilityValue_->setText(tr("-"));
        inspectorSourceValue_->setText(tr("-"));
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("The selected row is not a structure object that can be edited."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setText(tr("Open Source"));
        inspectorOpenSourceButton_->setEnabled(false);
        selectedStructureIndex_ = QPersistentModelIndex();
        selectedStructureSourceFile_.clear();
        selectedStructureName_.clear();
        selectedStructureLine_ = 0;
        return;
    }

    selectedStructureIndex_ = QPersistentModelIndex(index);
    selectedStructureSourceFile_ = sourceFile;
    selectedStructureName_ = name;
    selectedStructureLine_ = lineNumber;

    const QString objectKind = inspectorObjectKindLabel(category);
    const QString nameLabel = inspectorNameLabelForCategory(category);
    const QString relativePath = QDir(projectRootPath_).relativeFilePath(sourceFile);
    const QString projectName = QFileInfo(projectRootPath_).fileName().isEmpty() ? projectRootPath_ : QFileInfo(projectRootPath_).fileName();

    inspectorSummary_->setText(tr("%1: %2\nSource: %3:%4")
                                   .arg(objectKind)
                                   .arg(name)
                                   .arg(relativePath)
                                   .arg(lineNumber));
    inspectorCategoryLabel_->setText(tr("Object Kind"));
    inspectorNameLabel_->setText(nameLabel);
    inspectorCategoryValue_->setText(category);
    inspectorNameEdit_->setEnabled(!isContainerNode);
    inspectorNameEdit_->setText(name);
    inspectorProjectValue_->setText(projectName);
    inspectorRelativePathValue_->setText(relativePath);
    inspectorObjectKindValue_->setText(objectKind);
    inspectorEditabilityValue_->setText(isContainerNode ? tr("Container") : tr("Editable in-memory"));
    inspectorSourceValue_->setText(sourceFile);
    inspectorLineValue_->setText(lineNumber > 0 ? QString::number(lineNumber) : tr("-"));
    inspectorOpenSourceButton_->setText(isContainerNode ? tr("Open First Item") : tr("Open Source"));
    inspectorOpenSourceButton_->setEnabled(isContainerNode ? firstStructureSourceIndex(index).isValid() : true);
    inspectorValidationLabel_->setText(isContainerNode
                                           ? tr("This row groups child objects; use Open First Item or double-click to drill down.")
                                           : tr("Editing is currently in-memory for the selected structure entry."));
    inspectorApplyButton_->setEnabled(false);
}

void MainWindow::handleInspectorNameEdited(const QString &text)
{
    const bool canApply = selectedStructureIndex_.isValid() && !text.trimmed().isEmpty() && text.trimmed() != selectedStructureName_;
    if (!text.trimmed().isEmpty()) {
        inspectorValidationLabel_->setText(tr("Editing is currently in-memory for the selected structure entry."));
    } else {
        inspectorValidationLabel_->setText(tr("Name is required."));
    }
    inspectorApplyButton_->setEnabled(canApply);
}

void MainWindow::handleInspectorApplyTriggered()
{
    if (!selectedStructureIndex_.isValid()) {
        return;
    }

    const QString newName = inspectorNameEdit_->text().trimmed();
    if (newName.isEmpty()) {
        inspectorValidationLabel_->setText(tr("Name is required."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    if (newName == selectedStructureName_) {
        inspectorValidationLabel_->setText(tr("No changes to apply."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    const QString category = selectedStructureIndex_.data(CategoryRole).toString();
    const QString sourceFile = selectedStructureIndex_.data(SourceFileRole).toString();
    const int lineNumber = selectedStructureIndex_.data(LineNumberRole).toInt();
    if (category.isEmpty() || sourceFile.isEmpty()) {
        inspectorValidationLabel_->setText(tr("The selected structure item cannot be updated."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    QString errorMessage;
    QWidget *tabWidget = documentWidgetForFilePath(sourceFile);
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        if (!textTab->rewriteStructureEntryName(lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!textTab->save(&errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        if (!mapTab->rewriteStructureEntryName(lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!mapTab->save(&errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    } else {
        QString contents;
        if (!TherionStudio::DocumentFile::readUtf8TextFile(sourceFile, &contents, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!TherionStudio::TherionDocumentEditor::rewriteStructureEntryName(&contents, lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!TherionStudio::DocumentFile::writeUtf8TextFile(sourceFile, contents, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    }

    const QString overrideKey = structureOverrideKey(sourceFile, lineNumber);
    structureNameOverrides_.remove(overrideKey);
    saveStructureNameOverrides();
    rebuildStructureSidebar();
    rebuildMapObjectsTree();
    updateStructureSelectionFromEditorLocation(sourceFile, lineNumber);
    updateMapObjectSelectionFromEditorLocation(sourceFile, lineNumber);
    inspectorSummary_->setText(tr("%1: %2\nSource: %3:%4")
                                   .arg(inspectorObjectKindLabel(category))
                                   .arg(newName)
                                   .arg(QDir(projectRootPath_).relativeFilePath(sourceFile))
                                   .arg(lineNumber));
    inspectorValidationLabel_->setText(tr("Updated the selected structure item in the source document."));
    inspectorApplyButton_->setEnabled(false);
}

void MainWindow::openSelectedStructureSource()
{
    if (!selectedStructureIndex_.isValid()) {
        return;
    }

    QModelIndex sourceIndex = selectedStructureIndex_;
    if (isStructureContainerIndex(sourceIndex)) {
        sourceIndex = firstStructureSourceIndex(sourceIndex);
    }

    if (!sourceIndex.isValid()) {
        return;
    }

    const QString sourceFile = sourceIndex.data(SourceFileRole).toString();
    const int lineNumber = sourceIndex.data(LineNumberRole).toInt();
    if (sourceFile.isEmpty()) {
        return;
    }

    QWidget *tabWidget = nullptr;
    if (QFileInfo(sourceFile).suffix().toLower() == QStringLiteral("th2")) {
        tabWidget = openMapEditorTab(sourceFile);
    } else {
        tabWidget = openTextTab(sourceFile);
    }

    if (tabWidget != nullptr && lineNumber > 0) {
        documentGoToLineForWidget(tabWidget, lineNumber);
    }
}

void MainWindow::updateMapEditorActionState()
{
    const bool hasTh2Document = currentDocumentSupportsMapPane();

    if (openMapEditorAction_ != nullptr) {
        openMapEditorAction_->setEnabled(hasTh2Document);
    }

    if (sidebarModeTabs_ != nullptr) {
        const int mapPaneIndex = static_cast<int>(SidebarPane::MapEditor);
        sidebarModeTabs_->setTabEnabled(mapPaneIndex, hasTh2Document);

        if (!hasTh2Document && activeSidebarPane_ == SidebarPane::MapEditor) {
            setSidebarPane(lastNonMapSidebarPane_);
            return;
        }

        updateSidebarModeTabIcons(sidebarModeTabs_, static_cast<int>(activeSidebarPane_));
    }
}

bool MainWindow::currentDocumentSupportsMapPane() const
{
    QWidget *tabWidget = currentDocumentWidget();
    return tabWidget != nullptr && documentPathForWidget(tabWidget).endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive);
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