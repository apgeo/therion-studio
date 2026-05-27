#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QEventLoop>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTabWidget>
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

void sendKey(QWidget *widget, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    if (widget == nullptr) {
        return;
    }

    QKeyEvent event(type, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
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
    if (!expect(stylePreview->isVisible(),
                "Line insert should show the style preview for the pending object.")) {
        return 1;
    }
    auto *subtypeCombo = mapTab->findChild<QComboBox *>(QStringLiteral("mapObjectQuickSubtypeCombo"));
    if (!expect(subtypeCombo != nullptr, "Selection panel subtype combo was not found.")) {
        return 1;
    }
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

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runSelectionPanelTypeValuesTest();
}
