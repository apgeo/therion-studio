#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QTemporaryDir>
#include <QVBoxLayout>

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
}

int runBackgroundVisibilityDoesNotDirtyDocumentTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for background visibility test.")) {
        return 1;
    }

    const QString imagePath = tempDir.filePath(QStringLiteral("background.png"));
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(QColor(80, 120, 160, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary background image.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_visibility.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for background visibility test.")) {
        return 1;
    }

    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 -16 16 0\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1} {0 {}} background.png 0 {}\n"
        "\n"
        "scrap visibility-smoke -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    file.write(th2Contents.toUtf8());
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for background visibility test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one background layer auto-loaded from xth_me_image_insert metadata.")) {
        return 1;
    }
    if (!expect(mapTab->isBackgroundLayerVisible(0),
                "Background layer should start visible from xth metadata.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Map tab should not be dirty after loading metadata background layer.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    mapTab->setSelectedBackgroundLayerIndex(0);
    mapTab->toggleSelectedBackgroundLayerVisibility();
    pumpEvents();

    if (!expect(!mapTab->isBackgroundLayerVisible(0), "Background layer should be hidden after toggle.")) {
        return 1;
    }
    if (!expect(mapTab->text() == originalText,
                "Hiding a background layer should not rewrite xth metadata or source text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Hiding a background layer should not dirty the TH2 document.")) {
        return 1;
    }

    mapTab->toggleSelectedBackgroundLayerVisibility();
    pumpEvents();

    if (!expect(mapTab->isBackgroundLayerVisible(0), "Background layer should be visible after second toggle.")) {
        return 1;
    }
    if (!expect(mapTab->text() == originalText,
                "Showing a background layer should not rewrite xth metadata or source text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Showing a background layer should not dirty the TH2 document.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runBackgroundVisibilityDoesNotDirtyDocumentTest();
}
