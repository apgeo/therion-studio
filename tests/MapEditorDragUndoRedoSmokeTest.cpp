#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/TextEditorTab.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QIcon>
#include <QMainWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeView>
#include <QVBoxLayout>

#include <cmath>
#include <iostream>
#include <optional>

using namespace TherionStudio;

namespace
{
constexpr int kInspectorSourceLineRoleForTest = Qt::UserRole + 700;
constexpr int kInspectorObjectNameColumnForTest = 0;
constexpr int kInspectorObjectDragColumnForTest = 1;

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

bool tokenIsNumber(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    token.toDouble(&ok);
    return ok;
}

int countDirectiveLines(const QString &text, const QString &directive)
{
    if (directive.isEmpty()) {
        return 0;
    }

    const QString trimmedDirective = directive.trimmed().toLower();
    int count = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }
        if (trimmedLine == trimmedDirective || trimmedLine.startsWith(trimmedDirective + QLatin1Char(' '))) {
            ++count;
        }
    }
    return count;
}

int lastDraftLineCoordinateTokenCount(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0 || blockStart + 1 >= lines.size()) {
        return 0;
    }

    int numericTokenCount = 0;
    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (const QString &token : tokens) {
            if (tokenIsNumber(token)) {
                ++numericTokenCount;
            }
        }
    }
    return numericTokenCount;
}

bool lastDraftLineHasBezierCoordinateRow(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return false;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        int numericTokenCount = 0;
        for (const QString &token : tokens) {
            if (tokenIsNumber(token)) {
                ++numericTokenCount;
            }
        }
        if (numericTokenCount >= 6) {
            return true;
        }
    }
    return false;
}

std::optional<QVector<double>> lastDraftLineBezierCoordinateRow(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return std::nullopt;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        QVector<double> numericValues;
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (ok) {
                numericValues.append(value);
            }
        }
        if (numericValues.size() >= 6) {
            return numericValues;
        }
    }

    return std::nullopt;
}

QVector<QVector<double>> lastDraftLineNumericRows(const QString &text)
{
    QVector<QVector<double>> rows;
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return rows;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        QVector<double> numericValues;
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (ok) {
                numericValues.append(value);
            }
        }
        if (!numericValues.isEmpty()) {
            rows.append(numericValues);
        }
    }

    return rows;
}

bool lastDraftAreaReferencesLineId(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int areaStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("area water")
            || trimmed.startsWith(QStringLiteral("area water "))) {
            areaStart = index;
        }
    }
    if (areaStart < 0) {
        return false;
    }

    for (int index = areaStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.compare(QStringLiteral("endarea"), Qt::CaseInsensitive) == 0) {
            return false;
        }

        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (tokens.size() != 1) {
            return false;
        }
        const QString token = tokens.first();
        if (token.startsWith(QLatin1Char('-')) || tokenIsNumber(token)) {
            return false;
        }
        return true;
    }

    return false;
}

std::optional<QString> lastDraftAreaReferencedLineId(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int areaStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("area water")
            || trimmed.startsWith(QStringLiteral("area water "))) {
            areaStart = index;
        }
    }
    if (areaStart < 0) {
        return std::nullopt;
    }

    for (int index = areaStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.compare(QStringLiteral("endarea"), Qt::CaseInsensitive) == 0) {
            return std::nullopt;
        }

        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (tokens.size() != 1) {
            return std::nullopt;
        }
        const QString token = tokens.first();
        if (token.startsWith(QLatin1Char('-')) || tokenIsNumber(token)) {
            return std::nullopt;
        }
        return token;
    }

    return std::nullopt;
}

bool lineBlockByIdHasBezierCoordinateRow(const QString &text, const QString &lineId)
{
    if (lineId.trimmed().isEmpty()) {
        return false;
    }

    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (!trimmed.startsWith(QStringLiteral("line "), Qt::CaseInsensitive)) {
            continue;
        }
        const QString lower = trimmed.toLower();
        const QString idNeedle = QStringLiteral("-id %1").arg(lineId.toLower());
        if (lower.contains(idNeedle)) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return false;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        int numericTokenCount = 0;
        for (const QString &token : tokens) {
            if (tokenIsNumber(token)) {
                ++numericTokenCount;
            }
        }
        if (numericTokenCount >= 6) {
            return true;
        }
    }

    return false;
}

QVector<QVector<double>> lineBlockByIdNumericRows(const QString &text, const QString &lineId)
{
    QVector<QVector<double>> rows;
    if (lineId.trimmed().isEmpty()) {
        return rows;
    }

    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (!trimmed.startsWith(QStringLiteral("line "), Qt::CaseInsensitive)) {
            continue;
        }
        const QString lower = trimmed.toLower();
        const QString idNeedle = QStringLiteral("-id %1").arg(lineId.toLower());
        if (lower.contains(idNeedle)) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return rows;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        QVector<double> numericValues;
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (ok) {
                numericValues.append(value);
            }
        }
        if (!numericValues.isEmpty()) {
            rows.append(numericValues);
        }
    }

    return rows;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

MapEditableGeometryVertexItem *findCenteredLineAnchor(QGraphicsScene *scene, const QRectF &visibleSceneRect)
{
    if (scene == nullptr) {
        return nullptr;
    }

    MapEditableGeometryVertexItem *best = nullptr;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    const QPointF center = visibleSceneRect.center();
    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(rawItem);
        if (vertexItem == nullptr) {
            continue;
        }
        if (!vertexItem->isVisible()) {
            continue;
        }
        if (vertexItem->geometryKind() != QStringLiteral("line")) {
            continue;
        }
        const QPointF scenePoint = vertexItem->scenePos();
        if (!visibleSceneRect.contains(scenePoint)) {
            continue;
        }

        const qreal dx = scenePoint.x() - center.x();
        const qreal dy = scenePoint.y() - center.y();
        const qreal distance = std::sqrt((dx * dx) + (dy * dy));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = vertexItem;
        }
    }

    return best;
}

