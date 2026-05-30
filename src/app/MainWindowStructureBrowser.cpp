#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "MainWindowStructureRoles.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QSignalBlocker>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QSvgRenderer>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <functional>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "../core/ProjectStructureIndex.h"

namespace
{
constexpr auto kStructureActionFocusTargetConfig = "focus-target-config";

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

QStandardItem *createIndexedItem(const QString &text,
                                 const QString &sourceFile,
                                 int lineNumber,
                                 const QString &category,
                                 const QString &name,
                                 const QString &objectId = QString())
{
    auto *item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(sourceFile, SourceFileRole);
    item->setData(lineNumber, LineNumberRole);
    item->setData(category, CategoryRole);
    item->setData(name, NameRole);
    item->setData(objectId, ObjectIdRole);
    return item;
}

QString structureBrowserItemText(const TherionStudio::ProjectStructureEntry &entry)
{
    const QString displayName = entry.category == QStringLiteral("Scraps")
            && entry.name == QStringLiteral("Unnamed Scrap")
        ? QObject::tr("Unnamed Scrap")
        : entry.name;
    if (entry.category == QStringLiteral("Surveys") && !entry.createsNamespace) {
        return QObject::tr("%1 (namespace off)").arg(displayName);
    }

    return displayName;
}

QString mapObjectItemText(const TherionStudio::ProjectStructureEntry &entry)
{
    const QString displayName = entry.category == QStringLiteral("Scraps")
            && entry.name == QStringLiteral("Unnamed Scrap")
        ? QObject::tr("Unnamed Scrap")
        : entry.name;
    if (entry.category == QStringLiteral("Scraps")) {
        return displayName;
    }

    return QStringLiteral("%1: %2").arg(structureObjectKindLabel(entry.category), displayName);
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

QString normalizedStructurePathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

QString projectIndexStructuralSignature(const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    QStringList parts;
    parts.reserve(projectIndex.entries.size() * 9
                  + projectIndex.mapScrapReferencesByMapKey.size() * 4
                  + projectIndex.diagnostics.size() * 4);

    parts.append(QStringLiteral("entries"));
    parts.append(QString::number(projectIndex.entries.size()));
    for (const TherionStudio::ProjectStructureEntry &entry : projectIndex.entries) {
        parts.append(QString::number(static_cast<int>(entry.kind)));
        parts.append(entry.objectId);
        parts.append(entry.parentObjectId);
        parts.append(entry.category);
        parts.append(entry.name);
        parts.append(normalizedStructurePathKey(entry.sourceFile));
        parts.append(QString::number(entry.depth));
        parts.append(entry.createsNamespace ? QStringLiteral("1") : QStringLiteral("0"));
    }

    QStringList mapKeys = projectIndex.mapScrapReferencesByMapKey.keys();
    std::sort(mapKeys.begin(), mapKeys.end());
    parts.append(QStringLiteral("map-scrap-refs"));
    parts.append(QString::number(mapKeys.size()));
    for (const QString &mapKey : mapKeys) {
        QStringList scrapNames = projectIndex.mapScrapReferencesByMapKey.value(mapKey).values();
        std::sort(scrapNames.begin(), scrapNames.end());
        parts.append(mapKey);
        parts.append(scrapNames.join(QLatin1Char(',')));
    }

    parts.append(QStringLiteral("diagnostics"));
    parts.append(QString::number(projectIndex.diagnostics.size()));
    for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : projectIndex.diagnostics) {
        parts.append(QString::number(static_cast<int>(diagnostic.kind)));
        parts.append(diagnostic.sourceObjectId);
        parts.append(normalizedStructurePathKey(diagnostic.sourceFile));
        parts.append(diagnostic.referencedName);
    }

    return parts.join(QChar(0x1f));
}

void updateStructureSourceLocationRoles(QStandardItemModel *model,
                                        const QString &projectRootPath,
                                        const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    if (model == nullptr) {
        return;
    }

    QHash<QString, TherionStudio::ProjectStructureEntry> entriesByObjectId;
    for (const TherionStudio::ProjectStructureEntry &entry : projectIndex.entries) {
        if (!entry.objectId.isEmpty()) {
            entriesByObjectId.insert(entry.objectId, entry);
        }
    }

    std::function<void(QStandardItem *)> visitItem = [&](QStandardItem *item) {
        if (item == nullptr) {
            return;
        }

        const QString objectId = item->data(ObjectIdRole).toString();
        const auto entryIt = entriesByObjectId.constFind(objectId);
        if (entryIt != entriesByObjectId.constEnd()) {
            item->setData(entryIt->sourceFile, SourceFileRole);
            item->setData(entryIt->lineNumber, LineNumberRole);
            if (entryIt->lineNumber > 0 && entryIt->category != QStringLiteral("File")) {
                item->setData(QStringLiteral("%1|%2|%3").arg(QDir(projectRootPath).absolutePath(),
                                                              entryIt->sourceFile)
                                                   .arg(entryIt->lineNumber),
                              OverrideKeyRole);
            }
        }

        for (int row = 0; row < item->rowCount(); ++row) {
            visitItem(item->child(row));
        }
    };

    for (int row = 0; row < model->rowCount(); ++row) {
        visitItem(model->item(row));
    }
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

    const QString preferredConfigPath = resolvedTherionTargetConfigPath();
    structureSidebarScanner_->requestScan(projectRootPath_,
                                          inMemoryProjectContentsByPath,
                                          preferredConfigPath);
}

void MainWindow::handleStructureSidebarScanFinished(const TherionStudio::ProjectStructureScanner::Result &result)
{
    if (!result.errorMessage.isEmpty()) {
        appendConsoleLine(result.errorMessage);
    }

    if (result.projectRootPath == projectRootPath_
        && !projectRootPath_.isEmpty()
        && QDir(projectRootPath_).exists()) {
        if (!result.errorMessage.isEmpty()) {
            showStructureSidebarMessage(result.errorMessage);
            return;
        }
        applyStructureSidebarIndex(result.projectIndex);
    }
}

void MainWindow::rebuildStructureSidebar()
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});
    hasAppliedStructureSidebarIndex_ = false;
    lastAppliedStructureSidebarSignature_.clear();

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

