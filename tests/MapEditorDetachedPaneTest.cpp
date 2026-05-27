#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QEventLoop>
#include <QGraphicsView>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QWidget>

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

int countDetachedMapWindows(const QWidget *hostWindow)
{
    int count = 0;
    const auto topLevels = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevels) {
        if (widget == nullptr || widget == hostWindow || !widget->isVisible()) {
            continue;
        }

        auto *window = qobject_cast<QMainWindow *>(widget);
        if (window == nullptr) {
            continue;
        }

        QWidget *central = window->centralWidget();
        if (central == nullptr) {
            continue;
        }

        if (central->findChild<QGraphicsView *>() != nullptr) {
            ++count;
        }
    }

    return count;
}

int runDetachedPaneLifecycleTest()
{
    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QWidget hostWindow;
    hostWindow.resize(1280, 900);
    auto *layout = new QVBoxLayout(&hostWindow);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), &hostWindow);
    layout->addWidget(mapTab);
    hostWindow.show();
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr, "Map view was not found in MapEditorTab.")) {
        return 1;
    }

    if (!expect(mapTab->mapPaneWindowActionText().contains(QStringLiteral("Separate"), Qt::CaseInsensitive),
                "Initial detach action text should indicate separating the map.")) {
        return 1;
    }

    if (!expect(countDetachedMapWindows(&hostWindow) == 0,
                "No detached map windows should exist before detaching.")) {
        return 1;
    }

    mapTab->toggleMapPaneWindow();
    pumpEvents();

    if (!expect(mapTab->mapPaneWindowActionText().contains(QStringLiteral("Return"), Qt::CaseInsensitive),
                "After detaching, action text should switch to return-map state.")) {
        return 1;
    }

    if (!expect(mapView->window() != &hostWindow,
                "Map view should move into a detached top-level window after detaching.")) {
        return 1;
    }

    if (!expect(countDetachedMapWindows(&hostWindow) == 1,
                "Exactly one detached map window should exist after detaching.")) {
        return 1;
    }

    // Reattaches by closing the detached window through the same toggle action.
    mapTab->toggleMapPaneWindow();
    pumpEvents();

    if (!expect(mapTab->mapPaneWindowActionText().contains(QStringLiteral("Separate"), Qt::CaseInsensitive),
                "After reattaching, action text should switch back to separate-map state.")) {
        return 1;
    }

    if (!expect(mapView->window() == &hostWindow,
                "Map view should return to the host window after reattaching.")) {
        return 1;
    }

    if (!expect(countDetachedMapWindows(&hostWindow) == 0,
                "No detached map windows should remain after reattaching.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runDetachedPaneLifecycleTest();
}
