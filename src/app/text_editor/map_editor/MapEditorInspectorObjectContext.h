#pragma once

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPointF>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

class QGraphicsScene;
class QStandardItemModel;
class QTreeView;

namespace TherionStudio
{
class TextEditorTab;

struct MapEditorInspectorObjectContext
{
    TextEditorTab *textEditor = nullptr;
    QTreeView *objectsTree = nullptr;
    QStandardItemModel *objectsModel = nullptr;
    QGraphicsScene *scene = nullptr;
    QSet<int> *hiddenObjectLines = nullptr;
    QPersistentModelIndex *pressedNameIndex = nullptr;
    bool *pressedWasSelected = nullptr;
    bool *updatingSelection = nullptr;
    int *lastClickedLineNumber = nullptr;
    QSet<int> *suppressedAutoReselectLineNumbers = nullptr;
    bool *commandApplyInProgress = nullptr;
    int *selectedObjectLineNumber = nullptr;
    int *selectedObjectVertexIndex = nullptr;
    QString *selectedObjectKind = nullptr;
    std::optional<QPointF> *selectedObjectCoordinate = nullptr;
    QString *toolbarStatusNote = nullptr;

    std::function<QString(const char *)> translate;
    std::function<QString()> filePath;
    std::function<int()> currentLineNumber;
    std::function<void()> refreshToolbarSummary;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<void()> updateCommandSurfaceState;
    std::function<void()> updateGeometrySelectionPresentation;
    std::function<void(int, int)> syncMapSelectionFromTextCursor;
    std::function<void(const QSet<int> &, bool)> selectMapLines;
    std::function<void(const QString &, const QString &, const QString &, int)> recordSourceTextSnapshot;
};
}
