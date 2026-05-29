#include "MapEditorTab.h"

#include "MapEditorObjectDetailsContext.h"
#include "../TextEditorTab.h"

#include <QTabWidget>

namespace TherionStudio
{
namespace
{
QString normalizedPendingCommandKind(const QString &commandKind)
{
    const QString normalized = commandKind.trimmed().toLower();
    if (normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("point")
        || normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area")) {
        return normalized;
    }
    return QString();
}

QString defaultPendingTypeForCommand(const QString &commandKind)
{
    if (commandKind == QStringLiteral("point")) {
        return QStringLiteral("station");
    }
    if (commandKind == QStringLiteral("line")) {
        return QStringLiteral("wall");
    }
    if (commandKind == QStringLiteral("area")) {
        return QStringLiteral("water");
    }
    return QString();
}

bool pendingPointNameVisible(const QString &type, const QString &name)
{
    return type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0
        || !name.trimmed().isEmpty();
}
}

void MapEditorTab::beginPendingInsertObject(const QString &commandKind)
{
    const QString normalizedCommand = normalizedPendingCommandKind(commandKind);
    if (normalizedCommand.isEmpty()) {
        clearPendingInsertObject();
        return;
    }

    InspectorObjectQuickFields fields;
    fields.commandKind = normalizedCommand;
    fields.typeEditable = normalizedCommand != QStringLiteral("scrap");
    fields.type = defaultPendingTypeForCommand(normalizedCommand);
    if (normalizedCommand == QStringLiteral("scrap")) {
        fields.identifier = QStringLiteral("new-scrap");
    } else if (normalizedCommand == QStringLiteral("point")) {
        fields.name = QStringLiteral("draft-point");
        fields.nameVisible = true;
    }
    interactiveDrawState_.pendingInsertFields_ = fields;
}

void MapEditorTab::clearPendingInsertObject()
{
    interactiveDrawState_.pendingInsertFields_ = InspectorObjectQuickFields{};
}

std::optional<InspectorObjectQuickFields> MapEditorTab::pendingInsertQuickFields() const
{
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().isEmpty()) {
        return std::nullopt;
    }
    return interactiveDrawState_.pendingInsertFields_;
}

void MapEditorTab::setPendingInsertQuickFields(const InspectorObjectQuickFields &fields)
{
    const QString existingCommand = interactiveDrawState_.pendingInsertFields_.commandKind;
    const QString normalizedCommand = normalizedPendingCommandKind(fields.commandKind.isEmpty()
                                                                       ? existingCommand
                                                                       : fields.commandKind);
    if (normalizedCommand.isEmpty()) {
        clearPendingInsertObject();
        return;
    }

    interactiveDrawState_.pendingInsertFields_ = fields;
    interactiveDrawState_.pendingInsertFields_.commandKind = normalizedCommand;
    interactiveDrawState_.pendingInsertFields_.typeEditable = normalizedCommand != QStringLiteral("scrap");
    if (interactiveDrawState_.pendingInsertFields_.type.trimmed().isEmpty()) {
        interactiveDrawState_.pendingInsertFields_.type = defaultPendingTypeForCommand(normalizedCommand);
    }
    if (normalizedCommand == QStringLiteral("point")) {
        if (interactiveDrawState_.pendingInsertFields_.type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) != 0
            && interactiveDrawState_.pendingInsertFields_.name.trimmed() == QStringLiteral("draft-point")) {
            interactiveDrawState_.pendingInsertFields_.name.clear();
        }
        interactiveDrawState_.pendingInsertFields_.nameVisible = pendingPointNameVisible(interactiveDrawState_.pendingInsertFields_.type,
                                                                   interactiveDrawState_.pendingInsertFields_.name);
    } else {
        interactiveDrawState_.pendingInsertFields_.nameVisible = false;
        interactiveDrawState_.pendingInsertFields_.name.clear();
    }
}

