#include "../src/app/MapEditorTab.h"
#include "../src/app/MapEditorSceneInternals.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <cmath>
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

QPushButton *findButtonByText(const MapEditorTab &tab, const QString &text)
{
    const auto buttons = tab.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button != nullptr && button->text().compare(text, Qt::CaseInsensitive) == 0) {
            return button;
        }
    }

    return nullptr;
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

bool dragItemBySceneDelta(QGraphicsView *view, QGraphicsItem *item, const QPointF &requestedSceneDelta)
{
    if (view == nullptr || item == nullptr || view->viewport() == nullptr) {
        return false;
    }

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
        "endline\n"
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

    const QRectF visibleSceneRect = mapView->mapToScene(mapView->viewport()->rect()).boundingRect();
    mapTab->goToLine(4);
    pumpEvents();

    auto *anchorItem = findCenteredLineAnchor(mapView->scene(), visibleSceneRect);
    if (!expect(anchorItem != nullptr, "No visible editable line anchor was found after selecting line geometry.")) {
        return 1;
    }

    QPushButton *undoButton = findButtonByText(*mapTab, QStringLiteral("Undo"));
    QPushButton *redoButton = findButtonByText(*mapTab, QStringLiteral("Redo"));
    if (!expect(undoButton != nullptr && redoButton != nullptr, "Undo/Redo buttons were not found.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    if (!expect(!originalText.isEmpty(), "Loaded TH2 text should not be empty.")) {
        return 1;
    }

    if (!expect(dragItemBySceneDelta(mapView, anchorItem, QPointF(36.0, -24.0)),
                "Failed to drag editable map anchor during smoke test.")) {
        return 1;
    }

    const QString changedText = mapTab->text();
    if (!expect(changedText != originalText, "Drag did not change TH2 text content.")) {
        return 1;
    }
    if (!expect(undoButton->isEnabled(), "Undo should be enabled after a map drag edit.")) {
        return 1;
    }

    undoButton->click();
    pumpEvents();

    if (!expect(mapTab->text() == originalText, "Undo did not restore original TH2 text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Map tab should be clean after undo to baseline.")) {
        return 1;
    }
    if (!expect(redoButton->isEnabled(), "Redo should be enabled after undoing map drag edit.")) {
        return 1;
    }

    redoButton->click();
    pumpEvents();

    if (!expect(mapTab->text() == changedText, "Redo did not restore edited TH2 text.")) {
        return 1;
    }
    if (!expect(mapTab->isDirty(), "Map tab should be dirty after redo reapplies edit.")) {
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
    return runDragUndoRedoSmoke();
}
