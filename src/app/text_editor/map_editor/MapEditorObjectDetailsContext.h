#pragma once

#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace TherionStudio
{
class TextEditorTab;

struct MapEditorObjectDetailsContext
{
    TextEditorTab *textEditor = nullptr;
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
    QWidget *geometrySection = nullptr;
    QWidget *advancedSection = nullptr;
    QPushButton *deleteButton = nullptr;
    QLabel *areaReferenceLabel = nullptr;
    QWidget *quickFieldsEditor = nullptr;
    QLabel *quickIdentifierLabel = nullptr;
    QLabel *quickNameLabel = nullptr;
    QLabel *quickProjectionLabel = nullptr;
    QLabel *quickTypeLabel = nullptr;
    QLabel *quickSubtypeLabel = nullptr;
    QComboBox *quickTypeCombo = nullptr;
    QComboBox *quickSubtypeCombo = nullptr;
    QComboBox *quickProjectionCombo = nullptr;
    QLineEdit *quickIdentifierEdit = nullptr;
    QLineEdit *quickNameEdit = nullptr;
    QWidget *vertexActionsEditor = nullptr;
    QPushButton *vertexInsertButton = nullptr;
    QPushButton *vertexDeleteButton = nullptr;
    QPushButton *vertexToggleSmoothButton = nullptr;
    QLabel *metadataLabel = nullptr;
    QWidget *orientationEditor = nullptr;
    QCheckBox *orientationEnabledCheck = nullptr;
    QDoubleSpinBox *orientationSpin = nullptr;
    QCheckBox *linePointLeftSizeEnabledCheck = nullptr;
    QDoubleSpinBox *linePointLeftSizeSpin = nullptr;
    QPushButton *orientationApplyButton = nullptr;
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
    std::function<void()> clearInspectorObjectSelection;
    std::function<void(int, bool)> selectMapLine;
    std::function<bool(int, const QString &, bool, QString *)> rewriteLineOptionToggle;
    std::function<void(const QString &, const QString &, const QString &, int)> recordSourceTextSnapshot;
    std::function<bool()> insertLineVertexFromSelection;
    std::function<bool()> removeLineVertexFromSelection;
    std::function<bool()> toggleLineVertexSmoothFromSelection;
};
}
