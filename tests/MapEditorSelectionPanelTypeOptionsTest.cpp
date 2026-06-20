#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QThread>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <QFile>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void pumpEventsFor(int milliseconds)
{
    const int iterations = qMax(1, milliseconds / 10);
    for (int index = 0; index < iterations; ++index) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void sendKey(QWidget *widget, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    if (widget == nullptr) {
        return;
    }

    QKeyEvent event(type, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

void sendMouseClick(QWidget *widget, const QPoint &position)
{
    if (widget == nullptr) {
        return;
    }

    const QPoint globalPosition = widget->mapToGlobal(position);
    QMouseEvent pressEvent(QEvent::MouseButtonPress,
                           position,
                           globalPosition,
                           Qt::LeftButton,
                           Qt::LeftButton,
                           Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &pressEvent);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                             position,
                             globalPosition,
                             Qt::LeftButton,
                             Qt::NoButton,
                             Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &releaseEvent);
}

void commitComboEdit(QComboBox *combo, const QString &text)
{
    if (combo == nullptr) {
        return;
    }

    combo->setCurrentText(text);
    if (QLineEdit *lineEdit = combo->lineEdit(); lineEdit != nullptr) {
        QMetaObject::invokeMethod(lineEdit, "editingFinished", Qt::DirectConnection);
    }
}

bool visibleNoSelectionLabel(QWidget *root)
{
    if (root == nullptr) {
        return false;
    }

    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label != nullptr
            && label->isVisible()
            && label->text() == QStringLiteral("No map object selected.")) {
            return true;
        }
    }
    return false;
}

bool visibleLabelStartingWith(QWidget *root, const QString &prefix)
{
    if (root == nullptr) {
        return false;
    }

    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label != nullptr
            && label->isVisible()
            && label->text().startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

QLabel *visibleLabelContaining(QWidget *root, const QString &fragment)
{
    if (root == nullptr) {
        return nullptr;
    }

    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label != nullptr
            && label->isVisible()
            && label->text().contains(fragment)) {
            return label;
        }
    }
    return nullptr;
}

int visibleLabelCount(QWidget *root, const QString &text)
{
    if (root == nullptr) {
        return 0;
    }

    int count = 0;
    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label != nullptr && label->isVisible() && label->text() == text) {
            ++count;
        }
    }
    return count;
}

bool isVisibleWithin(QWidget *root, const QString &objectName)
{
    auto *widget = root != nullptr ? root->findChild<QWidget *>(objectName) : nullptr;
    return widget != nullptr && widget->isVisible();
}

bool widgetComesBefore(QWidget *root, const QString &firstObjectName, const QString &secondObjectName)
{
    auto *first = root != nullptr ? root->findChild<QWidget *>(firstObjectName) : nullptr;
    auto *second = root != nullptr ? root->findChild<QWidget *>(secondObjectName) : nullptr;
    if (first == nullptr || second == nullptr || first->parentWidget() != second->parentWidget()) {
        return false;
    }

    if (QLayout *layout = first->parentWidget()->layout(); layout != nullptr) {
        return layout->indexOf(first) >= 0
            && layout->indexOf(second) >= 0
            && layout->indexOf(first) < layout->indexOf(second);
    }

    return false;
}

bool widgetComesAfter(QWidget *root, const QString &firstObjectName, const QString &secondObjectName)
{
    return widgetComesBefore(root, secondObjectName, firstObjectName);
}

QString parentObjectNameFor(QWidget *root, const QString &objectName)
{
    auto *widget = root != nullptr ? root->findChild<QWidget *>(objectName) : nullptr;
    return widget != nullptr && widget->parentWidget() != nullptr
        ? widget->parentWidget()->objectName()
        : QString();
}

QString grandparentObjectNameFor(QWidget *root, const QString &objectName)
{
    auto *widget = root != nullptr ? root->findChild<QWidget *>(objectName) : nullptr;
    return widget != nullptr
            && widget->parentWidget() != nullptr
            && widget->parentWidget()->parentWidget() != nullptr
        ? widget->parentWidget()->parentWidget()->objectName()
        : QString();
}

