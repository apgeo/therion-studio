#pragma once

#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>

#include <functional>

#include "MapEditorInspectorData.h"
#include "MapEditorObjectDetailsLogic.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QWidget;

namespace TherionStudio
{
class MapEditorStylePreviewWidget;
class TextEditorTab;

struct MapEditorObjectDetailsContext
{
    QObject *callbackContext = nullptr;
    TextEditorTab *textEditor = nullptr;
    const InspectorSymbolCatalog *inspectorSymbolCatalog = nullptr;
    const MapEditorOrientationApplicabilityByCommand *orientationApplicabilityByCommand = nullptr;
    bool *updatingUi = nullptr;
    bool *commandApplyInProgress = nullptr;
    int *selectedObjectLineNumber = nullptr;
    int *selectedObjectVertexIndex = nullptr;
    QString *selectedObjectKind = nullptr;
    QString *objectQuickCommandKind = nullptr;
    QSet<int> *hiddenInspectorObjectLines = nullptr;
    int *lastInspectorClickedObjectLineNumber = nullptr;
    QString *toolbarStatusNote = nullptr;

    QLabel *selectionLabel = nullptr;
    QWidget *selectionSection = nullptr;
    QLabel *selectionTitleLabel = nullptr;
    QWidget *vertexSection = nullptr;
    QLabel *vertexTitleLabel = nullptr;
    QWidget *geometrySection = nullptr;
    QWidget *advancedSection = nullptr;
    QPushButton *deleteButton = nullptr;
    QLabel *areaReferenceLabel = nullptr;
    QWidget *quickFieldsEditor = nullptr;
    QLabel *quickIdentifierLabel = nullptr;
    QLabel *quickNameLabel = nullptr;
    QLabel *quickTextLabel = nullptr;
    QLabel *quickProjectionLabel = nullptr;
    QLabel *quickTypeLabel = nullptr;
    QLabel *quickSubtypeLabel = nullptr;
    QLabel *stylePreviewLabel = nullptr;
    QComboBox *quickTypeCombo = nullptr;
    QComboBox *quickSubtypeCombo = nullptr;
    QComboBox *quickProjectionCombo = nullptr;
    QLineEdit *quickIdentifierEdit = nullptr;
    QLineEdit *quickNameEdit = nullptr;
    QLineEdit *quickTextEdit = nullptr;
    MapEditorStylePreviewWidget *stylePreview = nullptr;
    QWidget *vertexActionsEditor = nullptr;
    QPushButton *vertexInsertBeforeButton = nullptr;
    QPushButton *vertexInsertAfterButton = nullptr;
    QPushButton *vertexDeleteButton = nullptr;
    QPushButton *vertexSplitButton = nullptr;
    QLabel *metadataLabel = nullptr;
    QWidget *orientationEditor = nullptr;
    QCheckBox *linePointPreviousControlCheck = nullptr;
    QCheckBox *linePointSmoothCheck = nullptr;
    QCheckBox *linePointNextControlCheck = nullptr;
    QCheckBox *orientationEnabledCheck = nullptr;
    QDoubleSpinBox *orientationSpin = nullptr;
    QCheckBox *linePointLeftSizeEnabledCheck = nullptr;
    QDoubleSpinBox *linePointLeftSizeSpin = nullptr;
    QWidget *lineOptionsEditor = nullptr;
    QCheckBox *lineClosedCheck = nullptr;
    QCheckBox *lineReversedCheck = nullptr;
    QWidget *scrapScaleEditor = nullptr;
    QDoubleSpinBox *scrapScaleSourceX1Spin = nullptr;
    QDoubleSpinBox *scrapScaleSourceY1Spin = nullptr;
    QDoubleSpinBox *scrapScaleSourceX2Spin = nullptr;
    QDoubleSpinBox *scrapScaleSourceY2Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealX1Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealY1Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealX2Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealY2Spin = nullptr;
    QComboBox *scrapScaleUnitCombo = nullptr;
    QPushButton *configureButton = nullptr;

    std::function<QString(const char *)> translate;
    std::function<QRectF()> mapSourceBoundsForCurrentDocument;
    std::function<void()> refreshToolbarSummary;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<std::optional<InspectorObjectQuickFields>()> pendingInsertQuickFields;
    std::function<void(const InspectorObjectQuickFields &)> setPendingInsertQuickFields;
    std::function<void()> clearInspectorObjectSelection;
    std::function<void(int, bool)> selectMapLine;
    std::function<void(int)> restorePointSelectionLater;
    std::function<void(int, int)> restoreLineAnchorSelectionLater;
    std::function<bool(int, const QString &, bool, QString *)> rewriteLineOptionToggle;
    std::function<void(const QString &, const QString &, const QString &, int)> recordSourceTextSnapshot;
    std::function<bool(bool)> insertLineVertexFromSelection;
    std::function<bool()> splitLineAtSelection;
    std::function<bool()> removeLineVertexFromSelection;
    std::function<bool()> toggleLineVertexSmoothFromSelection;
    std::function<bool(bool)> setLineVertexSmoothForSelection;
    std::function<bool(bool, bool)> setLineVertexControlHandleForSelection;
};
}
