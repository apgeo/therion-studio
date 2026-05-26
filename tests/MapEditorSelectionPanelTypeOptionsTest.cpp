#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMainWindow>
#include <QTemporaryDir>
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

int runSelectionPanelTypeValuesTest()
{
    const QStringList directAreaTypes = inspectorTypeValuesForCommand(QStringLiteral("area"));
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

    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
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

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runSelectionPanelTypeValuesTest();
}