int formRowForLabelText(QWidget *root, const QString &labelText)
{
    if (root == nullptr) {
        return -1;
    }

    const QList<QFormLayout *> forms = root->findChildren<QFormLayout *>();
    for (QFormLayout *form : forms) {
        if (form == nullptr) {
            continue;
        }
        for (int row = 0; row < form->rowCount(); ++row) {
            QLayoutItem *labelItem = form->itemAt(row, QFormLayout::LabelRole);
            auto *label = labelItem != nullptr ? qobject_cast<QLabel *>(labelItem->widget()) : nullptr;
            if (label != nullptr && label->text() == labelText) {
                return row;
            }
        }
    }

    return -1;
}

QModelIndex treeIndexForSourceLine(QAbstractItemModel *model, int lineNumber, const QModelIndex &parent = QModelIndex())
{
    if (model == nullptr || lineNumber <= 0) {
        return QModelIndex();
    }

    constexpr int kInspectorSourceLineRoleForTest = Qt::UserRole + 700;
    const int rowCount = model->rowCount(parent);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if (!index.isValid()) {
            continue;
        }
        if (index.data(kInspectorSourceLineRoleForTest).toInt() == lineNumber) {
            return index;
        }
        const QModelIndex childMatch = treeIndexForSourceLine(model, lineNumber, index);
        if (childMatch.isValid()) {
            return childMatch;
        }
    }

    return QModelIndex();
}

QGraphicsItem *sceneItemForSourceLine(QGraphicsScene *scene, int lineNumber)
{
    if (scene == nullptr || lineNumber <= 0) {
        return nullptr;
    }

    constexpr int kMapSceneLineNumberRoleForTest = Qt::UserRole + 121;
    const QList<QGraphicsItem *> items = scene->items();
    for (QGraphicsItem *item : items) {
        if (item != nullptr
            && item->data(kMapSceneLineNumberRoleForTest).toInt() == lineNumber
            && item->flags().testFlag(QGraphicsItem::ItemIsSelectable)
            && item->acceptedMouseButtons() != Qt::NoButton) {
            return item;
        }
    }
    return nullptr;
}

int tabIndexForChild(QTabWidget *tabs, QWidget *child)
{
    if (tabs == nullptr || child == nullptr) {
        return -1;
    }

    for (int index = 0; index < tabs->count(); ++index) {
        QWidget *tab = tabs->widget(index);
        if (tab != nullptr && (tab == child || tab->isAncestorOf(child))) {
            return index;
        }
    }

    return -1;
}

