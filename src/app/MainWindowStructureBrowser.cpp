#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "MainWindowStructureRoles.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSvgRenderer>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>

#include <functional>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "../core/ProjectStructureIndex.h"
#include "../core/IFileSystem.h"
#include "../core/TherionDocumentParser.h"

namespace
{
QString structureObjectKindLabel(const QString &category)
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

    return QStringLiteral("%1: %2").arg(structureObjectKindLabel(entry.category), entry.name);
}

QPixmap renderStructureLucidePixmap(const QString &iconName, const QColor &color, int extent)
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

    const qreal devicePixelRatio = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(extent, extent) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, extent, extent));
    return pixmap;
}

QIcon structureLucideIcon(const QString &iconName)
{
    const QPalette palette = QApplication::palette();
    QIcon icon;
    icon.addPixmap(renderStructureLucidePixmap(iconName, palette.color(QPalette::Text), 16), QIcon::Normal);
    icon.addPixmap(renderStructureLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::Text), 16), QIcon::Disabled);
    return icon;
}

QIcon structureItemIconForCategory(const QString &category)
{
    if (category == QStringLiteral("Surveys")) {
        return structureLucideIcon(QStringLiteral("compass"));
    }
    if (category == QStringLiteral("Maps")) {
        return structureLucideIcon(QStringLiteral("map"));
    }
    if (category == QStringLiteral("Scraps")) {
        return structureLucideIcon(QStringLiteral("puzzle"));
    }

    return QIcon();
}

QString structureEntryNodeKey(const TherionStudio::ProjectStructureEntry &entry)
{
    const QString normalizedPath = QFileInfo(entry.sourceFile).absoluteFilePath();
    return QStringLiteral("%1:%2").arg(normalizedPath).arg(entry.lineNumber);
}

QString normalizedStructurePathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

QHash<QString, QSet<QString>> mapScrapReferencesByMapKey(TherionStudio::IFileSystem &fileSystem,
                                                         const QVector<TherionStudio::ProjectStructureEntry> &entries)
{
    QHash<QString, QSet<QString>> referencesByMap;
    QSet<QString> knownScrapNames;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        if (entry.category == QStringLiteral("Scraps") && !entry.name.trimmed().isEmpty()) {
            knownScrapNames.insert(entry.name.trimmed().toLower());
        }
    }
    if (knownScrapNames.isEmpty()) {
        return referencesByMap;
    }

    QHash<QString, QVector<TherionStudio::ProjectStructureEntry>> mapsBySourceFile;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        if (entry.category == QStringLiteral("Maps") && !entry.sourceFile.isEmpty() && entry.lineNumber > 0) {
            mapsBySourceFile[QFileInfo(entry.sourceFile).absoluteFilePath()].append(entry);
        }
    }
    if (mapsBySourceFile.isEmpty()) {
        return referencesByMap;
    }

    for (auto fileIt = mapsBySourceFile.constBegin(); fileIt != mapsBySourceFile.constEnd(); ++fileIt) {
        QString contents;
        if (!fileSystem.readUtf8TextFile(fileIt.key(), &contents, nullptr)) {
            continue;
        }
        const QVector<TherionStudio::TherionParsedLine> parsedLines = TherionStudio::TherionDocumentParser::parseText(contents);
        if (parsedLines.isEmpty()) {
            continue;
        }

        for (const TherionStudio::ProjectStructureEntry &mapEntry : fileIt.value()) {
            int mapLineIndex = -1;
            for (int index = 0; index < parsedLines.size(); ++index) {
                if (parsedLines.at(index).lineNumber == mapEntry.lineNumber
                    && parsedLines.at(index).directive == QStringLiteral("map")) {
                    mapLineIndex = index;
                    break;
                }
            }
            if (mapLineIndex < 0) {
                continue;
            }

            QSet<QString> scrapReferences;
            int mapDepth = 0;
            for (int index = mapLineIndex; index < parsedLines.size(); ++index) {
                const TherionStudio::TherionParsedLine &parsedLine = parsedLines.at(index);
                const QString directive = parsedLine.directive;

                if (directive == QStringLiteral("map")) {
                    ++mapDepth;
                    continue;
                }
                if (directive == QStringLiteral("endmap")) {
                    --mapDepth;
                    if (mapDepth <= 0) {
                        break;
                    }
                    continue;
                }
                if (mapDepth != 1) {
                    continue;
                }
                if (parsedLine.tokens.size() != 1) {
                    continue;
                }

                const QString candidate = parsedLine.tokens.first().trimmed().toLower();
                if (candidate.isEmpty() || candidate.startsWith(QLatin1Char('-'))) {
                    continue;
                }
                if (knownScrapNames.contains(candidate)) {
                    scrapReferences.insert(candidate);
                }
            }

            referencesByMap.insert(structureEntryNodeKey(mapEntry), scrapReferences);
        }
    }

    return referencesByMap;
}
}

void MainWindow::requestStructureSidebarRebuild()
{
    if (structureSidebarScanner_ == nullptr) {
        return;
    }

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        return;
    }

    QHash<QString, QString> inMemoryProjectContentsByPath;
    auto captureInMemoryStructureSource = [&inMemoryProjectContentsByPath](QWidget *widget) {
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

        const QString normalizedPath = normalizedStructurePathKey(sourceFile);
        if (normalizedPath.isEmpty()) {
            return;
        }
        inMemoryProjectContentsByPath.insert(normalizedPath, sourceText);
    };

    for (int index = 0; editorTabs_ != nullptr && index < editorTabs_->count(); ++index) {
        captureInMemoryStructureSource(editorTabs_->widget(index));
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        captureInMemoryStructureSource(detachedTab);
    }

    structureSidebarScanner_->requestScan(projectRootPath_, inMemoryProjectContentsByPath);
}

