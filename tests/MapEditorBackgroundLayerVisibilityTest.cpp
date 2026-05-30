#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionBackgroundMetadata.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtMath>

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

bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
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

int runBackgroundGammaPreservesPlacementTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for background gamma test.")) {
        return 1;
    }

    const QString imagePath = tempDir.filePath(QStringLiteral("background.png"));
    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(QColor(160, 120, 80, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary background image for gamma test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_gamma.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for background gamma test.")) {
        return 1;
    }

    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 0 1000 1000\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {245.0 1.0} 822.0 background.png 0 {}\n"
        "\n"
        "scrap gamma-smoke -projection plan\n"
        "point 500 500 station -name A\n"
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
                "MapEditorTab failed to load TH2 file for background gamma test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one background layer auto-loaded for gamma test.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(0);
    const QPointF originalPosition = mapTab->backgroundLayerPosition(0);
    mapTab->setSelectedBackgroundLayerGamma(1.5);
    pumpEvents();

    if (!expect(mapTab->text().contains(QStringLiteral("xth_me_image_insert {245.0 1.0 1.5} 822.0 background.png 0 {}")),
                "Changing background gamma should preserve the existing X/Y metadata tokens and update only the gamma token.")) {
        return 1;
    }

    const QPointF updatedPosition = mapTab->backgroundLayerPosition(0);
    if (!expect(nearlyEqual(updatedPosition.x(), originalPosition.x())
                    && nearlyEqual(updatedPosition.y(), originalPosition.y()),
                "Changing background gamma should not move the scene layer.")) {
        return 1;
    }

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(mapTab->text(), filePath);
    if (!expect(references.size() == 1,
                "Expected gamma-updated metadata to keep one background reference.")) {
        return 1;
    }
    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.hasBasePosition
                    && nearlyEqual(reference.basePosition.x(), 245.0)
                    && nearlyEqual(reference.basePosition.y(), 822.0),
                "Changing background gamma should preserve XTherion X/Y placement metadata.")) {
        return 1;
    }
    if (!expect(reference.hasImageScale && nearlyEqual(reference.imageScale, 1.5),
                "Changing background gamma should update only the metadata gamma value.")) {
        return 1;
    }

    return 0;
}

int runBackgroundGammaPreservesSelectedLayerTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for multi-layer gamma test.")) {
        return 1;
    }

    const QString firstImagePath = tempDir.filePath(QStringLiteral("zab_ch2.png"));
    const QString secondImagePath = tempDir.filePath(QStringLiteral("zab_dom1.png"));
    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(QColor(100, 140, 180, 255));
    if (!expect(image.save(firstImagePath) && image.save(secondImagePath),
                "Failed to create temporary background images for multi-layer gamma test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_gamma_selection.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for multi-layer gamma test.")) {
        return 1;
    }

    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 0 1000 1000\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {126.6 1.0} 304.1 zab_ch2.png 0 {}\n"
        "##XTHERION## xth_me_image_insert {820.4 1.0} 213.1 zab_dom1.png 0 {}\n"
        "\n"
        "scrap gamma-selection-smoke -projection plan\n"
        "point 500 500 station -name A\n"
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
                "MapEditorTab failed to load TH2 file for multi-layer gamma test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 2,
                "Expected two background layers auto-loaded for multi-layer gamma test.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(0);
    const QPointF firstOriginalPosition = mapTab->backgroundLayerPosition(0);
    mapTab->setSelectedBackgroundLayerGamma(0.8);
    pumpEvents();

    if (!expect(mapTab->selectedBackgroundLayerIndex() == 0,
                "Changing gamma on the first layer should preserve the selected background layer.")) {
        return 1;
    }
    if (!expect(nearlyEqual(mapTab->backgroundLayerPosition(0).x(), firstOriginalPosition.x())
                    && nearlyEqual(mapTab->backgroundLayerPosition(0).y(), firstOriginalPosition.y()),
                "Changing gamma on the first layer should not move that layer.")) {
        return 1;
    }
    if (!expect(mapTab->text().contains(QStringLiteral("xth_me_image_insert {126.6 1.0 0.8} 304.1 zab_ch2.png 0 {}"))
                    && mapTab->text().contains(QStringLiteral("xth_me_image_insert {820.4 1.0} 213.1 zab_dom1.png 0 {}")),
                "Changing gamma on the first layer should update only the first layer metadata.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(1);
    const QPointF secondOriginalPosition = mapTab->backgroundLayerPosition(1);
    mapTab->setSelectedBackgroundLayerGamma(1.4);
    pumpEvents();

    if (!expect(mapTab->selectedBackgroundLayerIndex() == 1,
                "Changing gamma on the second layer should preserve the selected background layer.")) {
        return 1;
    }
    if (!expect(nearlyEqual(mapTab->backgroundLayerPosition(1).x(), secondOriginalPosition.x())
                    && nearlyEqual(mapTab->backgroundLayerPosition(1).y(), secondOriginalPosition.y()),
                "Changing gamma on the second layer should not move that layer.")) {
        return 1;
    }
    if (!expect(mapTab->text().contains(QStringLiteral("xth_me_image_insert {126.6 1.0 0.8} 304.1 zab_ch2.png 0 {}"))
                    && mapTab->text().contains(QStringLiteral("xth_me_image_insert {820.4 1.0 1.4} 213.1 zab_dom1.png 0 {}")),
                "Changing gamma on the second layer should update only the second layer metadata.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int rc = runBackgroundVisibilityDoesNotDirtyDocumentTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBackgroundGammaPreservesPlacementTest(); rc != 0) {
        return rc;
    }
    return runBackgroundGammaPreservesSelectedLayerTest();
}