TherionDraftObjectOptions MapEditorTab::pendingDraftObjectOptions(const QString &commandKind) const
{
    TherionDraftObjectOptions options;
    const QString normalizedCommand = normalizedPendingCommandKind(commandKind);
    if (normalizedCommand.isEmpty()
        || interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() != normalizedCommand) {
        return options;
    }

    options.type = interactiveDrawState_.pendingInsertFields_.type;
    options.subtype = interactiveDrawState_.pendingInsertFields_.subtype;
    options.identifier = interactiveDrawState_.pendingInsertFields_.identifier;
    if (normalizedCommand == QStringLiteral("point")) {
        options.name = interactiveDrawState_.pendingInsertFields_.name;
        options.nameEnabled = interactiveDrawState_.pendingInsertFields_.nameVisible;
    }
    return options;
}

QString MapEditorTab::pendingScrapPreferredName() const
{
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() != QStringLiteral("scrap")) {
        return QStringLiteral("new-scrap");
    }

    const QString identifier = interactiveDrawState_.pendingInsertFields_.identifier.trimmed();
    return identifier.isEmpty() ? QStringLiteral("new-scrap") : identifier;
}

QString MapEditorTab::pendingScrapOptions(const QString &scaleOption) const
{
    QStringList options;
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() == QStringLiteral("scrap")) {
        const QString projection = interactiveDrawState_.pendingInsertFields_.projection.trimmed();
        if (!projection.isEmpty()) {
            options.append(QStringLiteral("-projection %1").arg(projection));
        }
    }
    if (!scaleOption.trimmed().isEmpty()) {
        options.append(scaleOption.trimmed());
    }
    return options.join(QLatin1Char(' '));
}

void MapEditorTab::activateSelectionInspector()
{
    if (mapInspectorTabs_ != nullptr) {
        mapInspectorTabs_->setCurrentIndex(0);
    }
}