MapEditablePointItem *findPointItemForLine(QGraphicsScene *scene, int lineNumber)
{
    if (scene == nullptr || lineNumber <= 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *pointItem = dynamic_cast<MapEditablePointItem *>(rawItem);
        if (pointItem == nullptr || !pointItem->isVisible()) {
            continue;
        }
        if (pointItem->data(kMapSceneLineNumberRole).toInt() == lineNumber) {
            return pointItem;
        }
    }

    return nullptr;
}

MapEditableGeometryVertexItem *findSelectedLineVertex(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = scene->selectedItems();
    for (QGraphicsItem *rawItem : selectedItems) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(rawItem);
        if (vertexItem == nullptr) {
            continue;
        }
        if (vertexItem->geometryKind().startsWith(QStringLiteral("line"))) {
            return vertexItem;
        }
    }

    return nullptr;
}

int selectedSourceLineNumber(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return 0;
    }

    const QList<QGraphicsItem *> selectedItems = scene->selectedItems();
    for (QGraphicsItem *rawItem : selectedItems) {
        if (rawItem == nullptr || !rawItem->isVisible()) {
            continue;
        }
        const int lineNumber = rawItem->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            return lineNumber;
        }
    }

    return 0;
}

QSet<int> selectedSourceLineNumbers(QGraphicsScene *scene)
{
    QSet<int> lineNumbers;
    if (scene == nullptr) {
        return lineNumbers;
    }

    const QList<QGraphicsItem *> selectedItems = scene->selectedItems();
    for (QGraphicsItem *rawItem : selectedItems) {
        if (rawItem == nullptr) {
            continue;
        }
        const int lineNumber = rawItem->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            lineNumbers.insert(lineNumber);
        }
    }

    return lineNumbers;
}


