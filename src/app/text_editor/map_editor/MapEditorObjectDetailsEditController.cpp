#include "MapEditorObjectDetailsEditController.h"

#include "../TextEditorTab.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentEditor.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLineF>
#include <QScopedValueRollback>

namespace TherionStudio
{
namespace
{
int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        if (lineFeature.lineVertices.at(index).anchorSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}
}

MapEditorObjectDetailsEditController::MapEditorObjectDetailsEditController(MapEditorTab *owner)
    : owner_(owner)
{
}

void MapEditorObjectDetailsEditController::populateScrapScaleFromSourceBounds()
{
    if (owner_->updatingObjectDetailsUi_
        || owner_->scrapScaleSourceX1Spin_ == nullptr
        || owner_->scrapScaleSourceY1Spin_ == nullptr
        || owner_->scrapScaleSourceX2Spin_ == nullptr
        || owner_->scrapScaleSourceY2Spin_ == nullptr
        || owner_->scrapScaleRealX1Spin_ == nullptr
        || owner_->scrapScaleRealY1Spin_ == nullptr
        || owner_->scrapScaleRealX2Spin_ == nullptr
        || owner_->scrapScaleRealY2Spin_ == nullptr
        || owner_->scrapScaleUnitCombo_ == nullptr) {
        return;
    }

    owner_->scrapScaleSourceX1Spin_->setDecimals(0);
    owner_->scrapScaleSourceY1Spin_->setDecimals(0);
    owner_->scrapScaleSourceX2Spin_->setDecimals(0);
    owner_->scrapScaleSourceY2Spin_->setDecimals(0);
    owner_->scrapScaleRealX1Spin_->setDecimals(4);
    owner_->scrapScaleRealY1Spin_->setDecimals(4);
    owner_->scrapScaleRealX2Spin_->setDecimals(4);
    owner_->scrapScaleRealY2Spin_->setDecimals(4);

    const InspectorScrapScale scale = defaultInspectorScrapScale(owner_->mapSourceBoundsForCurrentDocument());
    owner_->scrapScaleSourceX1Spin_->setValue(scale.sourcePoint1.x());
    owner_->scrapScaleSourceY1Spin_->setValue(scale.sourcePoint1.y());
    owner_->scrapScaleSourceX2Spin_->setValue(scale.sourcePoint2.x());
    owner_->scrapScaleSourceY2Spin_->setValue(scale.sourcePoint2.y());
    owner_->scrapScaleRealX1Spin_->setValue(scale.realPoint1.x());
    owner_->scrapScaleRealY1Spin_->setValue(scale.realPoint1.y());
    owner_->scrapScaleRealX2Spin_->setValue(scale.realPoint2.x());
    owner_->scrapScaleRealY2Spin_->setValue(scale.realPoint2.y());
    const int unitIndex = owner_->scrapScaleUnitCombo_->findText(scale.unitToken);
    owner_->scrapScaleUnitCombo_->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
}

void MapEditorObjectDetailsEditController::applyScrapScaleEdits()
{
    if (owner_->updatingObjectDetailsUi_
        || owner_->textEditor_ == nullptr
        || owner_->scrapScaleSourceX1Spin_ == nullptr
        || owner_->scrapScaleSourceY1Spin_ == nullptr
        || owner_->scrapScaleSourceX2Spin_ == nullptr
        || owner_->scrapScaleSourceY2Spin_ == nullptr
        || owner_->scrapScaleRealX1Spin_ == nullptr
        || owner_->scrapScaleRealY1Spin_ == nullptr
        || owner_->scrapScaleRealX2Spin_ == nullptr
        || owner_->scrapScaleRealY2Spin_ == nullptr
        || owner_->scrapScaleUnitCombo_ == nullptr) {
        return;
    }

    int targetLineNumber = 0;
    if (owner_->selectedObjectLineNumber_ > 0 && owner_->selectedObjectKind_ == QStringLiteral("scrap")) {
        targetLineNumber = owner_->selectedObjectLineNumber_;
    } else {
        const int cursorLineNumber = owner_->textEditor_->currentLineNumber();
        QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString &line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
        }
        if (cursorLineNumber > 0 && cursorLineNumber <= lines.size()) {
            const TherionParsedLine parsedLine =
                TherionDocumentParser::parseLine(lines.at(cursorLineNumber - 1), cursorLineNumber);
            if (parsedLine.directive == QStringLiteral("scrap")) {
                targetLineNumber = cursorLineNumber;
            }
        }
    }