void MainWindow::handleStructureSidebarScanFinished(const TherionStudio::ProjectStructureScanner::Result &result)
{
    if (!result.errorMessage.isEmpty()) {
        appendConsoleLine(result.errorMessage);
    }

    if (result.projectRootPath == projectRootPath_
        && !projectRootPath_.isEmpty()
        && QDir(projectRootPath_).exists()) {
        applyStructureSidebarEntries(result.entries);
    }
}

void MainWindow::rebuildStructureSidebar()
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        auto *rootItem = new QStandardItem(tr("Open a project to populate the survey hierarchy"));
        rootItem->setEditable(false);
        structureModel_->appendRow(rootItem);
        projectStructureSummary_ = tr("Open a project to view its survey hierarchy summary.");
        structureTree_->expandAll();
        return;
    }

    requestStructureSidebarRebuild();
}

void MainWindow::applyStructureSidebarEntries(const QVector<TherionStudio::ProjectStructureEntry> &entries)
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});

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
    const auto includeInStructureView = [](const QString &category) {
        return category == QStringLiteral("Surveys")
            || category == QStringLiteral("Maps")
            || category == QStringLiteral("Scraps");
    };

    if (entries.isEmpty()) {
        auto *emptyItem = new QStandardItem(tr("No survey hierarchy was found in the selected project"));
        emptyItem->setEditable(false);
        structureModel_->appendRow(emptyItem);
    } else {
        struct VisibleStructureNode
        {
            TherionStudio::ProjectStructureEntry entry;
            QString entryName;
            QString nodeKey;
            QString forcedMapParentKey;
            QStandardItem *item = nullptr;
        };

        const QHash<QString, QSet<QString>> mapScrapRefs = mapScrapReferencesByMapKey(fileSystem_, entries);
        QHash<QString, QSet<QString>> mapOwnersByScrapName;
        for (auto it = mapScrapRefs.constBegin(); it != mapScrapRefs.constEnd(); ++it) {
            for (const QString &scrapNameLower : it.value()) {
                mapOwnersByScrapName[scrapNameLower].insert(it.key());
            }
        }

        QVector<VisibleStructureNode> nodes;
        nodes.reserve(entries.size());

        for (const TherionStudio::ProjectStructureEntry &entry : entries) {
            if (!includeInStructureView(entry.category)) {
                continue;
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
            const QIcon entryIcon = structureItemIconForCategory(entry.category);
            if (!entryIcon.isNull()) {
                entryItem->setIcon(entryIcon);
            }

            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = structureOverrideKey(entry.sourceFile, entry.lineNumber);
                entryItem->setData(overrideKey, Qt::UserRole + 5);
            }

            VisibleStructureNode node;
            node.entry = entry;
            node.entryName = entryName;
            node.nodeKey = structureEntryNodeKey(entry);
            node.item = entryItem;

            if (entry.category == QStringLiteral("Scraps")) {
                const QString scrapNameLower = entryName.trimmed().toLower();
                const QSet<QString> owners = mapOwnersByScrapName.value(scrapNameLower);
                if (owners.size() == 1) {
                    node.forcedMapParentKey = *owners.constBegin();
                }
            }

            nodes.append(node);
        }

        if (nodes.isEmpty()) {
            auto *emptyItem = new QStandardItem(tr("No surveys, maps, or scraps were found in the selected project"));
            emptyItem->setEditable(false);
            structureModel_->appendRow(emptyItem);
            structureTree_->expandAll();
            return;
        }

        QHash<QString, QStandardItem *> mapItemByKey;
        for (const VisibleStructureNode &node : std::as_const(nodes)) {
            if (node.entry.category == QStringLiteral("Maps")) {
                mapItemByKey.insert(node.nodeKey, node.item);
            }
        }

        QVector<int> visibleNodeIndexByDepth;
        for (int nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            VisibleStructureNode &node = nodes[nodeIndex];

            while (visibleNodeIndexByDepth.size() <= node.entry.depth) {
                visibleNodeIndexByDepth.append(-1);
            }
            for (int depth = node.entry.depth; depth < visibleNodeIndexByDepth.size(); ++depth) {
                visibleNodeIndexByDepth[depth] = -1;
            }

            QStandardItem *parentItem = nullptr;
            if (!node.forcedMapParentKey.isEmpty()) {
                parentItem = mapItemByKey.value(node.forcedMapParentKey, nullptr);
            }
            if (parentItem == nullptr) {
                for (int parentDepth = node.entry.depth - 1; parentDepth >= 0; --parentDepth) {
                    if (parentDepth >= visibleNodeIndexByDepth.size()) {
                        continue;
                    }
                    const int parentNodeIndex = visibleNodeIndexByDepth.at(parentDepth);
                    if (parentNodeIndex >= 0 && parentNodeIndex < nodes.size()) {
                        parentItem = nodes.at(parentNodeIndex).item;
                        break;
                    }
                }
            }

            if (parentItem != nullptr) {
                parentItem->appendRow(node.item);
            } else {
            structureModel_->appendRow(node.item);
            }

            visibleNodeIndexByDepth[node.entry.depth] = nodeIndex;
        }
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

    if (mapObjectsTree_ != nullptr) {
        mapObjectsTree_->expandAll();
    }
    updateMapObjectSelectionFromEditorLocation(filePath, documentCurrentLineNumberForWidget(tabWidget));
    refreshMapBackgroundPanel();
}

void MainWindow::handleStructureSelectionChanged(const QModelIndex &current, const QModelIndex &, QTreeView *)
{
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
}