void sendMouse(QWidget *widget,
               QEvent::Type type,
               const QPoint &localPos,
               Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    const QPoint globalPos = widget->mapToGlobal(localPos);
    QMouseEvent event(type,
                      QPointF(localPos),
                      QPointF(globalPos),
                      button,
                      buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

void sendKey(QWidget *widget, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    if (widget == nullptr) {
        return;
    }

    QKeyEvent event(type, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

bool dragItemBySceneDelta(QGraphicsView *view, QGraphicsItem *item, const QPointF &requestedSceneDelta)
{
    if (view == nullptr || item == nullptr || view->viewport() == nullptr) {
        return false;
    }

    view->centerOn(item);
    pumpEvents();

    const QRectF visibleSceneRect = view->mapToScene(view->viewport()->rect()).boundingRect();
    QPointF sceneDelta = requestedSceneDelta;
    QPointF sceneStart = item->scenePos();
    QPointF sceneEnd = sceneStart + sceneDelta;
    if (!visibleSceneRect.contains(sceneEnd)) {
        const QPointF flipped = sceneStart - sceneDelta;
        if (visibleSceneRect.contains(flipped)) {
            sceneEnd = flipped;
        } else {
            sceneEnd = visibleSceneRect.center();
        }
    }

    const QPoint start = view->mapFromScene(sceneStart);
    const QPoint end = view->mapFromScene(sceneEnd);
    if (start == end) {
        return false;
    }

    QWidget *viewport = view->viewport();
    sendMouse(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    pumpEvents();

    constexpr int steps = 6;
    for (int step = 1; step <= steps; ++step) {
        const qreal t = static_cast<qreal>(step) / static_cast<qreal>(steps);
        const QPoint intermediate(qRound(start.x() + ((end.x() - start.x()) * t)),
                                  qRound(start.y() + ((end.y() - start.y()) * t)));
        sendMouse(viewport, QEvent::MouseMove, intermediate, Qt::NoButton, Qt::LeftButton);
        pumpEvents();
    }

    sendMouse(viewport, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    return true;
}

QGraphicsPathItem *findPathItemForLine(QGraphicsScene *scene, int lineNumber, std::optional<int> selectionSubtype = std::nullopt)
{
    if (scene == nullptr || lineNumber <= 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(rawItem);
        if (pathItem == nullptr || !pathItem->isVisible()) {
            continue;
        }
        if (pathItem->data(kMapSceneLineNumberRole).toInt() != lineNumber) {
            continue;
        }
        if (selectionSubtype.has_value()
            && pathItem->data(kMapSceneSelectionSubtypeRole).toInt() != selectionSubtype.value()) {
            continue;
        }
        if (!selectionSubtype.has_value()
            && pathItem->data(kMapSceneSelectionSubtypeRole).toInt() != kMapSceneSelectionSubtypeGeneric) {
            continue;
        }
        return pathItem;
    }

    return nullptr;
}

QModelIndex findObjectTreeIndexForLine(QAbstractItemModel *model, int lineNumber, const QModelIndex &parent = QModelIndex())
{
    if (model == nullptr || lineNumber <= 0) {
        return QModelIndex();
    }

    const int rowCount = model->rowCount(parent);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = model->index(row, kInspectorObjectNameColumnForTest, parent);
        if (!childIndex.isValid()) {
            continue;
        }
        if (childIndex.data(kInspectorSourceLineRoleForTest).toInt() == lineNumber) {
            return childIndex;
        }
        if (const QModelIndex nestedIndex = findObjectTreeIndexForLine(model, lineNumber, childIndex);
            nestedIndex.isValid()) {
            return nestedIndex;
        }
    }

    return QModelIndex();
}

void revealAncestorTabs(QWidget *widget)
{
    for (QWidget *ancestor = widget != nullptr ? widget->parentWidget() : nullptr;
         ancestor != nullptr;
         ancestor = ancestor->parentWidget()) {
        auto *tabs = qobject_cast<QTabWidget *>(ancestor);
        if (tabs == nullptr) {
            continue;
        }
        for (int index = 0; index < tabs->count(); ++index) {
            QWidget *page = tabs->widget(index);
            if (page == widget || (page != nullptr && page->isAncestorOf(widget))) {
                tabs->setCurrentIndex(index);
                break;
            }
        }
    }
}

enum class InspectorObjectDropPosition
{
    BeforeTarget,
    AfterTarget
};

bool dragObjectTreeRow(QTreeView *objectsTree,
                       int sourceLineNumber,
                       int targetLineNumber,
                       InspectorObjectDropPosition position)
{
    if (objectsTree == nullptr || objectsTree->model() == nullptr || objectsTree->viewport() == nullptr) {
        std::cerr << "Objects tree, model, or viewport is unavailable for drag simulation.\n";
        return false;
    }

    revealAncestorTabs(objectsTree);
    objectsTree->expandAll();
    pumpEvents();

    const QModelIndex sourceIndex = findObjectTreeIndexForLine(objectsTree->model(), sourceLineNumber);
    const QModelIndex targetIndex = findObjectTreeIndexForLine(objectsTree->model(), targetLineNumber);
    if (!sourceIndex.isValid() || !targetIndex.isValid()) {
        std::cerr << "Objects tree drag source/target line was not found: source valid="
                  << sourceIndex.isValid() << ", target valid=" << targetIndex.isValid() << '\n';
        return false;
    }

    const QModelIndex sourceDragIndex = sourceIndex.sibling(sourceIndex.row(), kInspectorObjectDragColumnForTest);
    if (!sourceDragIndex.isValid()
        || sourceDragIndex.data(Qt::DecorationRole).value<QIcon>().isNull()
        || sourceDragIndex.data(Qt::ToolTipRole).toString().isEmpty()) {
        std::cerr << "Objects tree movable rows should expose a visible drag handle icon and tooltip.\n";
        return false;
    }

    objectsTree->scrollTo(sourceIndex, QAbstractItemView::EnsureVisible);
    objectsTree->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    pumpEvents();

    const QRect sourceRect = objectsTree->visualRect(sourceDragIndex);
    const QRect targetRect = objectsTree->visualRect(targetIndex);
    if (!sourceRect.isValid() || !targetRect.isValid()) {
        std::cerr << "Objects tree drag source/target visual rect was invalid: source="
                  << sourceRect.isValid() << ", target=" << targetRect.isValid()
                  << ", tree visible=" << objectsTree->isVisible()
                  << ", viewport visible=" << objectsTree->viewport()->isVisible() << '\n';
        return false;
    }

    const QPoint sourcePoint = sourceRect.center();
    sendMouse(objectsTree->viewport(), QEvent::MouseMove, sourcePoint, Qt::NoButton, Qt::NoButton);
    pumpEvents();
    if (!objectsTree->hasMouseTracking()
        || !objectsTree->viewport()->hasMouseTracking()
        || objectsTree->viewport()->cursor().shape() != Qt::OpenHandCursor) {
        std::cerr << "Objects tree movable row hover should enable mouse tracking and use an open-hand cursor.\n";
        return false;
    }

    const QPoint targetPoint(targetRect.center().x(),
                             position == InspectorObjectDropPosition::AfterTarget
                                 ? targetRect.bottom() - 1
                                 : targetRect.top() + 1);
    sendMouse(objectsTree->viewport(), QEvent::MouseButtonPress, sourcePoint, Qt::LeftButton, Qt::LeftButton);
    pumpEvents();
    sendMouse(objectsTree->viewport(), QEvent::MouseMove, targetPoint, Qt::NoButton, Qt::LeftButton);
    pumpEvents();
    auto *dropIndicator = objectsTree->viewport()->findChild<QWidget *>(QStringLiteral("mapObjectsTreeDropIndicator"));
    if (dropIndicator == nullptr || !dropIndicator->isVisible()) {
        std::cerr << "Objects tree drag should show a visible drop indicator before release.\n";
        return false;
    }
    const QRect indicatorRect = dropIndicator->geometry();
    const int expectedDropY = position == InspectorObjectDropPosition::AfterTarget
        ? targetRect.bottom()
        : targetRect.top();
    if (objectsTree->viewport()->cursor().shape() != Qt::ClosedHandCursor) {
        std::cerr << "Objects tree drag should use a closed-hand cursor while moving a row.\n";
        return false;
    }
    if (indicatorRect.height() != 3
        || indicatorRect.width() < targetRect.width()
        || qAbs(indicatorRect.center().y() - expectedDropY) > 3) {
        std::cerr << "Objects tree drop indicator should be a slim line at the target edge; geometry="
                  << indicatorRect.x() << ',' << indicatorRect.y() << ' '
                  << indicatorRect.width() << 'x' << indicatorRect.height()
                  << ", expected target y=" << expectedDropY << '\n';
        return false;
    }
    sendMouse(objectsTree->viewport(), QEvent::MouseButtonRelease, targetPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (dropIndicator->isVisible()) {
        std::cerr << "Objects tree drag drop indicator should be hidden after release.\n";
        return false;
    }
    return true;
}

int runInspectorObjectMoveScenario(const char *scenarioName,
                                   const QByteArray &th2Contents,
                                   int sourceLineNumber,
                                   int targetLineNumber,
                                   InspectorObjectDropPosition position,
                                   int expectedCurrentLineNumber,
                                   const QString &expectedMovedText)
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for object move smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("object_move_%1.th2").arg(QString::fromLatin1(scenarioName)));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for object move smoke test.")) {
        return 1;
    }

    file.write(th2Contents);
    file.close();

    QMainWindow hostWindow;
    hostWindow.resize(900, 700);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for object move smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *objectsTree = mapTab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    if (!expect(objectsTree != nullptr && objectsTree->model() != nullptr,
                "Objects inspector tree was not initialized for object move smoke test.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    if (!expect(dragObjectTreeRow(objectsTree, sourceLineNumber, targetLineNumber, position),
                "Dragging an object row onto another object row should be routed through the inspector move handler.")) {
        return 1;
    }

    const QString movedText = mapTab->text();
    if (!expect(movedText == expectedMovedText,
                "Dragging an object row in Objects inspector should rewrite the TH2 source order.")) {
        std::cerr << "Scenario: " << scenarioName << '\n';
        std::cerr << "Actual moved source:\n" << movedText.toStdString() << '\n';
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == expectedCurrentLineNumber,
                "After object-row move, the text cursor should reveal the moved object at its new source line.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(), "Object-row move should be undoable.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == originalText,
                "Undo after object-row move should restore the original source order.")) {
        return 1;
    }
    if (!expect(!mapTab->canUndo(),
                "Undo after object-row move should not leave a duplicate text-editor undo command.")) {
        return 1;
    }
    if (!expect(mapTab->canRedo(), "Object-row move should be redoable after undo.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == expectedMovedText,
                "Redo after object-row move should restore the moved source order.")) {
        return 1;
    }
    if (!expect(!mapTab->canRedo(),
                "Redo after object-row move should consume the map redo command.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}

int runInspectorObjectMoveSmoke()
{
    const QByteArray afterLineContents =
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "point 1 2 station -name P1\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 3 4 station -name P2\n"
        "endscrap\n";
    const QString afterLineExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "endscrap\n");
    if (const int rc = runInspectorObjectMoveScenario("after_line",
                                                      afterLineContents,
                                                      3,
                                                      4,
                                                      InspectorObjectDropPosition::AfterTarget,
                                                      7,
                                                      afterLineExpected);
        rc != 0) {
        return rc;
    }

    const QByteArray beforePointContents =
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "endscrap\n";
    const QString beforePointExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "endscrap\n");
    if (const int rc = runInspectorObjectMoveScenario("before_point",
                                                      beforePointContents,
                                                      5,
                                                      3,
                                                      InspectorObjectDropPosition::BeforeTarget,
                                                      3,
                                                      beforePointExpected);
        rc != 0) {
        return rc;
    }

    const QByteArray betweenScrapContents =
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "endscrap\n";
    const QString betweenScrapExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "point 5 6 label -text target\n"
        "endscrap\n");
    if (const int rc = runInspectorObjectMoveScenario("between_scraps",
                                                      betweenScrapContents,
                                                      3,
                                                      8,
                                                      InspectorObjectDropPosition::BeforeTarget,
                                                      5,
                                                      betweenScrapExpected);
        rc != 0) {
        return rc;
    }

    const QByteArray intoScrapContents =
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "point 1 2 station -name P1\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "endscrap\n";
    const QString intoScrapExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "point 1 2 station -name P1\n"
        "endscrap\n");
    return runInspectorObjectMoveScenario("into_scrap",
                                          intoScrapContents,
                                          3,
                                          5,
                                          InspectorObjectDropPosition::AfterTarget,
                                          6,
                                          intoScrapExpected);
}

int runAreaBorderHitSelectionSmoke()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for area hit-selection smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("area_hit_selection_smoke.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for area hit-selection smoke test.")) {
        return 1;
    }

    const QByteArray th2Contents =
        "encoding utf-8\n"
        "\n"
        "scrap area-hit -projection plan\n"
        "line wall -id border\n"
        "  0 0\n"
        "  100 0\n"
        "  100 -100\n"
        "  0 -100\n"
        "  0 0\n"
        "endline\n"
        "area water\n"
        "  border\n"
        "endarea\n"
        "endscrap\n";
    file.write(th2Contents);
    file.close();

    QMainWindow hostWindow;
    hostWindow.resize(900, 700);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load referenced-area TH2 file for hit-selection smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();
    mapTab->triggerSelectMode();
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr && mapView->scene() != nullptr,
                "Map view/scene was not initialized for area hit-selection smoke test.")) {
        return 1;
    }
    auto *objectsTree = mapTab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    if (!expect(objectsTree != nullptr && objectsTree->selectionModel() != nullptr,
                "Objects inspector tree was not initialized for area hit-selection smoke test.")) {
        return 1;
    }

    auto *linePathItem = findPathItemForLine(mapView->scene(), 4);
    auto *areaFillItem = findPathItemForLine(mapView->scene(), 11, kMapSceneSelectionSubtypeAreaFill);
    if (!expect(linePathItem != nullptr, "Referenced area border line path was not found in the map scene.")) {
        return 1;
    }
    if (!expect(areaFillItem != nullptr, "Referenced area fill path was not found in the map scene.")) {
        return 1;
    }

    mapView->centerOn(areaFillItem);
    pumpEvents();

    const QPointF borderScenePoint = linePathItem->mapToScene(linePathItem->path().pointAtPercent(0.25));
    const QPoint borderViewportPoint = mapView->mapFromScene(borderScenePoint);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, borderViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, borderViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({4}),
                "Clicking a referenced area border should select the owning line object.")) {
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == 4,
                "Clicking a referenced area border should move the text cursor to the line directive.")) {
        return 1;
    }
    if (!expect(!objectsTree->selectionModel()->selectedRows().isEmpty(),
                "Map-object selection should select the corresponding row in the Objects inspector.")) {
        return 1;
    }

    const QPointF fillScenePoint = areaFillItem->mapToScene(areaFillItem->path().boundingRect().center());
    const QPoint fillViewportPoint = mapView->mapFromScene(fillScenePoint);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, fillViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, fillViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({11}),
                "Clicking inside a referenced area fill should select the area object.")) {
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == 11,
                "Clicking inside a referenced area fill should move the text cursor to the area directive.")) {
        return 1;
    }
    if (!expect(!objectsTree->selectionModel()->selectedRows().isEmpty(),
                "Area fill selection should select the corresponding row in the Objects inspector.")) {
        return 1;
    }

    const QPoint emptyViewportPoint = mapView->mapFromScene(QPointF(36.0, 36.0));
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, emptyViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, emptyViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(mapView->scene()->selectedItems().isEmpty(),
                "Clicking empty map-canvas space should clear the graphical selection.")) {
        return 1;
    }
    if (!expect(objectsTree->selectionModel()->selectedRows().isEmpty(),
                "Clicking empty map-canvas space should clear the Objects inspector selection.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}

int runDragUndoRedoSmoke()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("drag_undo_redo_smoke.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for smoke test.")) {
        return 1;
    }

    const QByteArray th2Contents =
        "encoding utf-8\n"
        "\n"
        "scrap smoke -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  100 0\n"
        "  100 -100\n"
        "  smooth off\n"
        "  subtype presumed\n"
        "  altitude 12\n"
        "endline\n"
        "area water\n"
        "  0 0\n"
        "  100 0 100 -100\n"
        "endarea\n"
        "\n"
        "point 200 -50 station -name P1\n"
        "endscrap\n";
    file.write(th2Contents);
    file.close();

    QMainWindow hostWindow;
    hostWindow.resize(1280, 900);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load temporary TH2 file for smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr, "Map view was not found in MapEditorTab.")) {
        return 1;
    }
    if (!expect(mapView->scene() != nullptr, "Map scene was not initialized.")) {
        return 1;
    }
    auto *textEditor = mapTab->findChild<TextEditorTab *>();
    if (!expect(textEditor != nullptr, "Text editor was not found in MapEditorTab.")) {
        return 1;
    }

    const QRectF visibleSceneRect = mapView->mapToScene(mapView->viewport()->rect()).boundingRect();
    mapTab->goToLine(4);
    pumpEvents();
    if (!expect(mapTab->currentLineNumber() == 4, "Expected map tab current line to be the line object start.")) {
        return 1;
    }
    textEditor->goToLineColumn(3, 3);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({4, 12, 17}),
                "Moving text cursor to scrap should select all map objects inside the scrap block.")) {
        return 1;
    }
    textEditor->goToLineColumn(18, 3);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({4, 12, 17}),
                "Moving text cursor to endscrap should select all map objects inside the scrap block.")) {
        return 1;
    }
    mapTab->goToLine(4);
    pumpEvents();

    auto *anchorItem = findCenteredLineAnchor(mapView->scene(), visibleSceneRect);
    if (!expect(anchorItem != nullptr, "No visible editable line anchor was found after selecting line geometry.")) {
        return 1;
    }
    anchorItem->setSelected(true);
    pumpEvents();
    const int anchorSelectedLine = mapTab->currentLineNumber();
    if (!expect(anchorSelectedLine >= 5 && anchorSelectedLine <= 7,
                "Selecting map vertex should move text editor cursor to that vertex coordinate row.")) {
        return 1;
    }

    textEditor->goToLineColumn(8, 3);
    pumpEvents();
    auto *selectedVertexFromSmooth = findSelectedLineVertex(mapView->scene());
    if (!expect(selectedVertexFromSmooth != nullptr,
                "Moving text cursor to a smooth-option row should select the corresponding map vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromSmooth->lineNumber() == 4 && selectedVertexFromSmooth->vertexIndex() == 2,
                "Text-to-map sync should map smooth-option row to the current line vertex.")) {
        return 1;
    }
    textEditor->goToLineColumn(9, 3);
    pumpEvents();
    auto *selectedVertexFromSubtype = findSelectedLineVertex(mapView->scene());
    if (!expect(selectedVertexFromSubtype != nullptr,
                "Moving text cursor to subtype-option row should select the current line vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromSubtype->lineNumber() == 4 && selectedVertexFromSubtype->vertexIndex() == 2,
                "Text-to-map sync should map subtype-option row to the current line vertex.")) {
        return 1;
    }
    textEditor->goToLineColumn(10, 3);
    pumpEvents();
    auto *selectedVertexFromGenericOption = findSelectedLineVertex(mapView->scene());
    if (!expect(selectedVertexFromGenericOption != nullptr,
                "Moving text cursor to an arbitrary line option row should select the current line vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromGenericOption->lineNumber() == 4 && selectedVertexFromGenericOption->vertexIndex() == 2,
                "Text-to-map sync should map arbitrary line option rows to the current line vertex.")) {
        return 1;
    }
    textEditor->goToLineColumn(11, 3);
    pumpEvents();
    if (!expect(selectedSourceLineNumber(mapView->scene()) == 4,
                "Moving text cursor to endline should keep selection on the owning line block start.")) {
        return 1;
    }
    textEditor->goToLineColumn(15, 3);
    pumpEvents();
    if (!expect(selectedSourceLineNumber(mapView->scene()) == 12,
                "Moving text cursor to endarea should keep selection on the owning area block start.")) {
        return 1;
    }

    auto *pointItem = findPointItemForLine(mapView->scene(), 17);
    if (!expect(pointItem != nullptr, "Failed to find map point geometry item for source line 17.")) {
        return 1;
    }
    mapView->scene()->clearSelection();
    pointItem->setData(kMapSceneLineNumberRole, 16);
    pointItem->setSelected(true);
    pumpEvents();
    if (!expect(mapTab->currentLineNumber() == 17,
                "Selecting map point geometry should resolve the coordinate row, not the blank line above it.")) {
        return 1;
    }
    mapTab->goToLine(4);
    pumpEvents();
    textEditor->goToLineColumn(7, 3);
    pumpEvents();
    auto *selectedVertexFromText = findSelectedLineVertex(mapView->scene());
    if (!expect(selectedVertexFromText != nullptr,
                "Moving text cursor to a vertex row should select the corresponding map vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromText->lineNumber() == 4 && selectedVertexFromText->vertexIndex() == 2,
                "Text-to-map vertex sync should select line vertex index 2 for source row line 7.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    if (!expect(!originalText.isEmpty(), "Loaded TH2 text should not be empty.")) {
        return 1;
    }

    const QRectF dragVisibleSceneRect = mapView->mapToScene(mapView->viewport()->rect()).boundingRect();
    auto *dragAnchorItem = findCenteredLineAnchor(mapView->scene(), dragVisibleSceneRect);
    if (!expect(dragAnchorItem != nullptr, "No visible editable line anchor was found before drag edit.")) {
        return 1;
    }
    dragAnchorItem->setSelected(true);
    pumpEvents();
    mapTab->triggerSelectMode();
    pumpEvents();

    if (!expect(dragItemBySceneDelta(mapView, dragAnchorItem, QPointF(36.0, -24.0)),
                "Failed to drag editable map anchor during smoke test.")) {
        return 1;
    }

    const QString changedText = mapTab->text();
    if (!expect(changedText != originalText, "Drag did not change TH2 text content.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(), "Undo should be enabled after a map drag edit.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();

    if (!expect(mapTab->text() == originalText, "Undo did not restore original TH2 text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Map tab should be clean after undo to baseline.")) {
        return 1;
    }
    if (!expect(mapTab->canRedo(), "Redo should be enabled after undoing map drag edit.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();

    if (!expect(mapTab->text() == changedText, "Redo did not restore edited TH2 text.")) {
        return 1;
    }
    if (!expect(mapTab->isDirty(), "Map tab should be dirty after redo reapplies edit.")) {
        return 1;
    }

    const QString textBeforeInteractiveDrawing = mapTab->text();
    const int pointDirectivesBefore = countDirectiveLines(textBeforeInteractiveDrawing, QStringLiteral("point"));
    const int lineDirectivesBefore = countDirectiveLines(textBeforeInteractiveDrawing, QStringLiteral("line"));
    const int areaDirectivesBefore = countDirectiveLines(textBeforeInteractiveDrawing, QStringLiteral("area"));
    const QPoint viewportCenter = mapView->viewport()->rect().center();
    const QString textBeforePointInsert = mapTab->text();
    mapTab->triggerAddPoint();
    pumpEvents();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, viewportCenter, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, viewportCenter, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("point")) == pointDirectivesBefore + 1,
                "Point mode click-to-place should insert one new point directive.")) {
        return 1;
    }
    const QString textAfterPointInsert = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforePointInsert,
                "Undo after Point mode insertion should remove the newly inserted point directive.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterPointInsert,
                "Redo after undoing Point mode insertion should restore the point directive.")) {
        return 1;
    }
    const QPoint secondPointPosition(viewportCenter.x() + 18, viewportCenter.y() + 12);
    const QString textAfterFirstPointInsert = mapTab->text();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondPointPosition, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondPointPosition, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    const QString textAfterSecondPointInsert = mapTab->text();
    if (!expect(countDirectiveLines(textAfterSecondPointInsert, QStringLiteral("point")) == pointDirectivesBefore + 2,
                "A second Point mode click should insert a second point directive.")) {
        return 1;
    }
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterFirstPointInsert,
                "First Undo after two Point mode insertions should remove only the second point directive.")) {
        return 1;
    }
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforePointInsert,
                "Second Undo after two Point mode insertions should remove the first point directive too.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterFirstPointInsert,
                "First Redo after two point undos should restore the first point directive.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterSecondPointInsert,
                "Second Redo after two point undos should restore the second point directive.")) {
        return 1;
    }

    const int pointDirectivesBeforeEscCancel = countDirectiveLines(mapTab->text(), QStringLiteral("point"));
    mapTab->triggerAddPoint();
    pumpEvents();
    textEditor->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(textEditor, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(textEditor, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, viewportCenter, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, viewportCenter, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("point")) == pointDirectivesBeforeEscCancel,
                "Esc in Point mode should cancel insert mode and return to Select mode.")) {
        return 1;
    }

    mapTab->triggerAddLine();
    pumpEvents();
    const QString textBeforeLineInsert = mapTab->text();
    const QPoint firstLineVertex(viewportCenter.x() - 20, viewportCenter.y() - 12);
    const QPoint secondLineVertex(viewportCenter.x() + 24, viewportCenter.y() - 8);
    const QPoint thirdLineVertex(viewportCenter.x() + 18, viewportCenter.y() + 18);
    const QPoint fourthLineVertex(viewportCenter.x() - 14, viewportCenter.y() + 22);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, firstLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, firstLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, thirdLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, thirdLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, fourthLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, fourthLineVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapView->setFocus(Qt::OtherFocusReason);
    sendKey(mapView->viewport(), QEvent::KeyPress, Qt::Key_Return);
    sendKey(mapView->viewport(), QEvent::KeyRelease, Qt::Key_Return);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBefore + 1,
                "Line mode click-to-add + Enter should insert one new line directive.")) {
        return 1;
    }
    if (!expect(lastDraftLineCoordinateTokenCount(mapTab->text()) >= 8,
                "Committing a 4-vertex line draft should preserve all captured vertices in source text.")) {
        return 1;
    }
    const QString textAfterLineInsert = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeLineInsert,
                "Undo after Line mode completion should remove the newly inserted line block.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterLineInsert,
                "Redo after undoing Line mode completion should restore the line block.")) {
        return 1;
    }

    const int lineDirectivesBeforeModePersistenceCheck = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    const QPoint fifthLineVertex(viewportCenter.x() - 14, viewportCenter.y() + 26);
    const QPoint sixthLineVertex(viewportCenter.x() + 28, viewportCenter.y() + 30);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, fifthLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, fifthLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, sixthLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, sixthLineVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeModePersistenceCheck + 1,
                "After Enter commit, Line mode should stay active so a new line can be inserted without reselecting Line mode.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint bezierAnchor1(viewportCenter.x() - 60, viewportCenter.y() - 30);
    const QPoint bezierAnchor2(viewportCenter.x() + 60, viewportCenter.y() - 22);
    const QPoint bezierDrag(viewportCenter.x() + 60, viewportCenter.y() + 24);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, bezierAnchor1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, bezierAnchor1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, bezierAnchor2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, bezierDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, bezierDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierInsert + 1,
                "Line mode click+drag should insert a new line directive.")) {
        return 1;
    }
    if (!expect(lastDraftLineHasBezierCoordinateRow(mapTab->text()),
                "Line mode click+drag should create a bezier coordinate row with control points.")) {
        return 1;
    }
    const auto baselineBezierRow = lastDraftLineBezierCoordinateRow(mapTab->text());
    if (!expect(baselineBezierRow.has_value(),
                "Bezier insert should expose a parseable coordinate row with control-point values.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierAdjustInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint adjustedBezierAnchor1(viewportCenter.x() - 62, viewportCenter.y() + 42);
    const QPoint adjustedBezierAnchor2(viewportCenter.x() + 58, viewportCenter.y() + 50);
    const QPoint adjustedBezierDrag(viewportCenter.x() + 58, viewportCenter.y() + 98);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, adjustedBezierAnchor1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, adjustedBezierAnchor1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, adjustedBezierAnchor2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, adjustedBezierDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, adjustedBezierDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    const QPointF anchor1Scene = mapView->mapToScene(adjustedBezierAnchor1);
    const QPointF dragScene = mapView->mapToScene(adjustedBezierDrag);
    const QPointF initialControlScene = anchor1Scene + ((dragScene - anchor1Scene) * (2.0 / 3.0));
    const QPoint controlPress = mapView->mapFromScene(initialControlScene);
    const QPoint controlDrag = mapView->mapFromScene(initialControlScene + QPointF(48.0, -30.0));
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, controlPress, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, controlDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, controlDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierAdjustInsert + 1,
                "Dragging a bezier control handle in Line mode should still commit as a new line.")) {
        return 1;
    }
    const auto adjustedBezierRow = lastDraftLineBezierCoordinateRow(mapTab->text());
    if (!expect(adjustedBezierRow.has_value(),
                "Adjusted bezier insert should keep a parseable coordinate row with control-point values.")) {
        return 1;
    }
    if (!expect(std::abs(adjustedBezierRow->first() - baselineBezierRow->first()) > 0.001
                || std::abs(adjustedBezierRow->at(1) - baselineBezierRow->at(1)) > 0.001,
                "Dragging a bezier control handle should change serialized control-point coordinates.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierAnchorMirrorInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint anchorMirror1(viewportCenter.x() - 88, viewportCenter.y() - 28);
    const QPoint anchorMirror2(viewportCenter.x() - 18, viewportCenter.y() - 14);
    const QPoint anchorMirrorDrag2(viewportCenter.x() - 18, viewportCenter.y() + 34);
    const QPoint anchorMirror3(viewportCenter.x() + 74, viewportCenter.y() - 40);
    const QPoint anchorMirrorDrag3(viewportCenter.x() + 74, viewportCenter.y() - 90);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, anchorMirror1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, anchorMirror1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, anchorMirror2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, anchorMirrorDrag2, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, anchorMirrorDrag2, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, anchorMirror3, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, anchorMirrorDrag3, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, anchorMirrorDrag3, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierAnchorMirrorInsert + 1,
                "Placing consecutive curved anchors should still commit as a new line.")) {
        return 1;
    }
    const QVector<QVector<double>> anchorMirrorRows = lastDraftLineNumericRows(mapTab->text());
    if (!expect(anchorMirrorRows.size() >= 3 && anchorMirrorRows.at(1).size() >= 6 && anchorMirrorRows.at(2).size() >= 6,
                "Consecutive curved-anchor draft should serialize two cubic coordinate rows.")) {
        return 1;
    }
    const QPointF anchorMirrorMiddleAnchorSource(anchorMirrorRows.at(1).at(4), anchorMirrorRows.at(1).at(5));
    const QPointF anchorMirrorMiddleIncomingSource(anchorMirrorRows.at(1).at(2), anchorMirrorRows.at(1).at(3));
    const QPointF anchorMirrorMiddleOutgoingSource(anchorMirrorRows.at(2).at(0), anchorMirrorRows.at(2).at(1));
    const QPointF anchorMirrorIncomingVector = anchorMirrorMiddleIncomingSource - anchorMirrorMiddleAnchorSource;
    const QPointF anchorMirrorOutgoingVector = anchorMirrorMiddleOutgoingSource - anchorMirrorMiddleAnchorSource;
    const qreal anchorMirrorIncomingLength = std::hypot(anchorMirrorIncomingVector.x(), anchorMirrorIncomingVector.y());
    const qreal anchorMirrorOutgoingLength = std::hypot(anchorMirrorOutgoingVector.x(), anchorMirrorOutgoingVector.y());
    const qreal anchorMirrorCross = anchorMirrorIncomingVector.x() * anchorMirrorOutgoingVector.y()
        - anchorMirrorIncomingVector.y() * anchorMirrorOutgoingVector.x();
    const qreal anchorMirrorDot = anchorMirrorIncomingVector.x() * anchorMirrorOutgoingVector.x()
        + anchorMirrorIncomingVector.y() * anchorMirrorOutgoingVector.y();
    if (!expect(anchorMirrorIncomingLength > 0.01 && anchorMirrorOutgoingLength > 0.01,
                "Consecutive curved anchors should keep non-zero incoming/outgoing control vectors on middle vertex.")) {
        return 1;
    }
    const qreal anchorMirrorNormalizedCross = std::abs(anchorMirrorCross / (anchorMirrorIncomingLength * anchorMirrorOutgoingLength));
    const qreal anchorMirrorNormalizedDot = anchorMirrorDot / (anchorMirrorIncomingLength * anchorMirrorOutgoingLength);
    if (!expect(anchorMirrorNormalizedCross < 0.08 && anchorMirrorNormalizedDot < -0.9,
                "Consecutive curved anchors should keep middle draft controls collinear/opposed.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierMirrorInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint mirroredAnchor1(viewportCenter.x() - 70, viewportCenter.y() + 58);
    const QPoint mirroredAnchor2(viewportCenter.x() - 8, viewportCenter.y() + 46);
    const QPoint mirroredDrag2(viewportCenter.x() - 8, viewportCenter.y() + 92);
    const QPoint mirroredAnchor3(viewportCenter.x() + 66, viewportCenter.y() + 54);
    const QPoint mirroredDrag3(viewportCenter.x() + 66, viewportCenter.y() + 8);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredAnchor1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredAnchor1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredAnchor2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, mirroredDrag2, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredDrag2, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredAnchor3, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, mirroredDrag3, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredDrag3, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    const QPointF mirroredAnchor2Scene = mapView->mapToScene(mirroredAnchor2);
    const QPointF mirroredDrag3Scene = mapView->mapToScene(mirroredDrag3);
    const QPointF middleOutgoingControlScene
        = mirroredAnchor2Scene + ((mirroredDrag3Scene - mirroredAnchor2Scene) * (2.0 / 3.0));
    const QPoint mirroredControlPress = mapView->mapFromScene(middleOutgoingControlScene);
    const QPoint mirroredControlDrag = mapView->mapFromScene(middleOutgoingControlScene + QPointF(52.0, -24.0));
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredControlPress, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, mirroredControlDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredControlDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierMirrorInsert + 1,
                "Dragging one control on a smooth draft vertex should still commit as a new line.")) {
        return 1;
    }
    const QVector<QVector<double>> mirroredRows = lastDraftLineNumericRows(mapTab->text());
    if (!expect(mirroredRows.size() >= 3 && mirroredRows.at(1).size() >= 6 && mirroredRows.at(2).size() >= 6,
                "Three-anchor bezier draft should serialize two cubic coordinate rows.")) {
        return 1;
    }
    const QPointF middleAnchorSource(mirroredRows.at(1).at(4), mirroredRows.at(1).at(5));
    const QPointF middleIncomingSource(mirroredRows.at(1).at(2), mirroredRows.at(1).at(3));
    const QPointF middleOutgoingSource(mirroredRows.at(2).at(0), mirroredRows.at(2).at(1));
    const QPointF incomingVector = middleIncomingSource - middleAnchorSource;
    const QPointF outgoingVector = middleOutgoingSource - middleAnchorSource;
    const qreal incomingLength = std::hypot(incomingVector.x(), incomingVector.y());
    const qreal outgoingLength = std::hypot(outgoingVector.x(), outgoingVector.y());
    const qreal cross = incomingVector.x() * outgoingVector.y() - incomingVector.y() * outgoingVector.x();
    const qreal dot = incomingVector.x() * outgoingVector.x() + incomingVector.y() * outgoingVector.y();
    if (!expect(incomingLength > 0.01 && outgoingLength > 0.01,
                "Middle draft vertex should keep non-zero incoming/outgoing control vectors.")) {
        return 1;
    }
    const qreal normalizedCross = std::abs(cross / (incomingLength * outgoingLength));
    const qreal normalizedDot = dot / (incomingLength * outgoingLength);
    if (!expect(normalizedCross < 0.05 && normalizedDot < -0.95,
                "Dragging one control on a smooth draft vertex should mirror-adapt the opposite control.")) {
        return 1;
    }

    const int lineDirectivesBeforeEscCancel = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, firstLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, firstLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondLineVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    textEditor->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(textEditor, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(textEditor, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeEscCancel + 1,
                "Esc in Line mode should commit captured vertices and return to Select mode.")) {
        return 1;
    }

    const int areaDirectivesBeforeEscCommit = countDirectiveLines(mapTab->text(), QStringLiteral("area"));
    const int lineDirectivesBeforeAreaEscCommit = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddArea();
    pumpEvents();
    const QPoint firstAreaVertex(viewportCenter.x() - 22, viewportCenter.y() + 18);
    const QPoint secondAreaVertex(viewportCenter.x() + 14, viewportCenter.y() + 18);
    const QPoint secondAreaDrag(viewportCenter.x() + 14, viewportCenter.y() + 48);
    const QPoint thirdAreaVertex(viewportCenter.x() - 4, viewportCenter.y() - 14);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, firstAreaVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, firstAreaVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondAreaVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, secondAreaDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondAreaDrag, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, thirdAreaVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, thirdAreaVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    textEditor->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(textEditor, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(textEditor, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("area")) == areaDirectivesBeforeEscCommit + 1,
                "Esc in Area mode should commit captured vertices and return to Select mode.")) {
        return 1;
    }
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeAreaEscCommit + 1,
                "Area commit should create a closed border line referenced by the area block.")) {
        return 1;
    }
    if (!expect(lastDraftAreaReferencesLineId(mapTab->text()),
                "Committed area should reference a border line id inside the area block.")) {
        return 1;
    }
    const auto areaBorderId = lastDraftAreaReferencedLineId(mapTab->text());
    if (!expect(areaBorderId.has_value() && lineBlockByIdHasBezierCoordinateRow(mapTab->text(), areaBorderId.value()),
                "Area click+drag draft should serialize bezier coordinate rows in the referenced border line.")) {
        return 1;
    }
    const QVector<QVector<double>> areaBorderRows = areaBorderId.has_value()
        ? lineBlockByIdNumericRows(mapTab->text(), areaBorderId.value())
        : QVector<QVector<double>>();
    if (!expect(areaBorderRows.size() >= 4
                && areaBorderRows.first().size() >= 2
                && areaBorderRows.last().size() >= 6,
                "Bezier area close should append an explicit closing cubic row in the referenced border line.")) {
        return 1;
    }
    const QVector<double> &areaFirstRow = areaBorderRows.first();
    const QVector<double> &areaLastRow = areaBorderRows.last();
    if (!expect(std::abs(areaLastRow.at(areaLastRow.size() - 2) - areaFirstRow.at(0)) < 0.01
                && std::abs(areaLastRow.at(areaLastRow.size() - 1) - areaFirstRow.at(1)) < 0.01,
                "Bezier area closing row should terminate at the first area anchor.")) {
        return 1;
    }

    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("area")) >= areaDirectivesBefore + 1,
                "Interactive area workflows should produce at least one additional area directive.")) {
        return 1;
    }

    const int lineDirectivesBeforeFreehand = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddFreehandLine();
    pumpEvents();
    const QPoint strokeStart(viewportCenter.x() - 30, viewportCenter.y() + 20);
    const QPoint strokeMid1(viewportCenter.x() - 10, viewportCenter.y() + 8);
    const QPoint strokeMid2(viewportCenter.x() + 16, viewportCenter.y() - 4);
    const QPoint strokeEnd(viewportCenter.x() + 34, viewportCenter.y() - 14);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, strokeStart, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, strokeMid1, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, strokeMid2, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, strokeEnd, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeFreehand + 1,
                "Freehand drag-and-release should insert one new line directive.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int rc = runInspectorObjectMoveSmoke(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaBorderHitSelectionSmoke(); rc != 0) {
        return rc;
    }
    return runDragUndoRedoSmoke();
}