    if (targetLineNumber <= 0) {
        owner_->toolbarStatusNote_ = owner_->tr("Select a scrap to edit its scale.");
        owner_->refreshToolbarSummary();
        return;
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(owner_->scrapScaleSourceX1Spin_->value(), owner_->scrapScaleSourceY1Spin_->value());
    scale.sourcePoint2 = QPointF(owner_->scrapScaleSourceX2Spin_->value(), owner_->scrapScaleSourceY2Spin_->value());
    scale.realPoint1 = QPointF(owner_->scrapScaleRealX1Spin_->value(), owner_->scrapScaleRealY1Spin_->value());
    scale.realPoint2 = QPointF(owner_->scrapScaleRealX2Spin_->value(), owner_->scrapScaleRealY2Spin_->value());
    scale.unitToken = owner_->scrapScaleUnitCombo_->currentText().trimmed();
    if (scale.unitToken.isEmpty()) {
        scale.unitToken = QStringLiteral("m");
    }

    if (QLineF(scale.sourcePoint1, scale.sourcePoint2).length() <= 1e-6
        || QLineF(scale.realPoint1, scale.realPoint2).length() <= 1e-6) {
        owner_->toolbarStatusNote_ = owner_->tr("Scrap scale requires two distinct picture points and two distinct real points.");
        owner_->refreshToolbarSummary();
        return;
    }

    const QString beforeText = owner_->textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteScrapScale(&afterText,
                                                  targetLineNumber,
                                                  scrapScaleExpression(scale),
                                                  &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Failed to update scrap scale.")
            : owner_->tr("Failed to update scrap scale: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
    owner_->textEditor_->replaceTextForCommand(afterText);
    owner_->recordSourceTextSnapshot(owner_->tr("Set Scrap Scale"), beforeText, afterText, targetLineNumber);
    owner_->toolbarStatusNote_ = owner_->tr("Updated scrap scale.");
    owner_->refreshToolbarSummary();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleConfigureObjectSettingsTriggered()
{
    if (owner_->textEditor_ == nullptr) {
        return;
    }

    int targetLineNumber = owner_->selectedObjectLineNumber_;
    QString targetKind = owner_->selectedObjectKind_.trimmed().toLower();
    if (!(targetLineNumber > 0 && isConfigurableMapObjectKind(targetKind))) {
        targetLineNumber = owner_->textEditor_->currentLineNumber();
        if (targetLineNumber > 0) {
            QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
            for (QString &line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
            if (targetLineNumber <= lines.size()) {
                const TherionParsedLine parsedLine =
                    TherionDocumentParser::parseLine(lines.at(targetLineNumber - 1), targetLineNumber);
                targetKind = objectKindForDirective(parsedLine.directive);
            }
        }
    }

    if (targetLineNumber <= 0 || !isConfigurableMapObjectKind(targetKind)) {
        owner_->toolbarStatusNote_ = owner_->tr("Object settings are available for scrap, point, line, and area commands.");
        owner_->refreshToolbarSummary();
        return;
    }

    owner_->textEditor_->configureCommandAtLine(targetKind, targetLineNumber);
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLineClosedToggled(bool checked)
{
    if (owner_->updatingObjectDetailsUi_
        || owner_->textEditor_ == nullptr
        || owner_->selectedObjectLineNumber_ <= 0
        || owner_->selectedObjectKind_ != QStringLiteral("line")) {
        return;
    }

    QString errorMessage;
    if (!owner_->rewriteLineOptionToggle(owner_->selectedObjectLineNumber_, QStringLiteral("close"), checked, &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Failed to update line closed state.")
            : owner_->tr("Failed to update line closed state: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        owner_->refreshObjectDetailsPanel();
    }
}

void MapEditorObjectDetailsEditController::handleLineReversedToggled(bool checked)
{
    if (owner_->updatingObjectDetailsUi_
        || owner_->textEditor_ == nullptr
        || owner_->selectedObjectLineNumber_ <= 0
        || owner_->selectedObjectKind_ != QStringLiteral("line")) {
        return;
    }

    QString errorMessage;
    if (!owner_->rewriteLineOptionToggle(owner_->selectedObjectLineNumber_, QStringLiteral("reverse"), checked, &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Failed to update line reverse state.")
            : owner_->tr("Failed to update line reverse state: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        owner_->refreshObjectDetailsPanel();
    }
}

void MapEditorObjectDetailsEditController::applyObjectOrientationEdits()
{
    if (owner_->updatingObjectDetailsUi_ || owner_->textEditor_ == nullptr || owner_->selectedObjectLineNumber_ <= 0) {
        return;
    }
    if (owner_->objectOrientationEnabledCheck_ == nullptr
        || owner_->objectOrientationSpin_ == nullptr
        || owner_->linePointLeftSizeEnabledCheck_ == nullptr
        || owner_->linePointLeftSizeSpin_ == nullptr) {
        return;
    }
    if (owner_->selectedObjectKind_ != QStringLiteral("point") && owner_->selectedObjectKind_ != QStringLiteral("line")) {
        return;
    }

    const bool enabled = owner_->objectOrientationEnabledCheck_->isChecked();
    const qreal orientation = normalizeOrientationDegreesForMapDetails(owner_->objectOrientationSpin_->value());
    const bool leftSizeEnabled = owner_->linePointLeftSizeEnabledCheck_->isVisible()
        && owner_->linePointLeftSizeEnabledCheck_->isChecked();
    const qreal leftSize = qMax<qreal>(0.1, owner_->linePointLeftSizeSpin_->value());
    QStringList documentLines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : documentLines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    QString errorMessage;
    bool rewritten = false;
    if (owner_->selectedObjectKind_ == QStringLiteral("point")) {
        if (owner_->selectedObjectLineNumber_ > documentLines.size()) {
            return;
        }
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(documentLines.at(owner_->selectedObjectLineNumber_ - 1), owner_->selectedObjectLineNumber_);
        if (!isOrientationSupportedForParsedLine(parsedLine)) {
            owner_->toolbarStatusNote_ = owner_->tr("Orientation is not supported for this point type.");
            owner_->refreshToolbarSummary();
            owner_->refreshObjectDetailsPanel();
            return;
        }
        rewritten = owner_->textEditor_->rewritePointOrientation(owner_->selectedObjectLineNumber_,
                                                         enabled,
                                                         orientation,
                                                         &errorMessage);
    } else {
        if (owner_->selectedObjectVertexIndex_ < 0) {
            return;
        }

        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), owner_->selectedObjectLineNumber_);
        if (!lineFeature.has_value()
            || lineFeature->kind != MapGeometryFeature::Kind::Line
            || lineVertexIndexForSourceVertex(lineFeature.value(), owner_->selectedObjectVertexIndex_) < 0) {
            owner_->toolbarStatusNote_ = owner_->tr("Orientation editing is available only for selected line anchor vertices.");
            owner_->refreshToolbarSummary();
            owner_->refreshObjectDetailsPanel();
            return;
        }
        if (owner_->selectedObjectLineNumber_ > documentLines.size()) {
            return;
        }
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(documentLines.at(owner_->selectedObjectLineNumber_ - 1), owner_->selectedObjectLineNumber_);
        const bool orientationSupported = isOrientationSupportedForParsedLine(parsedLine);
        const bool leftSizeSupported = isLinePointLeftSizeSupportedForParsedLine(parsedLine);
        if (!orientationSupported && !leftSizeSupported) {
            owner_->toolbarStatusNote_ = owner_->tr("Orientation is not supported for this line type.");
            owner_->refreshToolbarSummary();
            owner_->refreshObjectDetailsPanel();
            return;
        }

        rewritten = true;
        if (orientationSupported) {
            rewritten = owner_->textEditor_->rewriteLinePointOrientation(owner_->selectedObjectLineNumber_,
                                                                 owner_->selectedObjectVertexIndex_,
                                                                 enabled,
                                                                 orientation,
                                                                 &errorMessage);
        }
        if (rewritten && leftSizeSupported) {
            rewritten = owner_->textEditor_->rewriteLinePointLeftSize(owner_->selectedObjectLineNumber_,
                                                             owner_->selectedObjectVertexIndex_,
                                                             leftSizeEnabled,
                                                             leftSize,
                                                             &errorMessage);
        }
    }

    if (!rewritten) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Failed to update orientation.")
            : owner_->tr("Failed to update orientation: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        owner_->refreshObjectDetailsPanel();
        return;
    }

    if (owner_->selectedObjectKind_ == QStringLiteral("line")) {
        owner_->toolbarStatusNote_ = owner_->tr("Updated line point options.");
    } else {
        owner_->toolbarStatusNote_ = enabled
            ? owner_->tr("Updated orientation to %1 degrees.").arg(QString::number(orientation, 'f', 3))
            : owner_->tr("Cleared orientation override.");
    }
    owner_->refreshToolbarSummary();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleObjectOrientationEnabledToggled(bool checked)
{
    if (owner_->updatingObjectDetailsUi_ || owner_->objectOrientationSpin_ == nullptr) {
        return;
    }
    owner_->objectOrientationSpin_->setEnabled(checked);
}

void MapEditorObjectDetailsEditController::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    if (owner_->updatingObjectDetailsUi_ || owner_->linePointLeftSizeSpin_ == nullptr) {
        return;
    }
    owner_->linePointLeftSizeSpin_->setEnabled(checked);
    if (checked && owner_->linePointLeftSizeSpin_->value() <= 0.0) {
        owner_->linePointLeftSizeSpin_->setValue(40.0);
    }
}

void MapEditorObjectDetailsEditController::deleteSelectedObjectFromSelection()
{
    if (owner_->textEditor_ == nullptr) {
        return;
    }

    int targetLineNumber = owner_->selectedObjectLineNumber_;
    if (targetLineNumber <= 0) {
        targetLineNumber = owner_->textEditor_->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    if (owner_->textEditor_->deleteCommandAtLine(targetLineNumber)) {
        owner_->hiddenInspectorObjectLines_.remove(targetLineNumber);
        owner_->lastInspectorClickedObjectLineNumber_ = 0;
        owner_->clearInspectorObjectSelection();
        owner_->toolbarStatusNote_ = owner_->tr("Deleted selected object from source.");
        owner_->refreshToolbarSummary();
    }
}

void MapEditorObjectDetailsEditController::applyObjectQuickFieldEdits()
{
    if (owner_->updatingObjectDetailsUi_
        || owner_->textEditor_ == nullptr
        || owner_->objectQuickTypeCombo_ == nullptr
        || owner_->objectQuickSubtypeCombo_ == nullptr
        || owner_->objectQuickIdentifierEdit_ == nullptr
        || owner_->objectQuickNameEdit_ == nullptr) {
        return;
    }

    int targetLineNumber = owner_->selectedObjectLineNumber_;
    if (targetLineNumber <= 0) {
        targetLineNumber = owner_->textEditor_->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QString beforeText = owner_->textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteMapObjectQuickFields(&afterText,
                                                            targetLineNumber,
                                                            owner_->objectQuickTypeCombo_->currentText(),
                                                            owner_->objectQuickSubtypeCombo_->currentText(),
                                                            owner_->objectQuickIdentifierEdit_->text(),
                                                            owner_->objectQuickNameEdit_->text(),
                                                            owner_->objectQuickNameEdit_->isVisible(),
                                                            &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Failed to update object fields.")
            : owner_->tr("Failed to update object fields: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        owner_->refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
    owner_->textEditor_->replaceTextForCommand(afterText);
    owner_->recordSourceTextSnapshot(owner_->tr("Edit Object Fields"), beforeText, afterText, targetLineNumber);
    owner_->toolbarStatusNote_ = owner_->tr("Updated object fields.");
    owner_->refreshToolbarSummary();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::applyScrapProjectionEdit()
{
    if (owner_->updatingObjectDetailsUi_
        || owner_->textEditor_ == nullptr
        || owner_->objectQuickProjectionCombo_ == nullptr) {
        return;
    }

    int targetLineNumber = owner_->selectedObjectLineNumber_;
    if (targetLineNumber <= 0) {
        targetLineNumber = owner_->textEditor_->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QString beforeText = owner_->textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteScrapProjection(&afterText,
                                                       targetLineNumber,
                                                       owner_->objectQuickProjectionCombo_->currentText(),
                                                       &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Failed to update scrap projection.")
            : owner_->tr("Failed to update scrap projection: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        owner_->refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
    owner_->textEditor_->replaceTextForCommand(afterText);
    owner_->recordSourceTextSnapshot(owner_->tr("Edit Scrap Projection"), beforeText, afterText, targetLineNumber);
    owner_->toolbarStatusNote_ = owner_->tr("Updated scrap projection.");
    owner_->refreshToolbarSummary();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::updateObjectQuickSubtypeChoices()
{
    if (owner_->objectQuickSubtypeCombo_ == nullptr || owner_->objectQuickTypeCombo_ == nullptr) {
        return;
    }

    const QString currentSubtype = owner_->objectQuickSubtypeCombo_->currentText();
    setEditableComboValues(owner_->objectQuickSubtypeCombo_,
                           inspectorSubtypeValuesForCommandType(owner_->objectQuickCommandKind_, owner_->objectQuickTypeCombo_->currentText()),
                           currentSubtype);
}

void MapEditorObjectDetailsEditController::insertVertexFromSelectionPanel()
{
    owner_->insertLineVertexFromSelection();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::deleteVertexFromSelectionPanel()
{
    owner_->removeLineVertexFromSelection();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::toggleVertexSmoothFromSelectionPanel()
{
    owner_->toggleLineVertexSmoothFromSelection();
    owner_->refreshObjectDetailsPanel();
}
}