MapEditorObjectDetailsContext MapEditorTab::objectDetailsContext()
{
    return MapEditorObjectDetailsContext{
        .callbackContext = this,
        .textEditor = textEditor_,
        .inspectorSymbolCatalog = &inspectorSymbolCatalog_,
        .orientationApplicabilityByCommand = &orientationApplicabilityByCommand_,
        .updatingUi = &updatingObjectDetailsUi_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .selectedObjectLineNumber = &selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &selectedObjectVertexIndex_,
        .selectedObjectKind = &selectedObjectKind_,
        .objectQuickCommandKind = &objectQuickCommandKind_,
        .hiddenInspectorObjectLines = &hiddenInspectorObjectLines_,
        .lastInspectorClickedObjectLineNumber = &lastInspectorClickedObjectLineNumber_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .selectionLabel = objectDetailsSelectionLabel_,
        .selectionSection = objectSelectionSection_,
        .selectionTitleLabel = objectSelectionTitleLabel_,
        .vertexSection = vertexSelectionSection_,
        .vertexTitleLabel = vertexSelectionTitleLabel_,
        .geometrySection = geometrySelectionSection_,
        .advancedSection = advancedSelectionSection_,
        .deleteButton = objectDeleteButton_,
        .areaReferenceLabel = objectAreaReferenceLabel_,
        .quickFieldsEditor = objectQuickFieldsEditor_,
        .quickIdentifierLabel = objectQuickIdentifierLabel_,
        .quickNameLabel = objectQuickNameLabel_,
        .quickProjectionLabel = objectQuickProjectionLabel_,
        .quickTypeLabel = objectQuickTypeLabel_,
        .quickSubtypeLabel = objectQuickSubtypeLabel_,
        .stylePreviewLabel = objectStylePreviewLabel_,
        .quickTypeCombo = objectQuickTypeCombo_,
        .quickSubtypeCombo = objectQuickSubtypeCombo_,
        .quickProjectionCombo = objectQuickProjectionCombo_,
        .quickIdentifierEdit = objectQuickIdentifierEdit_,
        .quickNameEdit = objectQuickNameEdit_,
        .stylePreview = objectStylePreview_,
        .vertexActionsEditor = vertexActionsEditor_,
        .vertexInsertBeforeButton = vertexInsertBeforeButton_,
        .vertexInsertAfterButton = vertexInsertAfterButton_,
        .vertexDeleteButton = vertexDeleteButton_,
        .vertexSplitButton = vertexSplitButton_,
        .metadataLabel = objectDetailsMetadataLabel_,
        .orientationEditor = objectOrientationEditor_,
        .linePointPreviousControlCheck = linePointPreviousControlCheck_,
        .linePointSmoothCheck = linePointSmoothCheck_,
        .linePointNextControlCheck = linePointNextControlCheck_,
        .orientationEnabledCheck = objectOrientationEnabledCheck_,
        .orientationSpin = objectOrientationSpin_,
        .linePointLeftSizeEnabledCheck = linePointLeftSizeEnabledCheck_,
        .linePointLeftSizeSpin = linePointLeftSizeSpin_,
        .lineOptionsEditor = lineOptionsEditor_,
        .lineClosedCheck = lineClosedCheck_,
        .lineReversedCheck = lineReversedCheck_,
        .scrapScaleEditor = scrapScaleEditor_,
        .scrapScaleSourceX1Spin = scrapScaleSourceX1Spin_,
        .scrapScaleSourceY1Spin = scrapScaleSourceY1Spin_,
        .scrapScaleSourceX2Spin = scrapScaleSourceX2Spin_,
        .scrapScaleSourceY2Spin = scrapScaleSourceY2Spin_,
        .scrapScaleRealX1Spin = scrapScaleRealX1Spin_,
        .scrapScaleRealY1Spin = scrapScaleRealY1Spin_,
        .scrapScaleRealX2Spin = scrapScaleRealX2Spin_,
        .scrapScaleRealY2Spin = scrapScaleRealY2Spin_,
        .scrapScaleUnitCombo = scrapScaleUnitCombo_,
        .configureButton = objectConfigureButton_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .mapSourceBoundsForCurrentDocument = [this]() {
            return mapSourceBoundsForCurrentDocument();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .pendingInsertQuickFields = [this]() -> std::optional<InspectorObjectQuickFields> {
            return pendingInsertQuickFields();
        },
        .setPendingInsertQuickFields = [this](const InspectorObjectQuickFields &fields) {
            setPendingInsertQuickFields(fields);
        },
        .clearInspectorObjectSelection = [this]() {
            clearInspectorObjectSelection();
        },
        .selectMapLine = [this](int lineNumber, bool centerOnSelection) {
            selectMapLine(lineNumber, centerOnSelection);
            syncInspectorObjectSelectionToLine(lineNumber);
        },
        .restorePointSelectionLater = [this](int lineNumber) {
            restorePointSelection(lineNumber);
        },
        .restoreLineAnchorSelectionLater = [this](int lineNumber, int sourceVertexIndex) {
            restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
        },
        .rewriteLineOptionToggle = [this](int lineNumber, const QString &optionName, bool enabled, QString *errorMessage) {
            return rewriteLineOptionToggle(lineNumber, optionName, enabled, errorMessage);
        },
        .recordSourceTextSnapshot = [this](const QString &label, const QString &beforeText, const QString &afterText, int insertedLineNumber) {
            recordSourceTextSnapshot(label, beforeText, afterText, insertedLineNumber);
        },
        .insertLineVertexFromSelection = [this](bool before) {
            return insertLineVertexFromSelection(before);
        },
        .splitLineAtSelection = [this]() {
            return splitLineAtSelection();
        },
        .removeLineVertexFromSelection = [this]() {
            return removeLineVertexFromSelection();
        },
        .toggleLineVertexSmoothFromSelection = [this]() {
            return toggleLineVertexSmoothFromSelection();
        },
        .setLineVertexSmoothForSelection = [this](bool smooth) {
            return setLineVertexSmoothForSelection(smooth);
        },
        .setLineVertexControlHandleForSelection = [this](bool incoming, bool enabled) {
            return setLineVertexControlHandleForSelection(incoming, enabled);
        },
    };
}
}