int runSelectionPanelTypeValuesTest()
{
    const CommandCatalogStore catalogStore;
    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogStore.catalogObject());
    const QStringList directAreaTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("area"));
    if (!expect(!directAreaTypes.isEmpty(), "Inspector catalog for area types is empty before UI interaction.")) {
        return 1;
    }
    if (!expect(directAreaTypes.contains(QStringLiteral("water")) && directAreaTypes.contains(QStringLiteral("sand")),
                "Inspector catalog for area should include water and sand before UI interaction.")) {
        return 1;
    }

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for selection panel type-values test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("selection_panel_type_values.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for selection panel type-values test.")) {
        return 1;
    }

    const QByteArray th2Contents =
        "encoding utf-8\n"
        "\n"
        "scrap selection-panel-test -projection plan\n"
        "line border -id border\n"
        "  0 0\n"
        "  50 0\n"
        "endline\n"
        "area water\n"
        "  border\n"
        "endarea\n"
        "line label -text Entrance\n"
        "  60 0\n"
        "  80 0\n"
        "endline\n"
        "point station -name 3.11 -clip off\n"
        "  10 10\n"
        "point altitude -value \"[fix 1300]\"\n"
        "  15 10\n"
        "endscrap\n";
    file.write(th2Contents);
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *externalCommandSurfaceButton = new QPushButton(QStringLiteral("External command surface"), central);
    layout->addWidget(externalCommandSurfaceButton);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for selection panel type-values test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *typeCombo = mapTab->findChild<QComboBox *>(QStringLiteral("mapObjectQuickTypeCombo"));
    if (!expect(typeCombo != nullptr, "Selection panel type combo was not found.")) {
        return 1;
    }
    auto *targetScrapCombo = mapTab->findChild<QComboBox *>(QStringLiteral("mapObjectQuickTargetScrapCombo"));
    if (!expect(targetScrapCombo != nullptr, "Selection panel target scrap combo was not found.")) {
        return 1;
    }

    if (!expect(typeCombo->completer() != nullptr
                && typeCombo->completer()->completionMode() == QCompleter::UnfilteredPopupCompletion,
                "Selection panel type combo should use unfiltered completion mode so the popup is not restricted to current text.")) {
        return 1;
    }

    auto *inspectorTabs = mapTab->findChild<QTabWidget *>();
    if (!expect(inspectorTabs != nullptr, "Map inspector tabs were not found.")) {
        return 1;
    }
    auto *stylePreview = mapTab->findChild<QWidget *>(QStringLiteral("mapObjectStylePreview"));
    if (!expect(stylePreview != nullptr, "Selection panel style preview widget was not found.")) {
        return 1;
    }
    const QList<QDoubleSpinBox *> inspectorDoubleSpinBoxes = mapTab->findChildren<QDoubleSpinBox *>();
    if (!expect(!inspectorDoubleSpinBoxes.isEmpty(),
                "Map inspector should expose double spin boxes for numeric fields.")) {
        return 1;
    }
    for (QDoubleSpinBox *spinBox : inspectorDoubleSpinBoxes) {
        if (!expect(spinBox != nullptr && !spinBox->keyboardTracking(),
                    "Map inspector double spin boxes should defer typed values until editing is committed.")) {
            return 1;
        }
    }

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr && mapView->scene() != nullptr,
                "Map graphics view was not found.")) {
        return 1;
    }
    mapTab->goToLine(4);
    pumpEvents();
    if (!expect(!mapView->scene()->selectedItems().isEmpty(),
                "Moving the text cursor to a map-object line should select the corresponding map item.")) {
        return 1;
    }
    if (!expect(stylePreview->isVisible(),
                "Text-cursor synchronized map selection should show the style preview.")) {
        return 1;
    }
    hostWindow.resize(640, 720);
    pumpEvents();
    QLabel *metadataLabel = visibleLabelContaining(mapTab, QStringLiteral("Source line 4"));
    if (!expect(metadataLabel != nullptr,
                "Selection panel metadata label was not found after selecting a line object.")) {
        return 1;
    }
    const QRect metadataRect = metadataLabel->geometry().translated(metadataLabel->parentWidget()->mapTo(mapTab, QPoint(0, 0)));
    if (!expect(metadataRect.right() <= mapTab->rect().right(),
                "Selection panel metadata label should wrap inside a narrow inspector instead of overflowing horizontally.")) {
        return 1;
    }
    mapView->scene()->clearSelection();
    pumpEvents();
    if (!expect(mapView->scene()->selectedItems().isEmpty(),
                "Map scene selection should be empty after clearing it.")) {
        return 1;
    }
    if (!expect(visibleNoSelectionLabel(mapTab),
                "Selection panel should show the empty state after the map selection is cleared, even when the text cursor is on a map-object line.")) {
        return 1;
    }
    if (!expect(!stylePreview->isVisible(),
                "Style preview should be hidden when no map object is selected.")) {
        return 1;
    }

    auto *objectsTree = mapTab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    if (!expect(objectsTree != nullptr && objectsTree->selectionModel() != nullptr,
                "Objects inspector tree was not found before testing Selection tab refresh.")) {
        return 1;
    }
    auto *autoCollapseExpandScrapsCheck =
        mapTab->findChild<QCheckBox *>(QStringLiteral("mapObjectsAutoCollapseExpandScrapsCheck"));
    if (!expect(autoCollapseExpandScrapsCheck != nullptr,
                "Objects inspector auto-collapse/expand scraps checkbox was not found.")) {
        return 1;
    }
    if (!expect(!autoCollapseExpandScrapsCheck->isChecked()
                    && !sessionStore.therionMapObjectsAutoCollapseExpandScrapsEnabled(),
                "Objects inspector auto-collapse/expand scraps should be disabled by default.")) {
        return 1;
    }
    const int objectsTabIndex = tabIndexForChild(inspectorTabs, objectsTree);
    if (!expect(objectsTabIndex > 0,
                "Objects inspector tab should be available after the Selection tab.")) {
        return 1;
    }
    const QModelIndex lineObjectIndex = treeIndexForSourceLine(objectsTree->model(), 4);
    if (!expect(lineObjectIndex.isValid(),
                "Objects inspector tree should contain the line object from the TH2 fixture.")) {
        return 1;
    }
    QModelIndex scrapObjectIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    if (!expect(scrapObjectIndex.isValid(),
                "Objects inspector tree should contain the scrap object from the TH2 fixture before expansion checks.")) {
        return 1;
    }
    objectsTree->collapse(scrapObjectIndex);
    pumpEvents();
    if (!expect(!objectsTree->isExpanded(scrapObjectIndex),
                "Objects inspector test setup should be able to collapse the scrap row.")) {
        return 1;
    }
    auto *sourceEditor = mapTab->findChild<QPlainTextEdit *>();
    if (!expect(sourceEditor != nullptr,
                "Embedded source editor was not found before testing Objects expansion preservation.")) {
        return 1;
    }
    sourceEditor->moveCursor(QTextCursor::End);
    sourceEditor->insertPlainText(QStringLiteral("\n# refresh expansion state\n"));
    pumpEventsFor(200);
    scrapObjectIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    if (!expect(scrapObjectIndex.isValid(),
                "Objects inspector tree should still contain the scrap object after source refresh.")) {
        return 1;
    }
    if (!expect(!objectsTree->isExpanded(scrapObjectIndex),
                "Objects inspector should preserve manually collapsed scraps across source-driven rebuilds.")) {
        return 1;
    }
    auto *lineSceneItem = sceneItemForSourceLine(mapView->scene(), 4);
    if (!expect(lineSceneItem != nullptr,
                "Map scene should contain the line object before testing map selection expansion.")) {
        return 1;
    }
    mapView->centerOn(lineSceneItem);
    pumpEvents();
    const QPoint clickPosition = mapView->mapFromScene(lineSceneItem->sceneBoundingRect().center());
    sendMouseClick(mapView->viewport(), clickPosition);
    pumpEvents();
    scrapObjectIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    if (!expect(scrapObjectIndex.isValid() && objectsTree->isExpanded(scrapObjectIndex),
                "Selecting a map object inside a collapsed scrap should expand that scrap in the Objects inspector.")) {
        return 1;
    }
    inspectorTabs->setCurrentIndex(objectsTabIndex);
    pumpEvents();
    const QModelIndex refreshedLineObjectIndex = treeIndexForSourceLine(objectsTree->model(), 4);
    if (!expect(refreshedLineObjectIndex.isValid(),
                "Objects inspector tree should contain the line object after expansion refresh.")) {
        return 1;
    }
    objectsTree->selectionModel()->setCurrentIndex(refreshedLineObjectIndex,
                                                   QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    pumpEvents();
    inspectorTabs->setCurrentIndex(0);
    pumpEvents();
    if (!expect(stylePreview->isVisible(),
                "Selection panel should show the style preview after selecting an object in Objects and switching back.")) {
        return 1;
    }
    if (!expect(grandparentObjectNameFor(mapTab, QStringLiteral("mapObjectConfigureButton")) == QStringLiteral("mapLineOptionsEditor"),
                "Line selection should place the full object settings button inside the line Options panel.")) {
        return 1;
    }
    if (!expect(widgetComesAfter(mapTab,
                                 QStringLiteral("mapObjectConfigureButtonRow"),
                                 QStringLiteral("mapObjectClipDisabledCheck")),
                "Line selection should keep the full object settings button after the other line options.")) {
        return 1;
    }
    mapTab->goToLine(11);
    pumpEvents();
    if (!expect(parentObjectNameFor(mapTab, QStringLiteral("mapObjectQuickTextEditor")) == QStringLiteral("mapLineOptionsEditor"),
                "Line label selection should place Text (-text) inside the line Options panel.")) {
        return 1;
    }
    if (!expect(widgetComesBefore(mapTab,
                                  QStringLiteral("mapObjectQuickTextEditor"),
                                  QStringLiteral("mapObjectClipDisabledCheck")),
                "Line label selection should keep Text (-text) before the other line options.")) {
        return 1;
    }
    if (!expect(widgetComesAfter(mapTab,
                                 QStringLiteral("mapObjectConfigureButtonRow"),
                                 QStringLiteral("mapObjectQuickTextEditor")),
                "Line label selection should keep the full object settings button after Text (-text).")) {
        return 1;
    }
    mapTab->goToLine(15);
    pumpEvents();
    if (!expect(visibleLabelCount(mapTab, QStringLiteral("Options")) == 1,
                "Point selection should show a single Options section.")) {
        return 1;
    }
    if (!expect(visibleLabelCount(mapTab, QStringLiteral("Geometry")) == 0,
                "Point selection should not expose a Geometry section.")) {
        return 1;
    }
    if (!expect(isVisibleWithin(mapTab, QStringLiteral("mapObjectClipDisabledCheck")),
                "Point selection should expose the existing -clip off option in Geometry.")) {
        return 1;
    }
    if (!expect(parentObjectNameFor(mapTab, QStringLiteral("mapObjectQuickNameEditor")) == QStringLiteral("mapPointOptionsEditor"),
                "Point selection should place Name inside the point Options panel.")) {
        return 1;
    }
    if (!expect(widgetComesBefore(mapTab,
                                  QStringLiteral("mapObjectQuickNameEditor"),
                                  QStringLiteral("mapObjectClipDisabledCheck")),
                "Point selection should keep Name before the other point options.")) {
        return 1;
    }
    if (!expect(widgetComesBefore(mapTab,
                                  QStringLiteral("mapVertexGeometrySection"),
                                  QStringLiteral("mapObjectActionsSection")),
                "Point Options should appear before Object Actions in the Selection inspector.")) {
        return 1;
    }
    if (!expect(grandparentObjectNameFor(mapTab, QStringLiteral("mapObjectConfigureButton")) == QStringLiteral("mapPointOptionsEditor"),
                "Point selection should place the full object settings button inside the point Options panel.")) {
        return 1;
    }
    if (!expect(widgetComesAfter(mapTab,
                                 QStringLiteral("mapObjectConfigureButtonRow"),
                                 QStringLiteral("mapObjectClipDisabledCheck")),
                "Point selection should keep the full object settings button after the other point options.")) {
        return 1;
    }
    mapTab->goToLine(17);
    pumpEvents();
    if (!expect(parentObjectNameFor(mapTab, QStringLiteral("mapObjectQuickValueEditor")) == QStringLiteral("mapPointOptionsEditor"),
                "Point selection should place Value (-value) inside the point Options panel.")) {
        return 1;
    }
    if (!expect(widgetComesAfter(mapTab,
                                 QStringLiteral("mapObjectConfigureButtonRow"),
                                 QStringLiteral("mapObjectQuickValueEditor")),
                "Point selection should keep the full object settings button after Value (-value).")) {
        return 1;
    }
    scrapObjectIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    if (!expect(scrapObjectIndex.isValid(),
                "Objects inspector tree should contain the scrap object from the TH2 fixture.")) {
        return 1;
    }
    inspectorTabs->setCurrentIndex(objectsTabIndex);
    pumpEvents();
    objectsTree->selectionModel()->setCurrentIndex(scrapObjectIndex,
                                                   QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    pumpEvents();
    inspectorTabs->setCurrentIndex(0);
    pumpEvents();
    auto *projectionCombo = mapTab->findChild<QComboBox *>(QStringLiteral("mapObjectQuickProjectionCombo"));
    if (!expect(projectionCombo != nullptr && projectionCombo->isVisible(),
                "Scrap selection should expose Projection in the Options panel.")) {
        return 1;
    }
    if (!expect(grandparentObjectNameFor(mapTab, QStringLiteral("mapObjectConfigureButton")) == QStringLiteral("mapScrapOptionsEditor"),
                "Scrap selection should place the full object settings button inside the scrap Options panel.")) {
        return 1;
    }
    if (!expect(widgetComesAfter(mapTab,
                                 QStringLiteral("mapObjectConfigureButtonRow"),
                                 QStringLiteral("mapScrapProjectionEditor")),
                "Scrap selection should keep the full object settings button after Projection.")) {
        return 1;
    }
    mapTab->goToLine(4);
    pumpEvents();
    mapView->scene()->clearSelection();
    pumpEvents();

    mapTab->triggerAddLine();
    pumpEvents();
    if (!expect(inspectorTabs->currentIndex() == 0,
                "Line insert should activate the Selection inspector before the first vertex is placed.")) {
        return 1;
    }
    if (!expect(typeCombo->isVisible() && typeCombo->currentText() == QStringLiteral("wall"),
                "Line insert should expose pending type fields with the default wall type.")) {
        return 1;
    }
    if (!expect(targetScrapCombo->isVisible()
                    && targetScrapCombo->count() == 1
                    && targetScrapCombo->currentText() == QStringLiteral("selection-panel-test"),
                "Line insert should expose the existing scrap as the pending insertion target.")) {
        return 1;
    }
    if (!expect(formRowForLabelText(mapTab, QStringLiteral("Insert into")) < formRowForLabelText(mapTab, QStringLiteral("ID")),
                "Pending insert target scrap should be shown before the object ID field.")) {
        return 1;
    }
    if (!expect(!visibleLabelStartingWith(mapTab, QStringLiteral("Pending insert")),
                "Pending insert metadata should be hidden when the target scrap selector is visible.")) {
        return 1;
    }
    if (!expect(stylePreview->isVisible(),
                "Line insert should show the style preview for the pending object.")) {
        return 1;
    }
    auto *subtypeCombo = mapTab->findChild<QComboBox *>(QStringLiteral("mapObjectQuickSubtypeCombo"));
    if (!expect(subtypeCombo != nullptr, "Selection panel subtype combo was not found.")) {
        return 1;
    }
    if (!expect(typeCombo->width() > 200 && subtypeCombo->width() > 140,
                "Selection panel type and subtype fields should expand in the available inspector width.")) {
        return 1;
    }
    commitComboEdit(typeCombo, QStringLiteral("border"));
    pumpEvents();
    commitComboEdit(subtypeCombo, QStringLiteral("invisible"));
    pumpEvents();
    subtypeCombo->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(subtypeCombo, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(subtypeCombo, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    if (!expect(!mapTab->isInsertModeActive(),
                "Esc from a focused pending insert subtype field should leave map insert mode.")) {
        return 1;
    }
    if (!expect(visibleNoSelectionLabel(mapTab),
                "Esc from a focused pending insert field should clear the pending Selection inspector state.")) {
        return 1;
    }
    if (!expect(!stylePreview->isVisible(),
                "Esc from a focused pending insert field should hide the pending style preview.")) {
        return 1;
    }
    mapTab->triggerAddLine();
    pumpEvents();
    if (!expect(typeCombo->isVisible()
                    && typeCombo->currentText() == QStringLiteral("border")
                    && subtypeCombo->currentText() == QStringLiteral("invisible"),
                "Line insert should remember the last pending type and subtype as the next default.")) {
        return 1;
    }
    subtypeCombo->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(subtypeCombo, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(subtypeCombo, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    if (!expect(!mapTab->isInsertModeActive(),
                "Esc should leave the remembered line insert mode before point insert testing.")) {
        return 1;
    }

    mapTab->triggerAddPoint();
    pumpEvents();
    if (!expect(mapTab->isInsertModeActive(),
                "Point insert should enter insert mode before testing Esc from the external command surface.")) {
        return 1;
    }
    externalCommandSurfaceButton->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(externalCommandSurfaceButton, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(externalCommandSurfaceButton, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    if (!expect(!mapTab->isInsertModeActive(),
                "Esc from the main-window command surface focus should leave map insert mode.")) {
        return 1;
    }

    mapTab->triggerInsertScrap();
    pumpEvents();
    const QStringList sourceLines = mapTab->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    const int currentLine = mapTab->currentLineNumber();
    if (!expect(inspectorTabs->currentIndex() == 0,
                "Insert Scrap should keep the Selection inspector active for immediate scrap ID/projection editing.")) {
        return 1;
    }
    if (!expect(!targetScrapCombo->isVisible(),
                "Scrap insert should not show a target scrap selector because scraps are not nested in scraps.")) {
        return 1;
    }
    if (!expect(currentLine > 0
                    && currentLine <= sourceLines.size()
                    && sourceLines.at(currentLine - 1).startsWith(QStringLiteral("scrap ")),
                "Insert Scrap should immediately create and select the new scrap source line.")) {
        return 1;
    }
    if (!expect(visibleLabelStartingWith(mapTab, QStringLiteral("Scrap")),
                "Insert Scrap should select the created scrap in the Selection panel.")) {
        return 1;
    }
    if (!expect(!visibleLabelStartingWith(mapTab, QStringLiteral("New Scrap")),
                "Insert Scrap should not leave the Selection panel in a pending New Scrap state.")) {
        return 1;
    }
    if (!expect(objectsTree != nullptr && objectsTree->currentIndex().data(Qt::UserRole + 700).toInt() == currentLine,
                "Insert Scrap should select the newly created scrap in the Objects tree.")) {
        return 1;
    }

    mapTab->goToLine(4);
    inspectorTabs->setCurrentIndex(0);
    pumpEvents();
    if (!expect(!visibleLabelStartingWith(mapTab, QStringLiteral("New Scrap")),
                "Selecting another object after confirmed scrap insert should leave the pending New Scrap state.")) {
        return 1;
    }

    return 0;
}

int runObjectsInspectorAutoCollapseExpandScrapsTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for Objects inspector focus test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("objects_auto_collapse_expand_scraps.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for Objects inspector focus test.")) {
        return 1;
    }

    const QByteArray th2Contents =
        "encoding utf-8\n"
        "\n"
        "scrap primary-focus-test -projection plan\n"
        "line border -id primary-border\n"
        "  0 0\n"
        "  50 0\n"
        "endline\n"
        "endscrap\n"
        "scrap secondary-focus-test -projection plan\n"
        "line border -id secondary-border\n"
        "  100 100\n"
        "  150 100\n"
        "endline\n"
        "endscrap\n";
    file.write(th2Contents);
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), &hostWindow);
    hostWindow.setCentralWidget(mapTab);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for Objects inspector focus test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *objectsTree = mapTab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    if (!expect(objectsTree != nullptr && objectsTree->selectionModel() != nullptr,
                "Objects inspector tree was not found for auto-collapse/expand scraps test.")) {
        return 1;
    }
    auto *autoCollapseExpandScrapsCheck =
        mapTab->findChild<QCheckBox *>(QStringLiteral("mapObjectsAutoCollapseExpandScrapsCheck"));
    if (!expect(autoCollapseExpandScrapsCheck != nullptr,
                "Objects inspector auto-collapse/expand scraps checkbox was not found in focus test.")) {
        return 1;
    }
    if (!expect(!autoCollapseExpandScrapsCheck->isChecked()
                    && !sessionStore.therionMapObjectsAutoCollapseExpandScrapsEnabled(),
                "Objects inspector auto-collapse/expand scraps should be disabled by default in focus test.")) {
        return 1;
    }

    mapTab->goToLine(4);
    pumpEvents();
    QModelIndex primaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    QModelIndex secondaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 9);
    if (!expect(primaryScrapIndex.isValid() && secondaryScrapIndex.isValid(),
                "Objects inspector should contain both scraps for auto-collapse/expand scraps test.")) {
        return 1;
    }
    objectsTree->expand(secondaryScrapIndex);
    pumpEvents();
    if (!expect(objectsTree->isExpanded(secondaryScrapIndex),
                "Objects inspector test setup should be able to expand the secondary scrap.")) {
        return 1;
    }

    autoCollapseExpandScrapsCheck->setChecked(true);
    pumpEvents();
    primaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    secondaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 9);
    if (!expect(sessionStore.therionMapObjectsAutoCollapseExpandScrapsEnabled(),
                "Objects inspector auto-collapse/expand scraps checkbox should persist to the session store.")) {
        return 1;
    }
    if (!expect(primaryScrapIndex.isValid()
                    && secondaryScrapIndex.isValid()
                    && objectsTree->isExpanded(primaryScrapIndex)
                    && !objectsTree->isExpanded(secondaryScrapIndex),
                "Auto-collapse/expand scraps should keep the current object's scrap open and close other scraps.")) {
        return 1;
    }
    auto *sourceEditor = mapTab->findChild<QPlainTextEdit *>();
    if (!expect(sourceEditor != nullptr,
                "Embedded source editor was not found before testing auto-collapse/expand scraps refresh.")) {
        return 1;
    }
    sourceEditor->moveCursor(QTextCursor::End);
    sourceEditor->insertPlainText(QStringLiteral("\n# refresh auto-collapse expansion state\n"));
    pumpEventsFor(200);
    primaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    secondaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 9);
    if (!expect(primaryScrapIndex.isValid()
                    && secondaryScrapIndex.isValid()
                    && objectsTree->isExpanded(primaryScrapIndex)
                    && !objectsTree->isExpanded(secondaryScrapIndex),
                "Auto-collapse/expand scraps should keep other scraps collapsed across source-driven rebuilds.")) {
        return 1;
    }

    mapTab->goToLine(10);
    pumpEvents();
    primaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    secondaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 9);
    if (!expect(primaryScrapIndex.isValid()
                    && secondaryScrapIndex.isValid()
                    && !objectsTree->isExpanded(primaryScrapIndex)
                    && objectsTree->isExpanded(secondaryScrapIndex),
                "Auto-collapse/expand scraps should follow raw-editor object-line navigation.")) {
        return 1;
    }

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr && mapView->scene() != nullptr,
                "Map graphics view was not found for auto-collapse/expand scraps test.")) {
        return 1;
    }
    QGraphicsItem *secondaryLineItem = sceneItemForSourceLine(mapView->scene(), 10);
    if (!expect(secondaryLineItem != nullptr,
                "Map scene should contain the secondary scrap line before testing focus switch.")) {
        return 1;
    }
    mapView->centerOn(secondaryLineItem);
    pumpEvents();
    sendMouseClick(mapView->viewport(), mapView->mapFromScene(secondaryLineItem->sceneBoundingRect().center()));
    pumpEvents();
    primaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 3);
    secondaryScrapIndex = treeIndexForSourceLine(objectsTree->model(), 9);
    if (!expect(primaryScrapIndex.isValid()
                    && secondaryScrapIndex.isValid()
                    && !objectsTree->isExpanded(primaryScrapIndex)
                    && objectsTree->isExpanded(secondaryScrapIndex),
                "Selecting an object in another scrap should expand that scrap and collapse the previous scrap.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int result = runSelectionPanelTypeValuesTest(); result != 0) {
        return result;
    }
    return runObjectsInspectorAutoCollapseExpandScrapsTest();
}