void MainWindow::applyStructureSidebarIndex(const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    const QString nextSignature = projectIndexStructuralSignature(projectIndex);
    if (hasAppliedStructureSidebarIndex_ && nextSignature == lastAppliedStructureSidebarSignature_) {
        updateStructureSidebarSourceLocations(projectIndex);
        return;
    }

    hasAppliedStructureSidebarIndex_ = true;
    lastAppliedStructureSidebarSignature_ = nextSignature;

    const QVector<TherionStudio::ProjectStructureEntry> &entries = projectIndex.entries;

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

        const QHash<QString, QSet<QString>> &mapScrapRefs = projectIndex.mapScrapReferencesByMapKey;
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
                                                entryName,
                                                entry.objectId);
            const QIcon entryIcon = structureItemIconForCategory(entry.category);
            if (!entryIcon.isNull()) {
                entryItem->setIcon(entryIcon);
            }

            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = structureOverrideKey(entry.sourceFile, entry.lineNumber);
                entryItem->setData(overrideKey, OverrideKeyRole);
            }

            VisibleStructureNode node;
            node.entry = entry;
            node.entryName = entryName;
            node.nodeKey = TherionStudio::ProjectStructureIndex::structureEntryNodeKey(entry);
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

void MainWindow::showStructureSidebarMessage(const QString &message)
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});
    hasAppliedStructureSidebarIndex_ = false;
    lastAppliedStructureSidebarSignature_.clear();

    auto *messageItem = new QStandardItem(message);
    messageItem->setEditable(false);
    messageItem->setToolTip(message);
    structureModel_->appendRow(messageItem);

    auto *actionItem = new QStandardItem(tr("Select Target Config in Compiler"));
    actionItem->setEditable(false);
    actionItem->setData(QString::fromLatin1(kStructureActionFocusTargetConfig), ActionRole);
    actionItem->setToolTip(tr("Open the Compiler pane and focus the Target Config field."));
    structureModel_->appendRow(actionItem);

    projectStructureSummary_ = message;
    structureTree_->expandAll();
}

bool MainWindow::activateStructureSidebarAction(const QString &action)
{
    if (action != QString::fromLatin1(kStructureActionFocusTargetConfig)) {
        return false;
    }

    showSidebarPane(SidebarPane::Console);
    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex = therionRunTargetCombo_->findData(QStringLiteral("project"));
        if (projectIndex >= 0) {
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }
    if (therionTargetConfigEdit_ != nullptr) {
        therionTargetConfigEdit_->setFocus(Qt::ShortcutFocusReason);
        therionTargetConfigEdit_->selectAll();
    }
    return true;
}

void MainWindow::updateStructureSidebarSourceLocations(const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    updateStructureSourceLocationRoles(structureModel_, projectRootPath_, projectIndex);
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

    if (activateStructureSidebarAction(current.data(ActionRole).toString())) {
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

    if (activateStructureSidebarAction(index.data(ActionRole).toString())) {
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
    refreshMapBackgroundPanel();
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
