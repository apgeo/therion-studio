#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionBackgroundMetadata.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

bool writeTextFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(content) == content.size();
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

int runMetadataXviPlacementIgnoresSessionScenePositionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for XVI placement test.")) {
        return 1;
    }

    const QString xviPath = tempDir.filePath(QStringLiteral("placed.xvi"));
    QFile xviFile(xviPath);
    if (!expect(xviFile.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary XVI file for placement test.")) {
        return 1;
    }
    xviFile.write(
        "set XVIgrid {-2651.57480315 -4006.2992126 78.7401574803 0.0 0.0 78.7401574803 76.0 98.0}\n"
        "set XVIstations {\n"
        "  {-701.54 -2508.75 1.4}\n"
        "  {0.0 0.0 1.0}\n"
        "}\n"
        "set XVIshots {\n"
        "  {-701.54 -2508.75 0.0 0.0}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {black -701.54 -2508.75 -640.0 -2450.0}\n"
        "}\n");
    xviFile.close();

    const QString filePath = tempDir.filePath(QStringLiteral("metadata_xvi_restore.th2"));
    QFile th2File(filePath);
    if (!expect(th2File.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for XVI placement test.")) {
        return 1;
    }
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust -128.0 -128.0 6033.5118110225 7797.7952755891\n"
        "##XTHERION## xth_me_area_zoom_to 25\n"
        "##XTHERION## xth_me_image_insert {1950.0348031500002 1 1.0} {1497.5492126 1.4} placed.xvi 0 {}\n"
        "\n"
        "scrap xvi-restore -projection plan -scale [0 0 787.4 787.4 0 0 10 10 m]\n"
        "point 1950.0348031500002 1497.5492126 station -name 1.4\n"
        "point 2651.57480315 4006.2992126 station -name 1.0\n"
        "endscrap\n");
    th2File.write(th2Contents.toUtf8());
    th2File.close();

    QtFileSystem fileSystem;
    FakeSessionStore cleanSessionStore;
    QPointF metadataPosition;
    {
        QMainWindow hostWindow;
        hostWindow.resize(960, 720);
        auto *central = new QWidget(&hostWindow);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto *mapTab = new MapEditorTab(fileSystem, cleanSessionStore, CommandCatalogStore(), central);
        layout->addWidget(mapTab);
        hostWindow.setCentralWidget(central);
        hostWindow.show();
        pumpEvents();

        QString errorMessage;
        if (!expect(mapTab->loadFile(filePath, &errorMessage),
                    "MapEditorTab failed to load TH2 file for clean XVI placement test.")) {
            if (!errorMessage.isEmpty()) {
                std::cerr << errorMessage.toStdString() << '\n';
            }
            return 1;
        }
        pumpEvents();

        if (!expect(mapTab->backgroundLayerCount() == 1,
                    "Expected one metadata XVI layer in clean placement test.")) {
            return 1;
        }
        metadataPosition = mapTab->backgroundLayerPosition(0);
    }

    FakeSessionStore staleSessionStore;
    const QString documentKey = QFileInfo(filePath).canonicalFilePath();
    QJsonObject staleLayer;
    staleLayer.insert(QStringLiteral("path"), QFileInfo(xviPath).absoluteFilePath());
    staleLayer.insert(QStringLiteral("visible"), true);
    staleLayer.insert(QStringLiteral("opacity"), 0.58);
    staleLayer.insert(QStringLiteral("gamma"), 1.0);
    staleLayer.insert(QStringLiteral("x"), metadataPosition.x() + 197.0);
    staleLayer.insert(QStringLiteral("y"), metadataPosition.y() + 42.0);
    QJsonArray layers;
    layers.append(staleLayer);
    QJsonObject root;
    root.insert(documentKey, layers);
    staleSessionStore.setTherionMapBackgroundLayers(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, staleSessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for stale-session XVI placement test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one metadata XVI layer restored from stale session.")) {
        return 1;
    }
    const QPointF restoredPosition = mapTab->backgroundLayerPosition(0);
    if (!expect(nearlyEqual(restoredPosition.x(), metadataPosition.x())
                    && nearlyEqual(restoredPosition.y(), metadataPosition.y()),
                "Metadata-backed XVI layer should ignore stale session scene position and keep XTherion placement.")) {
        return 1;
    }

    return 0;
}

int runXviCacheReloadsSameTimestampContentChangeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for XVI cache reload test.")) {
        return 1;
    }

    const QString xviPath = tempDir.filePath(QStringLiteral("cached.xvi"));
    const QByteArray firstXvi =
        "set XVIgrid {0 0}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {black 0 0 100 100}\n"
        "}\n";
    if (!expect(writeTextFile(xviPath, firstXvi), "Failed to write initial XVI cache test file.")) {
        return 1;
    }
    const QDateTime originalModified = QFileInfo(xviPath).lastModified();

    const QString filePath = tempDir.filePath(QStringLiteral("xvi_cache_reload.th2"));
    const QByteArray th2Contents =
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 0 1000 1000\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {100 1 1.0} {100 root} cached.xvi 0 {}\n"
        "\n"
        "scrap cache-reload -projection plan\n"
        "point 100 100 station -name root\n"
        "endscrap\n";
    if (!expect(writeTextFile(filePath, th2Contents), "Failed to write TH2 cache reload test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    QRectF firstBounds;
    {
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
                    "MapEditorTab failed to load initial XVI cache reload file.")) {
            if (!errorMessage.isEmpty()) {
                std::cerr << errorMessage.toStdString() << '\n';
            }
            return 1;
        }
        pumpEvents();

        if (!expect(mapTab->backgroundLayerCount() == 1,
                    "Expected one metadata XVI layer before cache reload.")) {
            return 1;
        }
        firstBounds = mapTab->backgroundLayerSceneBounds(0);
        if (!expect(firstBounds.isValid(), "Expected valid initial XVI background bounds.")) {
            return 1;
        }
    }

    const QByteArray secondXvi =
        "set XVIgrid {0 0}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {black 800 800 900 900}\n"
        "}\n";
    if (!expect(writeTextFile(xviPath, secondXvi), "Failed to rewrite XVI cache test file.")) {
        return 1;
    }
    QFile timestampFile(xviPath);
    if (!expect(timestampFile.open(QIODevice::ReadWrite),
                "Failed to reopen XVI cache test file for timestamp reset.")) {
        return 1;
    }
    if (!expect(timestampFile.setFileTime(originalModified, QFileDevice::FileModificationTime),
                "Failed to restore original XVI cache test modification timestamp.")) {
        return 1;
    }
    timestampFile.close();

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
                "MapEditorTab failed to reload TH2 after same-timestamp XVI content change.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one metadata XVI layer after same-timestamp cache reload.")) {
        return 1;
    }
    const QRectF secondBounds = mapTab->backgroundLayerSceneBounds(0);
    if (!expect(secondBounds.isValid(), "Expected valid reloaded XVI background bounds.")) {
        return 1;
    }
    if (!expect(!nearlyEqual(secondBounds.left(), firstBounds.left())
                    || !nearlyEqual(secondBounds.top(), firstBounds.top())
                    || !nearlyEqual(secondBounds.width(), firstBounds.width())
                    || !nearlyEqual(secondBounds.height(), firstBounds.height()),
                "XVI cache should reload changed content even when the file timestamp is unchanged.")) {
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
    if (const int rc = runBackgroundGammaPreservesSelectedLayerTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMetadataXviPlacementIgnoresSessionScenePositionTest(); rc != 0) {
        return rc;
    }
    return runXviCacheReloadsSameTimestampContentChangeTest();
}
