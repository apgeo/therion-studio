#include "MapEditorTab.h"

#include <QFrame>
#include <QEvent>
#include <QCursor>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitterHandle>
#include <QTabletEvent>
#include <QTextBrowser>
#include <QPointer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QFont>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <optional>

#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorInputPolicy.h"
#include "TextEditorTab.h"
#include "../core/TherionDocumentParser.h"

namespace TherionStudio
{
namespace
{
constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;

bool sourcePointsDifferForCommands(const QPointF &a, const QPointF &b)
{
    return (a - b).manhattanLength() > 0.01;
}

bool wheelEventHasPreciseScrollingDeltas(const QWheelEvent *event)
{
    if (event == nullptr) {
        return false;
    }

    if (!event->pixelDelta().isNull()) {
        return true;
    }

    return event->phase() != Qt::NoScrollPhase;
}

bool isInteractiveMapSelectionItem(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return false;
    }
    if (!item->isVisible()) {
        return false;
    }
    if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
        return false;
    }
    if (item->acceptedMouseButtons() == Qt::NoButton) {
        return false;
    }
    return true;
}

int mapSelectionHitPriority(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return 100;
    }

    if (dynamic_cast<const MapEditablePointItem *>(item) != nullptr) {
        return 0;
    }
    if (auto *vertexItem = dynamic_cast<const MapEditableGeometryVertexItem *>(item)) {
        const QString geometryKind = vertexItem->geometryKind().trimmed().toLower();
        if (geometryKind == QStringLiteral("line") || geometryKind == QStringLiteral("area")) {
            return 0;
        }
        if (geometryKind.startsWith(QStringLiteral("line control"))) {
            return 2;
        }
    }

    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    switch (subtype) {
    case kMapSceneSelectionSubtypeLineAnchor:
    case kMapSceneSelectionSubtypeAreaVertex:
        return 0;
    case kMapSceneSelectionSubtypeLineControl:
        return 2;
    case kMapSceneSelectionSubtypeLineControlConnector:
        return 3;
    case kMapSceneSelectionSubtypeLineDetail:
        return 4;
    default:
        break;
    }

    return 1;
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems, bool requireSelected = false)
{
    QGraphicsItem *bestItem = nullptr;
    int bestPriority = std::numeric_limits<int>::max();
    for (QGraphicsItem *item : hitItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        if (requireSelected && !item->isSelected()) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }

        const int priority = mapSelectionHitPriority(item);
        if (priority < bestPriority) {
            bestPriority = priority;
            bestItem = item;
            if (bestPriority == 0) {
                break;
            }
        }
    }
    return bestItem;
}

class MapPointGeometryMoveCommand final : public QUndoCommand
{
public:
    static constexpr int kCommandId = 0x4d50474d; // MPGM

    MapPointGeometryMoveCommand(TextEditorTab *textEditor,
                                int lineNumber,
                                const QPointF &oldPoint,
                                const QPointF &newPoint,
                                std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , lineNumber_(lineNumber)
        , oldPoint_(oldPoint)
        , newPoint_(newPoint)
        , statusCallback_(std::move(statusCallback))
    {
        if (textEditor_ != nullptr) {
            beforeTextSnapshot_ = textEditor_->text();
        }
    }

    int id() const override
    {
        return kCommandId;
    }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *otherCommand = dynamic_cast<const MapPointGeometryMoveCommand *>(other);
        if (otherCommand == nullptr
            || textEditor_ != otherCommand->textEditor_
            || lineNumber_ != otherCommand->lineNumber_) {
            return false;
        }

        newPoint_ = otherCommand->newPoint_;
        return true;
    }

    void undo() override
    {
        if (textEditor_ == nullptr || beforeTextSnapshot_.isNull()) {
            setObsolete(true);
            return;
        }

        textEditor_->replaceTextForCommand(beforeTextSnapshot_);
        if (statusCallback_ != nullptr) {
            statusCallback_(QStringLiteral("Reverted point geometry at source line %1.").arg(lineNumber_));
        }
    }

    void redo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        if (afterTextSnapshot_.has_value()) {
            textEditor_->replaceTextForCommand(afterTextSnapshot_.value());
            if (statusCallback_ != nullptr) {
                statusCallback_(QStringLiteral("Updated point geometry at source line %1.").arg(lineNumber_));
            }
            return;
        }

        if (!applyPoint(newPoint_, QStringLiteral("Updated point geometry at source line %1.").arg(lineNumber_))) {
            setObsolete(true);
            return;
        }

        afterTextSnapshot_ = textEditor_->text();
    }

private:
    bool applyPoint(const QPointF &point, const QString &successMessage)
    {
        if (textEditor_ == nullptr) {
            return false;
        }

        QString errorMessage;
        if (!textEditor_->rewritePointCoordinates(lineNumber_, point, &errorMessage)) {
            if (statusCallback_ != nullptr) {
                statusCallback_(errorMessage.isEmpty()
                                    ? QStringLiteral("Point move failed.")
                                    : QStringLiteral("Point move failed: %1").arg(errorMessage));
            }
            return false;
        }

        if (statusCallback_ != nullptr) {
            statusCallback_(successMessage);
        }
        return true;
    }

    QPointer<TextEditorTab> textEditor_;
    int lineNumber_ = 0;
    QPointF oldPoint_;
    QPointF newPoint_;
    std::function<void(const QString &)> statusCallback_;
    QString beforeTextSnapshot_;
    std::optional<QString> afterTextSnapshot_;
};

class MapLineAreaVertexMoveCommand final : public QUndoCommand
{
public:
    static constexpr int kCommandId = 0x4d4c564d; // MLVM
    struct SecondaryMove
    {
        int vertexIndex = -1;
        QPointF oldPoint;
        QPointF newPoint;
    };

    MapLineAreaVertexMoveCommand(TextEditorTab *textEditor,
                                 int lineNumber,
                                 QString kind,
                                 int vertexIndex,
                                 const QPointF &oldPoint,
                                 const QPointF &newPoint,
                                 QVector<SecondaryMove> secondaryMoves,
                                 std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , lineNumber_(lineNumber)
        , kind_(std::move(kind))
        , vertexIndex_(vertexIndex)
        , oldPoint_(oldPoint)
        , newPoint_(newPoint)
        , secondaryMoves_(std::move(secondaryMoves))
        , statusCallback_(std::move(statusCallback))
    {
        if (textEditor_ != nullptr) {
            beforeTextSnapshot_ = textEditor_->text();
        }
    }

    int id() const override
    {
        return kCommandId;
    }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *otherCommand = dynamic_cast<const MapLineAreaVertexMoveCommand *>(other);
        if (otherCommand == nullptr
            || textEditor_ != otherCommand->textEditor_
            || lineNumber_ != otherCommand->lineNumber_
            || kind_ != otherCommand->kind_
            || vertexIndex_ != otherCommand->vertexIndex_
            || !secondaryMoves_.isEmpty()
            || !otherCommand->secondaryMoves_.isEmpty()) {
            return false;
        }

        newPoint_ = otherCommand->newPoint_;
        return true;
    }

    void undo() override
    {
        if (textEditor_ == nullptr || beforeTextSnapshot_.isNull()) {
            setObsolete(true);
            return;
        }

        textEditor_->replaceTextForCommand(beforeTextSnapshot_);
        if (statusCallback_ != nullptr) {
            statusCallback_(QStringLiteral("Reverted %1 vertex %2 at source line %3.")
                                .arg(kind_)
                                .arg(vertexIndex_ + 1)
                                .arg(lineNumber_));
        }
    }

    void redo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        if (afterTextSnapshot_.has_value()) {
            textEditor_->replaceTextForCommand(afterTextSnapshot_.value());
            if (statusCallback_ != nullptr) {
                statusCallback_(QStringLiteral("Updated %1 vertex %2 at source line %3.")
                                    .arg(kind_)
                                    .arg(vertexIndex_ + 1)
                                    .arg(lineNumber_));
            }
            return;
        }

        if (!applyVertexWithSecondaries(newPoint_,
                                        secondaryMoves_,
                                        QStringLiteral("Updated %1 vertex %2 at source line %3.")
                                            .arg(kind_)
                                            .arg(vertexIndex_ + 1)
                                            .arg(lineNumber_))) {
            setObsolete(true);
            return;
        }

        afterTextSnapshot_ = textEditor_->text();
    }

private:
    bool applyVertexWithSecondaries(const QPointF &point,
                                    const QVector<SecondaryMove> &secondaryMoves,
                                    const QString &successMessage)
    {
        if (textEditor_ == nullptr) {
            return false;
        }

        QString errorMessage;
        if (!textEditor_->rewriteLineAreaVertex(lineNumber_, kind_, vertexIndex_, point, &errorMessage)) {
            if (statusCallback_ != nullptr) {
                statusCallback_(errorMessage.isEmpty()
                                    ? QStringLiteral("%1 vertex move failed.").arg(kind_)
                                    : QStringLiteral("%1 vertex move failed: %2").arg(kind_, errorMessage));
            }
            return false;
        }

        for (const SecondaryMove &move : secondaryMoves) {
            if (move.vertexIndex < 0) {
                continue;
            }

            if (!textEditor_->rewriteLineAreaVertex(lineNumber_, kind_, move.vertexIndex, move.newPoint, &errorMessage)) {
                if (statusCallback_ != nullptr) {
                    statusCallback_(errorMessage.isEmpty()
                                        ? QStringLiteral("%1 coupled vertex move failed.").arg(kind_)
                                        : QStringLiteral("%1 coupled vertex move failed: %2").arg(kind_, errorMessage));
                }
                return false;
            }
        }

        if (statusCallback_ != nullptr) {
            statusCallback_(successMessage);
        }
        return true;
    }

    QPointer<TextEditorTab> textEditor_;
    int lineNumber_ = 0;
    QString kind_;
    int vertexIndex_ = -1;
    QPointF oldPoint_;
    QPointF newPoint_;
    QVector<SecondaryMove> secondaryMoves_;
    std::function<void(const QString &)> statusCallback_;
    QString beforeTextSnapshot_;
    std::optional<QString> afterTextSnapshot_;
};

struct CoupledLineMove
{
    QVector<MapLineAreaVertexMoveCommand::SecondaryMove> secondaryMoves;
};

std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QString &documentText, int lineNumber)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(documentText);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line
            && feature.lineNumber == lineNumber
            && !feature.lineVertices.isEmpty()) {
            return feature;
        }
    }

    return std::nullopt;
}

CoupledLineMove coupledMovesForLineVertex(const MapGeometryFeature &lineFeature,
                                          int sourceVertexIndex,
                                          const QPointF &oldPoint,
                                          const QPointF &newPoint)
{
    CoupledLineMove result;
    const QVector<MapLineSecondaryMove> moves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                       sourceVertexIndex,
                                                                                       oldPoint,
                                                                                       newPoint);
    result.secondaryMoves.reserve(moves.size());
    for (const MapLineSecondaryMove &sourceMove : moves) {
        MapLineAreaVertexMoveCommand::SecondaryMove move;
        move.vertexIndex = sourceMove.sourceVertexIndex;
        move.oldPoint = sourceMove.oldPoint;
        move.newPoint = sourceMove.newPoint;
        result.secondaryMoves.append(move);
    }
    return result;
}

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

int lineVertexOwnerIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &vertex = lineFeature.lineVertices.at(index);
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex
            || vertex.incomingSourceVertexIndex == sourceVertexIndex
            || vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

QString formatSourceCoordinate(qreal value)
{
    return QString::number(value, 'f', 1);
}

QStringList coordinateRowsForLineVertices(const QVector<MapGeometryFeature::TH2LineVertex> &lineVertices)
{
    QStringList rows;
    if (lineVertices.size() < 2) {
        return rows;
    }

    const auto pointRow = [](const QPointF &point) {
        return QStringLiteral("%1 %2")
            .arg(formatSourceCoordinate(point.x()), formatSourceCoordinate(point.y()));
    };

    rows.append(pointRow(lineVertices.first().anchor));
    if (!lineVertices.first().isSmooth) {
        rows.append(QStringLiteral("smooth off"));
    }

    for (int index = 1; index < lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previous = lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &current = lineVertices.at(index);
        QStringList rowTokens;
        const bool cubic = previous.outgoingControl.has_value() || current.incomingControl.has_value();
        const auto appendPointTokens = [&rowTokens](const QPointF &point) {
            rowTokens.append(formatSourceCoordinate(point.x()));
            rowTokens.append(formatSourceCoordinate(point.y()));
        };
        if (cubic) {
            appendPointTokens(previous.outgoingControl.value_or(previous.anchor));
            appendPointTokens(current.incomingControl.value_or(current.anchor));
        }
        appendPointTokens(current.anchor);
        rows.append(rowTokens.join(QLatin1Char(' ')));
        if (!current.isSmooth) {
            rows.append(QStringLiteral("smooth off"));
        }
    }
    return rows;
}

struct SourceVertexTextReference
{
    int lineNumber = 0;
    int sourceVertexIndex = -1;
    int xStartColumn = 1;
    int xEndColumn = 1;
    int yStartColumn = 1;
    int yEndColumn = 1;
};

struct CursorGeometrySelection
{
    int featureLineNumber = 0;
    QString geometryKind;
    std::optional<SourceVertexTextReference> sourceVertexReference;
};

bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(token).hasMatch();
}

QPair<int, int> tokenColumnsForParsedLine(const TherionParsedLine &parsedLine, int tokenIndex)
{
    if (tokenIndex < 0 || tokenIndex >= parsedLine.tokenSpans.size()) {
        return qMakePair(1, 1);
    }

    const TherionParsedToken &span = parsedLine.tokenSpans.at(tokenIndex);
    const int startColumn = qMax(1, span.start + 1);
    const int endColumn = qMax(startColumn, span.start + qMax(1, span.length));
    return qMakePair(startColumn, endColumn);
}

QVector<int> coordinateTokenIndicesFromLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<int> numericIndices;
    const int firstTokenIndex = qMax(0, startTokenIndex);
    if (firstTokenIndex >= parsedLine.tokens.size()) {
        return numericIndices;
    }

    if (firstTokenIndex == 0) {
        int firstNonQuotedIndex = -1;
        for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
            if (index < parsedLine.tokenSpans.size()
                && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
                continue;
            }
            firstNonQuotedIndex = index;
            break;
        }

        if (firstNonQuotedIndex >= 0 && !tokenLooksNumeric(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return numericIndices;
        }
    }

    const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
    if (firstTokenIndex == 0
        && firstToken.startsWith(QLatin1Char('-'))
        && !tokenLooksNumeric(firstToken)) {
        return numericIndices;
    }

    bool sawCoordinateToken = false;
    bool skipOptionValueToken = false;
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (skipOptionValueToken && !sawCoordinateToken) {
            skipOptionValueToken = false;
            continue;
        }

        const QString token = parsedLine.tokens.at(index);
        if (!sawCoordinateToken
            && firstTokenIndex > 0
            && token.startsWith(QLatin1Char('-'))
            && !tokenLooksNumeric(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!nextToken.startsWith(QLatin1Char('-')) || tokenLooksNumeric(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        if (!tokenLooksNumeric(token)) {
            if (sawCoordinateToken) {
                break;
            }
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        numericIndices.append(index);
        sawCoordinateToken = true;
    }

    return numericIndices;
}

QVector<SourceVertexTextReference> lineSourceVertexReferencesFromParsedLine(const TherionParsedLine &parsedLine,
                                                                            int startTokenIndex,
                                                                            int *nextSourceVertexIndex)
{
    QVector<SourceVertexTextReference> references;
    if (nextSourceVertexIndex == nullptr) {
        return references;
    }

    const QVector<int> numericIndices = coordinateTokenIndicesFromLine(parsedLine, startTokenIndex);
    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        SourceVertexTextReference reference;
        reference.lineNumber = parsedLine.lineNumber;
        reference.sourceVertexIndex = *nextSourceVertexIndex;
        ++(*nextSourceVertexIndex);
        const auto xColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index));
        const auto yColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index + 1));
        reference.xStartColumn = xColumns.first;
        reference.xEndColumn = xColumns.second;
        reference.yStartColumn = yColumns.first;
        reference.yEndColumn = yColumns.second;
        references.append(reference);
    }

    return references;
}

QVector<SourceVertexTextReference> areaSourceVertexReferencesFromParsedLine(const TherionParsedLine &parsedLine,
                                                                            int startTokenIndex,
                                                                            int *nextSourceVertexIndex)
{
    QVector<SourceVertexTextReference> references;
    if (nextSourceVertexIndex == nullptr) {
        return references;
    }

    QVector<int> numericIndices;
    for (int index = qMax(0, startTokenIndex); index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (!tokenLooksNumeric(token)) {
            continue;
        }
        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }
        numericIndices.append(index);
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        SourceVertexTextReference reference;
        reference.lineNumber = parsedLine.lineNumber;
        reference.sourceVertexIndex = *nextSourceVertexIndex;
        ++(*nextSourceVertexIndex);
        const auto xColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index));
        const auto yColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index + 1));
        reference.xStartColumn = xColumns.first;
        reference.xEndColumn = xColumns.second;
        reference.yStartColumn = yColumns.first;
        reference.yEndColumn = yColumns.second;
        references.append(reference);
    }

    return references;
}

std::optional<SourceVertexTextReference> sourceVertexReferenceAtCursor(const QVector<SourceVertexTextReference> &references,
                                                                       int cursorColumn,
                                                                       bool preferLastOutsideTokenRange = false)
{
    if (references.isEmpty()) {
        return std::nullopt;
    }

    const int normalizedColumn = qMax(1, cursorColumn);
    int minColumn = std::numeric_limits<int>::max();
    int maxColumn = std::numeric_limits<int>::min();
    for (const SourceVertexTextReference &reference : references) {
        minColumn = qMin(minColumn, qMin(reference.xStartColumn, reference.yStartColumn));
        maxColumn = qMax(maxColumn, qMax(reference.xEndColumn, reference.yEndColumn));
        if ((normalizedColumn >= reference.xStartColumn && normalizedColumn <= reference.xEndColumn)
            || (normalizedColumn >= reference.yStartColumn && normalizedColumn <= reference.yEndColumn)) {
            return reference;
        }
    }

    if (preferLastOutsideTokenRange
        && (normalizedColumn < minColumn || normalizedColumn > maxColumn)) {
        return references.last();
    }

    int bestDistance = std::numeric_limits<int>::max();
    std::optional<SourceVertexTextReference> bestReference;
    for (const SourceVertexTextReference &reference : references) {
        const int distance = qMin(qAbs(normalizedColumn - reference.xStartColumn),
                                  qAbs(normalizedColumn - reference.yStartColumn));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestReference = reference;
        }
    }

    return bestReference;
}

CursorGeometrySelection cursorGeometrySelectionForTextCursor(const QVector<TherionParsedLine> &parsedLines,
                                                             int cursorLine,
                                                             int cursorColumn)
{
    CursorGeometrySelection selection;
    bool inLineBlock = false;
    bool inAreaBlock = false;
    int activeFeatureLine = 0;
    int nextLineSourceVertexIndex = 0;
    int nextAreaSourceVertexIndex = 0;

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            if (inLineBlock && cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("line");
                return selection;
            }
            inLineBlock = false;
            activeFeatureLine = 0;
            nextLineSourceVertexIndex = 0;
            continue;
        }

        if (directive == QStringLiteral("endarea")) {
            if (inAreaBlock && cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("area");
                return selection;
            }
            inAreaBlock = false;
            activeFeatureLine = 0;
            nextAreaSourceVertexIndex = 0;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            inLineBlock = true;
            activeFeatureLine = parsedLine.lineNumber;
            nextLineSourceVertexIndex = 0;
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("line");
                const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextLineSourceVertexIndex);
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn, true);
                return selection;
            }

            lineSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextLineSourceVertexIndex);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            inAreaBlock = true;
            activeFeatureLine = parsedLine.lineNumber;
            nextAreaSourceVertexIndex = 0;
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("area");
                const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextAreaSourceVertexIndex);
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn);
                return selection;
            }

            areaSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextAreaSourceVertexIndex);
            continue;
        }

        if (inLineBlock) {
            const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextLineSourceVertexIndex);
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("line");
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn, true);
                // Any non-coordinate row inside a line block should still map to
                // the current/last parsed line vertex.
                if (!selection.sourceVertexReference.has_value()
                    && references.isEmpty()
                    && !parsedLine.tokens.isEmpty()
                    && nextLineSourceVertexIndex > 0) {
                    SourceVertexTextReference optionReference;
                    optionReference.lineNumber = parsedLine.lineNumber;
                    optionReference.sourceVertexIndex = nextLineSourceVertexIndex - 1;
                    optionReference.xStartColumn = 1;
                    optionReference.xEndColumn = 1;
                    optionReference.yStartColumn = 1;
                    optionReference.yEndColumn = 1;
                    selection.sourceVertexReference = optionReference;
                }
                return selection;
            }
            continue;
        }

        if (inAreaBlock) {
            const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextAreaSourceVertexIndex);
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("area");
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn);
                return selection;
            }
            continue;
        }
    }

    return selection;
}

std::optional<SourceVertexTextReference> sourceVertexTextReferenceForSelection(const QVector<TherionParsedLine> &parsedLines,
                                                                               int featureLineNumber,
                                                                               const QString &geometryKind,
                                                                               int sourceVertexIndex)
{
    if (featureLineNumber <= 0 || sourceVertexIndex < 0) {
        return std::nullopt;
    }

    const bool forLine = geometryKind.startsWith(QStringLiteral("line"));
    const bool forArea = geometryKind.startsWith(QStringLiteral("area"));
    if (!forLine && !forArea) {
        return std::nullopt;
    }

    bool inLineBlock = false;
    bool inAreaBlock = false;
    int nextLineSourceVertexIndex = 0;
    int nextAreaSourceVertexIndex = 0;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            inLineBlock = false;
            nextLineSourceVertexIndex = 0;
            continue;
        }
        if (directive == QStringLiteral("endarea")) {
            inAreaBlock = false;
            nextAreaSourceVertexIndex = 0;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            inLineBlock = (parsedLine.lineNumber == featureLineNumber) && forLine;
            nextLineSourceVertexIndex = 0;
            const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextLineSourceVertexIndex);
            if (inLineBlock) {
                for (const SourceVertexTextReference &reference : references) {
                    if (reference.sourceVertexIndex == sourceVertexIndex) {
                        return reference;
                    }
                }
            }
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            inAreaBlock = (parsedLine.lineNumber == featureLineNumber) && forArea;
            nextAreaSourceVertexIndex = 0;
            const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextAreaSourceVertexIndex);
            if (inAreaBlock) {
                for (const SourceVertexTextReference &reference : references) {
                    if (reference.sourceVertexIndex == sourceVertexIndex) {
                        return reference;
                    }
                }
            }
            continue;
        }

        if (inLineBlock) {
            const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextLineSourceVertexIndex);
            for (const SourceVertexTextReference &reference : references) {
                if (reference.sourceVertexIndex == sourceVertexIndex) {
                    return reference;
                }
            }
            continue;
        }

        if (inAreaBlock) {
            const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextAreaSourceVertexIndex);
            for (const SourceVertexTextReference &reference : references) {
                if (reference.sourceVertexIndex == sourceVertexIndex) {
                    return reference;
                }
            }
            continue;
        }
    }

    return std::nullopt;
}

std::optional<QSet<int>> scrapObjectLinesForCursor(const QVector<TherionParsedLine> &parsedLines,
                                                   int cursorLine)
{
    if (cursorLine <= 0) {
        return std::nullopt;
    }

    struct ScrapContext
    {
        int startLine = 0;
        QSet<int> objectLines;
    };

    QVector<ScrapContext> scrapStack;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;
        if (directive == QStringLiteral("scrap")) {
            ScrapContext context;
            context.startLine = parsedLine.lineNumber;
            scrapStack.append(context);
            continue;
        }

        if (!scrapStack.isEmpty()
            && (directive == QStringLiteral("point")
                || directive == QStringLiteral("station")
                || directive == QStringLiteral("line")
                || directive == QStringLiteral("area"))) {
            for (ScrapContext &context : scrapStack) {
                context.objectLines.insert(parsedLine.lineNumber);
            }
        }

        if (directive != QStringLiteral("endscrap")) {
            continue;
        }

        if (scrapStack.isEmpty()) {
            continue;
        }

        const ScrapContext context = scrapStack.takeLast();
        if (cursorLine == context.startLine || cursorLine == parsedLine.lineNumber) {
            return context.objectLines;
        }
    }

    for (const ScrapContext &context : std::as_const(scrapStack)) {
        if (cursorLine == context.startLine) {
            return context.objectLines;
        }
    }

    return std::nullopt;
}

std::optional<int> sourcePointLineNumberForSelection(const QVector<TherionParsedLine> &parsedLines,
                                                     const QPointF &sourcePoint)
{
    int bestLineNumber = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("point")
            && parsedLine.directive != QStringLiteral("station")) {
            continue;
        }

        const QVector<QPointF> points = pointsFromTokens(parsedLine.tokens.mid(1));
        if (points.isEmpty()) {
            continue;
        }

        const QPointF delta = points.first() - sourcePoint;
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance < bestDistance) {
            bestDistance = distance;
            bestLineNumber = parsedLine.lineNumber;
        }
    }

    if (bestLineNumber > 0 && bestDistance <= 0.5) {
        return bestLineNumber;
    }

    return std::nullopt;
}

MapEditableGeometryVertexItem *findGeometryVertexItem(QGraphicsScene *scene,
                                                      int lineNumber,
                                                      int sourceVertexIndex,
                                                      const QString &geometryKindPrefix)
{
    if (scene == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *item : items) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertexItem == nullptr) {
            continue;
        }
        if (vertexItem->lineNumber() != lineNumber || vertexItem->vertexIndex() != sourceVertexIndex) {
            continue;
        }
        if (!geometryKindPrefix.isEmpty() && !vertexItem->geometryKind().startsWith(geometryKindPrefix)) {
            continue;
        }
        return vertexItem;
    }

    return nullptr;
}
}

bool MapEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (mapView_ == nullptr || event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    QWidget *viewport = mapView_->viewport();
    if (watched == viewport) {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            lastTabletInteractionUtc_ = QDateTime::currentDateTimeUtc();
            break;
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (interactiveDrawMode_ == InteractiveDrawMode::Freehand) {
                    if (textEditor_ == nullptr) {
                        toolbarStatusNote_ = tr("Drawing failed: no active TH2 text editor.");
                        refreshToolbarSummary();
                        event->accept();
                        return true;
                    }

                    clearInteractiveDrawSession(false);
                    const QPointF scenePoint = mapView_->mapToScene(mouseEvent->pos());
                    interactiveDrawStrokeActive_ = true;
                    interactiveDrawSourceVertices_.append(sourcePointFromScenePosition(scenePoint));
                    interactiveDrawSceneVertices_.append(scenePoint);
                    updateInteractiveDrawPreview();
                    toolbarStatusNote_ = tr("Freehand mode: drawing stroke...");
                    refreshToolbarSummary();
                    updateCommandSurfaceState();
                    primaryPointerInteractionActive_ = false;
                    event->accept();
                    return true;
                }
                if (interactiveDrawMode_ == InteractiveDrawMode::Line
                    || interactiveDrawMode_ == InteractiveDrawMode::Area) {
                    const QPointF scenePoint = mapView_->mapToScene(mouseEvent->pos());
                    const QPointF sceneOffset = mapView_->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (const auto handle = interactiveLineControlAt(scenePoint, controlHitRadius)) {
                        interactiveDrawControlDragActive_ = true;
                        interactiveDrawControlDragHandle_ = handle.value();
                        interactiveDrawAnchorPressActive_ = false;
                        interactiveDrawAnchorDragActive_ = false;
                        interactiveDrawHoverActive_ = false;
                        viewport->setCursor(Qt::ClosedHandCursor);
                        toolbarStatusNote_ = interactiveDrawMode_ == InteractiveDrawMode::Line
                            ? tr("Line mode: dragging bezier control point.")
                            : tr("Area mode: dragging bezier control point.");
                        refreshToolbarSummary();
                        updateCommandSurfaceState();
                        primaryPointerInteractionActive_ = false;
                        event->accept();
                        return true;
                    }

                    interactiveDrawAnchorPressActive_ = true;
                    interactiveDrawAnchorPressScenePoint_ = scenePoint;
                    interactiveDrawAnchorDragActive_ = false;
                    interactiveDrawAnchorDragScenePoint_ = interactiveDrawAnchorPressScenePoint_;
                    interactiveDrawControlDragActive_ = false;
                    interactiveDrawHoverActive_ = false;
                    updateInteractiveDrawPreview();
                    primaryPointerInteractionActive_ = false;
                    event->accept();
                    return true;
                }
                if (handleInteractiveDrawClick(mapView_->mapToScene(mouseEvent->pos()))) {
                    primaryPointerInteractionActive_ = false;
                    event->accept();
                    return true;
                }
                primaryPointerInteractionActive_ = true;
                if (mapView_ != nullptr) {
                    pendingMapClickSelection_ = true;
                    pendingMapClickScenePosition_ = mapView_->mapToScene(mouseEvent->pos());
                    pendingMapClickElapsed_.start();
                    pendingMapClickLineNumber_ = 0;
                    pendingMapClickSourceVertexIndex_ = -1;
                    pendingMapClickGeometryKind_.clear();
                    if (mapScene_ != nullptr) {
                        const QList<QGraphicsItem *> hitItems = mapScene_->items(pendingMapClickScenePosition_,
                                                                                 Qt::IntersectsItemShape,
                                                                                 Qt::DescendingOrder,
                                                                                 mapView_->transform());
                        if (QGraphicsItem *item = preferredMapHitItem(hitItems)) {
                            const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
                            pendingMapClickLineNumber_ = lineNumber;
                            if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
                                pendingMapClickSourceVertexIndex_ = vertexItem->vertexIndex();
                                pendingMapClickGeometryKind_ = vertexItem->geometryKind();
                            }
                        }
                    }
                }
            }

            if (mouseEvent->button() == Qt::RightButton) {
                mapPanActive_ = true;
                mapPanLastPosition_ = mouseEvent->pos();
                viewport->setCursor(Qt::ClosedHandCursor);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if ((interactiveDrawMode_ == InteractiveDrawMode::Line
                 || interactiveDrawMode_ == InteractiveDrawMode::Area)
                && interactiveDrawControlDragActive_) {
                const QPointF scenePoint = mapView_->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                if (setInteractiveLineControlScenePoint(interactiveDrawControlDragHandle_, scenePoint)) {
                    updateInteractiveDrawPreview();
                }
                event->accept();
                return true;
            }

            if ((interactiveDrawMode_ == InteractiveDrawMode::Line
                 || interactiveDrawMode_ == InteractiveDrawMode::Area)
                && interactiveDrawAnchorPressActive_) {
                const QPointF scenePoint = mapView_->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal dragThreshold = 4.0;
                if (!interactiveDrawAnchorDragActive_
                    && QLineF(interactiveDrawAnchorPressScenePoint_, scenePoint).length() >= dragThreshold) {
                    interactiveDrawAnchorDragActive_ = true;
                }
                interactiveDrawAnchorDragScenePoint_ = scenePoint;
                updateInteractiveDrawPreview();
                event->accept();
                return true;
            }

            if (interactiveDrawMode_ == InteractiveDrawMode::Freehand && interactiveDrawStrokeActive_) {
                const QPointF scenePoint = mapView_->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal minimumSceneSampleDistance = 4.0;
                if (interactiveDrawSceneVertices_.isEmpty()
                    || QLineF(interactiveDrawSceneVertices_.last(), scenePoint).length() >= minimumSceneSampleDistance) {
                    interactiveDrawSceneVertices_.append(scenePoint);
                    interactiveDrawSourceVertices_.append(sourcePointFromScenePosition(scenePoint));
                    updateInteractiveDrawPreview();
                    updateCommandSurfaceState();
                }
                event->accept();
                return true;
            }

            if (interactiveDrawMode_ == InteractiveDrawMode::Line
                || interactiveDrawMode_ == InteractiveDrawMode::Area) {
                const bool hasDraftVertices = !interactiveDrawLineVertices_.isEmpty();
                if (hasDraftVertices) {
                    const QPoint mousePosition = static_cast<QMouseEvent *>(event)->pos();
                    const QPointF scenePoint = mapView_->mapToScene(mousePosition);
                    const QPointF sceneOffset = mapView_->mapToScene(mousePosition + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                        viewport->setCursor(Qt::OpenHandCursor);
                    } else {
                        viewport->unsetCursor();
                    }
                    interactiveDrawHoverActive_ = true;
                    interactiveDrawHoverScenePoint_ = scenePoint;
                    updateInteractiveDrawPreview();
                    event->accept();
                    return true;
                }
            }

            if (!mapPanActive_) {
                break;
            }

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint delta = mouseEvent->pos() - mapPanLastPosition_;
            mapPanLastPosition_ = mouseEvent->pos();
            if (mapView_->horizontalScrollBar() != nullptr) {
                mapView_->horizontalScrollBar()->setValue(mapView_->horizontalScrollBar()->value() - delta.x());
            }
            if (mapView_->verticalScrollBar() != nullptr) {
                mapView_->verticalScrollBar()->setValue(mapView_->verticalScrollBar()->value() - delta.y());
            }

            autoFitEnabled_ = false;
            syncZoomFactorFromView();
            updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((interactiveDrawMode_ == InteractiveDrawMode::Line
                 || interactiveDrawMode_ == InteractiveDrawMode::Area)
                && interactiveDrawControlDragActive_
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF scenePoint = mapView_->mapToScene(mouseEvent->pos());
                setInteractiveLineControlScenePoint(interactiveDrawControlDragHandle_, scenePoint);
                interactiveDrawControlDragActive_ = false;
                const QPointF sceneOffset = mapView_->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                if (interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                    viewport->setCursor(Qt::OpenHandCursor);
                } else {
                    viewport->unsetCursor();
                }
                toolbarStatusNote_ = interactiveDrawMode_ == InteractiveDrawMode::Line
                    ? tr("Line mode: bezier control adjusted.")
                    : tr("Area mode: bezier control adjusted.");
                refreshToolbarSummary();
                updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if ((interactiveDrawMode_ == InteractiveDrawMode::Line
                 || interactiveDrawMode_ == InteractiveDrawMode::Area)
                && interactiveDrawAnchorPressActive_
                && mouseEvent->button() == Qt::LeftButton) {
                std::optional<QPointF> dragScenePoint;
                if (interactiveDrawAnchorDragActive_) {
                    const QPointF releaseScenePoint = mapView_->mapToScene(mouseEvent->pos());
                    constexpr qreal dragThreshold = 4.0;
                    if (QLineF(interactiveDrawAnchorPressScenePoint_, releaseScenePoint).length() >= dragThreshold) {
                        dragScenePoint = releaseScenePoint;
                    }
                }

                captureInteractiveLineAnchor(interactiveDrawAnchorPressScenePoint_, dragScenePoint);
                interactiveDrawAnchorPressActive_ = false;
                interactiveDrawAnchorDragActive_ = false;
                interactiveDrawHoverActive_ = false;
                toolbarStatusNote_ = interactiveDrawMode_ == InteractiveDrawMode::Line
                    ? tr("Line mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg(interactiveDrawLineVertices_.size())
                    : tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg(interactiveDrawLineVertices_.size());
                refreshToolbarSummary();
                updateCommandSurfaceState();
                event->accept();
                return true;
            }
            if (interactiveDrawMode_ == InteractiveDrawMode::Freehand
                && interactiveDrawStrokeActive_
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF releasePoint = mapView_->mapToScene(mouseEvent->pos());
                if (interactiveDrawSceneVertices_.isEmpty()
                    || QLineF(interactiveDrawSceneVertices_.last(), releasePoint).length() >= 1.0) {
                    interactiveDrawSceneVertices_.append(releasePoint);
                    interactiveDrawSourceVertices_.append(sourcePointFromScenePosition(releasePoint));
                }
                interactiveDrawStrokeActive_ = false;

                if (interactiveDrawSourceVertices_.size() < 2) {
                    clearInteractiveDrawSession(false);
                    toolbarStatusNote_ = tr("Freehand mode needs a drag stroke to create a line.");
                    refreshToolbarSummary();
                    updateCommandSurfaceState();
                    event->accept();
                    return true;
                }

                const bool committed = commitInteractiveDrawVertices(QStringLiteral("line"),
                                                                     interactiveDrawSourceVertices_,
                                                                     tr("freehand line"));
                clearInteractiveDrawSession(false);
                if (committed) {
                    updateHelpPanel();
                }
                refreshToolbarSummary();
                updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->buttons() == Qt::NoButton) {
                primaryPointerInteractionActive_ = false;
            }

            if (mapPanActive_ && mouseEvent->button() == Qt::RightButton) {
                mapPanActive_ = false;
                viewport->unsetCursor();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::Wheel: {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            if (nativeZoomGestureActive_
                && lastNativeZoomGestureUtc_.isValid()
                && lastNativeZoomGestureUtc_.msecsTo(QDateTime::currentDateTimeUtc()) > 1500) {
                nativeZoomGestureActive_ = false;
            }

            const bool recentNativeZoom = lastNativeZoomGestureUtc_.isValid()
                && lastNativeZoomGestureUtc_.msecsTo(QDateTime::currentDateTimeUtc()) <= 150;
            if (nativeZoomGestureActive_ || recentNativeZoom) {
                event->accept();
                return true;
            }

            if (primaryPointerInteractionActive_) {
                event->accept();
                return true;
            }

            const Qt::KeyboardModifiers modifiers = wheelEvent->modifiers();
            const bool cmdModifier = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
            const bool preciseScroll = wheelEventHasPreciseScrollingDeltas(wheelEvent);
            const MapEditorWheelAction wheelAction = resolveMapEditorWheelAction(touchFriendlyControlsEnabled_,
                                                                                 preciseScroll,
                                                                                 cmdModifier);
            if (wheelAction == MapEditorWheelAction::Zoom) {
                const QPoint pixelDelta = wheelEvent->pixelDelta();
                const QPoint angleDelta = wheelEvent->angleDelta();
                qreal delta = !pixelDelta.isNull()
                    ? static_cast<qreal>(pixelDelta.y())
                    : static_cast<qreal>(angleDelta.y());
                if (qFuzzyIsNull(delta) && !angleDelta.isNull()) {
                    delta = static_cast<qreal>(angleDelta.x());
                }

                if (!qFuzzyIsNull(delta)) {
                    const qreal factor = std::pow(1.0015, delta);
                    applyZoomAtViewportPosition(factor, wheelEvent->position());
                }

                event->accept();
                return true;
            }

            QPoint panDelta = wheelEvent->pixelDelta();
            if (panDelta.isNull()) {
                const QPoint angleDelta = wheelEvent->angleDelta();
                panDelta = QPoint(qRound(angleDelta.x() / 4.0), qRound(angleDelta.y() / 4.0));
            }

            if (!panDelta.isNull()) {
                if (mapView_->horizontalScrollBar() != nullptr) {
                    mapView_->horizontalScrollBar()->setValue(mapView_->horizontalScrollBar()->value() - panDelta.x());
                }
                if (mapView_->verticalScrollBar() != nullptr) {
                    mapView_->verticalScrollBar()->setValue(mapView_->verticalScrollBar()->value() - panDelta.y());
                }

                autoFitEnabled_ = false;
                syncZoomFactorFromView();
                updateCommandSurfaceState();
            }

            event->accept();
            return true;
        }
        case QEvent::NativeGesture: {
            auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
            if (primaryPointerInteractionActive_) {
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::BeginNativeGesture) {
                nativeZoomGestureActive_ = true;
                lastNativeZoomGestureUtc_ = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::EndNativeGesture) {
                nativeZoomGestureActive_ = false;
                lastNativeZoomGestureUtc_ = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
                nativeZoomGestureActive_ = true;
                lastNativeZoomGestureUtc_ = QDateTime::currentDateTimeUtc();
                const qreal rawValue = gestureEvent->value();
                if (!std::isfinite(rawValue)) {
                    event->accept();
                    return true;
                }

                // Clamp one pinch update so trackpad spikes cannot jump straight to extreme scales.
                const qreal clampedDelta = qBound(-0.35, rawValue, 0.35);
                const qreal factor = std::exp(clampedDelta);
                if (factor > 0.0) {
                    applyZoomAtViewportPosition(factor, gestureEvent->position());
                }
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::TouchBegin: {
            if (!shouldEnableTouchPanCandidate(touchFriendlyControlsEnabled_,
                                               selectModeActive_,
                                               primaryPointerInteractionActive_)) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() == 2) {
                const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
                touchPanCandidate_ = true;
                touchPanActive_ = false;
                touchPanStartPosition_ = centroid;
                touchPanLastPosition_ = centroid;
            }
            break;
        }
        case QEvent::TouchUpdate: {
            if (!touchPanCandidate_ || primaryPointerInteractionActive_) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() != 2) {
                touchPanCandidate_ = false;
                touchPanActive_ = false;
                break;
            }

            const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
            if (!touchPanActive_) {
                const qreal threshold = 8.0;
                if (QLineF(touchPanStartPosition_, centroid).length() < threshold) {
                    event->accept();
                    return true;
                }
                touchPanActive_ = true;
            }

            const QPointF delta = centroid - touchPanLastPosition_;
            touchPanLastPosition_ = centroid;
            if (mapView_->horizontalScrollBar() != nullptr) {
                mapView_->horizontalScrollBar()->setValue(mapView_->horizontalScrollBar()->value() - qRound(delta.x()));
            }
            if (mapView_->verticalScrollBar() != nullptr) {
                mapView_->verticalScrollBar()->setValue(mapView_->verticalScrollBar()->value() - qRound(delta.y()));
            }

            autoFitEnabled_ = false;
            syncZoomFactorFromView();
            updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            touchPanCandidate_ = false;
            touchPanActive_ = false;
            break;
        case QEvent::Leave:
            if (mapPanActive_) {
                mapPanActive_ = false;
                viewport->unsetCursor();
            }
            primaryPointerInteractionActive_ = false;
            touchPanCandidate_ = false;
            touchPanActive_ = false;
            nativeZoomGestureActive_ = false;
            interactiveDrawStrokeActive_ = false;
            interactiveDrawAnchorPressActive_ = false;
            interactiveDrawAnchorDragActive_ = false;
            interactiveDrawControlDragActive_ = false;
            viewport->unsetCursor();
            if (interactiveDrawHoverActive_) {
                interactiveDrawHoverActive_ = false;
                updateInteractiveDrawPreview();
            }
            break;
        case QEvent::Resize:
            if (autoFitEnabled_ && mapView_->isVisible()) {
                fitMapToView(fitBackgroundRequested_);
            }
            break;
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (interactiveDrawMode_ == InteractiveDrawMode::Line
                || interactiveDrawMode_ == InteractiveDrawMode::Area
                || interactiveDrawMode_ == InteractiveDrawMode::Freehand) {
                if ((keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
                    && keyEvent->modifiers() == Qt::NoModifier
                    && interactiveDrawMode_ != InteractiveDrawMode::Freehand) {
                    if (interactiveDrawMode_ == InteractiveDrawMode::Line
                        && !interactiveDrawLineVertices_.isEmpty()) {
                        interactiveDrawLineVertices_.removeLast();
                        if (!interactiveDrawLineVertices_.isEmpty()) {
                            InteractiveLineDraftVertex &tail = interactiveDrawLineVertices_.last();
                            tail.outgoingControlScene.reset();
                            tail.outgoingControlSource.reset();
                        }
                        updateInteractiveDrawPreview();
                        toolbarStatusNote_ = tr("Vertex removed from current draft (%1 remaining).")
                                                 .arg(interactiveDrawLineVertices_.size());
                        refreshToolbarSummary();
                        updateCommandSurfaceState();
                        event->accept();
                        return true;
                    }
                    if (interactiveDrawMode_ == InteractiveDrawMode::Area
                        && !interactiveDrawLineVertices_.isEmpty()) {
                        interactiveDrawLineVertices_.removeLast();
                        if (!interactiveDrawLineVertices_.isEmpty()) {
                            InteractiveLineDraftVertex &tail = interactiveDrawLineVertices_.last();
                            tail.outgoingControlScene.reset();
                            tail.outgoingControlSource.reset();
                        }
                        updateInteractiveDrawPreview();
                        toolbarStatusNote_ = tr("Vertex removed from current draft (%1 remaining).")
                                                 .arg(interactiveDrawLineVertices_.size());
                        refreshToolbarSummary();
                        updateCommandSurfaceState();
                        event->accept();
                        return true;
                    }
                }
            }

            const bool insertShortcut = keyEvent->key() == Qt::Key_Insert
                || (keyEvent->key() == Qt::Key_I && keyEvent->modifiers() == Qt::NoModifier);
            if (insertShortcut) {
                if (insertLineVertexFromSelection()) {
                    event->accept();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
                if (removeLineVertexFromSelection()) {
                    event->accept();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_S && keyEvent->modifiers() == Qt::NoModifier) {
                if (toggleLineVertexSmoothFromSelection()) {
                    event->accept();
                    return true;
                }
            }
            break;
        }
        default:
            break;
        }
    } else if (watched == mapView_ && event->type() == QEvent::Resize) {
        if (autoFitEnabled_ && mapView_->isVisible()) {
            fitMapToView(fitBackgroundRequested_);
        }
    }

    return QWidget::eventFilter(watched, event);
}

void MapEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void MapEditorTab::handleApplicationAppearanceChanged()
{
    if (mapView_ != nullptr) {
        mapView_->setBackgroundBrush(mapView_->palette().color(QPalette::Window));
        if (QWidget *viewport = mapView_->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    if (mapScene_ != nullptr) {
        refreshMapScenePreservingUndoStack();
    }
}

void MapEditorTab::buildMapScene()
{
    mapScene_ = new QGraphicsScene(this);
    mapView_->setScene(mapScene_);
    connect(mapScene_, &QGraphicsScene::selectionChanged, this, &MapEditorTab::handleMapSceneSelectionChanged);
}

void MapEditorTab::buildHelpPanel()
{
    helpPanel_ = new QFrame(this);
    helpPanel_->setFrameShape(QFrame::StyledPanel);

    auto *panelLayout = new QVBoxLayout(helpPanel_);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);

    auto *headerLabel = new QLabel(tr("Map Help"), helpPanel_);
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);

    headerRow->addWidget(headerLabel);
    headerRow->addStretch(1);

    helpBrowser_ = new QTextBrowser(helpPanel_);
    helpBrowser_->setFrameShape(QFrame::NoFrame);
    helpBrowser_->setOpenLinks(false);
    helpBrowser_->setOpenExternalLinks(false);
    helpBrowser_->setMinimumHeight(120);
    helpBrowser_->setHtml(mapWorkspaceHelpHtml());

    panelLayout->addLayout(headerRow);
    panelLayout->addWidget(helpBrowser_, 1);
}

void MapEditorTab::refreshMapScene()
{
    if (mapScene_ == nullptr) {
        return;
    }

    if (mapCommandApplyInProgress_) {
        mapSceneRefreshPending_ = true;
        return;
    }

    if (undoStack_ != nullptr) {
        undoStack_->clear();
    }

    refreshMapScenePreservingUndoStack();
}

void MapEditorTab::refreshMapScenePreservingUndoStack()
{
    if (mapScene_ == nullptr) {
        return;
    }

    if (undoStack_ != nullptr) {
        updateCommandSurfaceState();
    }

    clearMapScene();

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
    const QVector<MapSceneEntry> entries = collectMapSceneEntries(parsedLines);
    const QVector<MapGeometryFeature> geometryFeatures = collectGeometryFeatures(parsedLines);
    renderMapWorkspaceScene(mapScene_,
                            filePath(),
                            entries,
                            geometryFeatures,
                            &mapItemsByLine_,
                            [this](int lineNumber, const QPointF &oldPosition, const QPointF &newPosition) {
                                recordCardMove(lineNumber, oldPosition, newPosition);
                            },
                            [this](int lineNumber, bool oldVisible, bool newVisible) {
                                recordCardVisibility(lineNumber, oldVisible, newVisible);
                            },
                            [this](int lineNumber, const QPointF &oldPoint, const QPointF &newPoint) {
                                recordPointGeometryMove(lineNumber, oldPoint, newPoint);
                            },
                            [this](int lineNumber, const QString &kind, int vertexIndex, const QPointF &oldPoint, const QPointF &newPoint) {
                                recordLineAreaVertexMove(lineNumber, kind, vertexIndex, oldPoint, newPoint);
                            });

    restoreBackgroundImageItems();
    restoreDraftGeometryItems();
    selectMapLine(textEditor_->currentLineNumber());
    updateGeometrySelectionPresentation();
    if (autoFitEnabled_) {
        fitMapToView(fitBackgroundRequested_);
    } else {
        syncZoomFactorFromView();
    }
    updateInteractiveDrawPreview();
    refreshStatus();
    updateCommandSurfaceState();
    updateHelpPanel();
}

void MapEditorTab::flushPendingMapSceneRefreshAfterCommand()
{
    if (!mapSceneRefreshPending_) {
        return;
    }

    mapSceneRefreshPending_ = false;
    refreshMapScenePreservingUndoStack();
}

void MapEditorTab::handleMapSceneSelectionChanged()
{
    if (updatingSelection_ || mapScene_ == nullptr) {
        updateHelpPanel();
        return;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.isEmpty()) {
        updateGeometrySelectionPresentation();
        updateHelpPanel();
        return;
    }

    int selectedLineNumber = 0;
    int selectedSourceVertexIndex = -1;
    QString selectedGeometryKind;
    std::optional<QPointF> selectedPointSource;
    MapCardItem *selectedCard = nullptr;
    QList<QGraphicsItem *> selectedLineItems;
    QSet<int> selectedLineNumbers;
    selectedLineItems.reserve(selectedItems.size());
    for (QGraphicsItem *item : selectedItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }
        selectedLineItems.append(item);
        selectedLineNumbers.insert(lineNumber);
    }

    QGraphicsItem *primarySelectedItem = nullptr;
    const bool usePendingClickSelection = pendingMapClickSelection_
        && pendingMapClickElapsed_.isValid()
        && pendingMapClickElapsed_.elapsed() <= 1000;
    if (usePendingClickSelection && pendingMapClickLineNumber_ > 0) {
        if (pendingMapClickSourceVertexIndex_ >= 0) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item == nullptr) {
                    continue;
                }
                if (item->data(kMapSceneLineNumberRole).toInt() != pendingMapClickLineNumber_) {
                    continue;
                }
                auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
                if (vertexItem == nullptr) {
                    continue;
                }
                if (vertexItem->vertexIndex() != pendingMapClickSourceVertexIndex_) {
                    continue;
                }
                if (!pendingMapClickGeometryKind_.isEmpty()
                    && !vertexItem->geometryKind().startsWith(pendingMapClickGeometryKind_)) {
                    continue;
                }
                primarySelectedItem = item;
                break;
            }
        }
        if (primarySelectedItem == nullptr) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == pendingMapClickLineNumber_) {
                    primarySelectedItem = item;
                    break;
                }
            }
        }
    }
    if (mapView_ != nullptr && mapView_->viewport() != nullptr) {
        if (primarySelectedItem == nullptr && usePendingClickSelection) {
            const QPointF scenePos = pendingMapClickScenePosition_;
            const QList<QGraphicsItem *> hitItems = mapScene_->items(scenePos,
                                                                     Qt::IntersectsItemShape,
                                                                     Qt::DescendingOrder,
                                                                     mapView_->transform());
            primarySelectedItem = preferredMapHitItem(hitItems, true);
        } else if (primarySelectedItem == nullptr) {
            const QPoint viewportPos = mapView_->viewport()->mapFromGlobal(QCursor::pos());
            if (mapView_->viewport()->rect().contains(viewportPos)) {
                const QPointF scenePos = mapView_->mapToScene(viewportPos);
                const QList<QGraphicsItem *> hitItems = mapScene_->items(scenePos,
                                                                         Qt::IntersectsItemShape,
                                                                         Qt::DescendingOrder,
                                                                         mapView_->transform());
                primarySelectedItem = preferredMapHitItem(hitItems, true);
            }
        }
    }

    if (primarySelectedItem == nullptr && !selectedLineItems.isEmpty()) {
        if (selectedLineNumbers.size() == 1 && selectedLineItems.size() > 1) {
            QGraphicsItem *highestZItem = selectedLineItems.first();
            qreal highestZ = highestZItem != nullptr ? highestZItem->zValue() : 0.0;
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->zValue() > highestZ) {
                    highestZ = item->zValue();
                    highestZItem = item;
                }
            }
            primarySelectedItem = highestZItem;
        } else {
            primarySelectedItem = selectedLineItems.first();
        }
    }

    pendingMapClickSelection_ = false;
    pendingMapClickLineNumber_ = 0;
    pendingMapClickSourceVertexIndex_ = -1;
    pendingMapClickGeometryKind_.clear();

    if (primarySelectedItem != nullptr) {
        selectedLineNumber = primarySelectedItem->data(kMapSceneLineNumberRole).toInt();
        selectedCard = dynamic_cast<MapCardItem *>(primarySelectedItem);
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            selectedSourceVertexIndex = vertexItem->vertexIndex();
            selectedGeometryKind = vertexItem->geometryKind();
        } else if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(primarySelectedItem)) {
            selectedPointSource = pointItem->sourcePoint();
        }
    }

    if (selectedLineNumber > 0 && textEditor_ != nullptr) {
        const QScopedValueRollback<bool> syncGuard(mapSelectionDrivenTextNavigationInProgress_, true);
        if (selectedPointSource.has_value()) {
            const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
            const std::optional<int> pointLineNumber =
                sourcePointLineNumberForSelection(parsedLines, selectedPointSource.value());
            if (pointLineNumber.has_value()) {
                textEditor_->goToLineColumn(pointLineNumber.value(), 1);
            } else if (selectedLineNumber != currentLineNumber()) {
                textEditor_->goToLine(selectedLineNumber);
            }
        } else if (selectedSourceVertexIndex >= 0) {
            const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
            const std::optional<SourceVertexTextReference> sourceReference =
                sourceVertexTextReferenceForSelection(parsedLines,
                                                      selectedLineNumber,
                                                      selectedGeometryKind,
                                                      selectedSourceVertexIndex);
            if (sourceReference.has_value()) {
                if (textEditor_->currentLineNumber() != sourceReference->lineNumber
                    || textEditor_->currentColumnNumber() != sourceReference->xStartColumn) {
                    textEditor_->goToLineColumn(sourceReference->lineNumber, sourceReference->xStartColumn);
                }
            } else if (selectedLineNumber != currentLineNumber()) {
                textEditor_->goToLine(selectedLineNumber);
            }
        } else if (selectedLineNumber != currentLineNumber()) {
            textEditor_->goToLine(selectedLineNumber);
        }
    }

    if (selectedCard != nullptr) {
        updateGeometrySelectionPresentation();
        updateCommandSurfaceState();
        updateHelpPanel();
        return;
    }

    updateGeometrySelectionPresentation();
    updateCommandSurfaceState();
    updateHelpPanel();
}

void MapEditorTab::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    lastCursorSyncedLine_ = lineNumber;
    lastCursorSyncedColumn_ = columnNumber;

    if (mapScene_ == nullptr || textEditor_ == nullptr || lineNumber <= 0) {
        return;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
    if (const std::optional<QSet<int>> scrapLines = scrapObjectLinesForCursor(parsedLines, lineNumber);
        scrapLines.has_value()) {
        if (!scrapLines->isEmpty()) {
            selectMapLines(scrapLines.value());
        } else {
            selectMapLine(lineNumber);
        }
        return;
    }

    const CursorGeometrySelection cursorSelection = cursorGeometrySelectionForTextCursor(parsedLines,
                                                                                         lineNumber,
                                                                                         columnNumber);
    if (cursorSelection.featureLineNumber <= 0) {
        selectMapLine(lineNumber);
        return;
    }

    selectMapLine(cursorSelection.featureLineNumber);
    if (!cursorSelection.sourceVertexReference.has_value()) {
        return;
    }

    const int sourceVertexIndex = cursorSelection.sourceVertexReference->sourceVertexIndex;
    if (sourceVertexIndex < 0 || mapScene_ == nullptr) {
        return;
    }

    const QString geometryKind = cursorSelection.geometryKind.trimmed().toLower();
    const bool lineGeometry = geometryKind == QStringLiteral("line");
    const bool areaGeometry = geometryKind == QStringLiteral("area");
    if (!lineGeometry && !areaGeometry) {
        return;
    }

    MapEditableGeometryVertexItem *targetItem = nullptr;
    if (lineGeometry) {
        int ownerSourceVertexIndex = -1;
        if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(),
                                                                                            cursorSelection.featureLineNumber);
            lineFeature.has_value()) {
            const int ownerVertexOrder = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
            if (ownerVertexOrder >= 0 && ownerVertexOrder < lineFeature->lineVertices.size()) {
                const MapGeometryFeature::TH2LineVertex &ownerVertex = lineFeature->lineVertices.at(ownerVertexOrder);
                ownerSourceVertexIndex = ownerVertex.anchorSourceVertexIndex >= 0
                    ? ownerVertex.anchorSourceVertexIndex
                    : ownerVertexOrder;
            }
        }

        updatingSelection_ = true;
        if (ownerSourceVertexIndex >= 0) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(mapScene_,
                                                                                     cursorSelection.featureLineNumber,
                                                                                     ownerSourceVertexIndex,
                                                                                     QStringLiteral("line"))) {
                ownerAnchor->setSelected(true);
            }
        }
        updatingSelection_ = false;
        updateGeometrySelectionPresentation();

        targetItem = findGeometryVertexItem(mapScene_,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("line"));
    } else {
        targetItem = findGeometryVertexItem(mapScene_,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("area"));
    }

    if (targetItem != nullptr) {
        updatingSelection_ = true;
        targetItem->setSelected(true);
        updatingSelection_ = false;
        updateGeometrySelectionPresentation();
        updateCommandSurfaceState();
        updateHelpPanel();
    }
}

void MapEditorTab::updateGeometrySelectionPresentation()
{
    if (mapScene_ == nullptr) {
        return;
    }

    QSet<int> selectedLines;
    QHash<int, QSet<int>> selectedLineControlOwnersByLine;
    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (item == nullptr) {
            continue;
        }

        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            selectedLines.insert(lineNumber);
        }

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (subtype == kMapSceneSelectionSubtypeLineAnchor
            || subtype == kMapSceneSelectionSubtypeLineControl
            || subtype == kMapSceneSelectionSubtypeLineControlConnector) {
            const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
            if (lineNumber > 0 && ownerVertexIndex >= 0) {
                selectedLineControlOwnersByLine[lineNumber].insert(ownerVertexIndex);
            }
        }
    }

    const auto sceneItems = mapScene_->items();
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr) {
            continue;
        }
        if (!item->data(kMapSceneSelectionGatedRole).toBool()) {
            continue;
        }

        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        bool visible = lineNumber > 0 && selectedLines.contains(lineNumber);
        if (visible) {
            const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
            if (subtype == kMapSceneSelectionSubtypeLineControl
                || subtype == kMapSceneSelectionSubtypeLineControlConnector) {
                const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
                visible = ownerVertexIndex >= 0
                    && selectedLineControlOwnersByLine.value(lineNumber).contains(ownerVertexIndex);
            }
        }

        item->setVisible(visible);
    }
}

void MapEditorTab::handleAddPointTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Point);
    toolbarStatusNote_ = tr("Point mode: click in map to insert a point.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Line);
    toolbarStatusNote_ = tr("Line mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Freehand);
    toolbarStatusNote_ = tr("Freehand mode: drag in map to draw a line, then release to finish.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddSmartTraceLineTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::None);
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Smart Trace mode: inserted a trace-ready draft line. Complete Draft commits one undo step.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Line), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(190.0, 190.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Area);
    toolbarStatusNote_ = tr("Area mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::None);
    selectModeActive_ = true;
    toolbarStatusNote_ = tr("Selection mode: select map cards or draft objects.");
    if (mapView_ != nullptr) {
        mapView_->setDragMode(QGraphicsView::NoDrag);
    }
    refreshToolbarSummary();
}

void MapEditorTab::handleInsertScrapTriggered()
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Insert Scrap failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    if (!textEditor_->insertScrapBlock(QStringLiteral("new-scrap"), &insertedLineNumber, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Insert Scrap failed.")
            : tr("Insert Scrap failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Inserted scrap block at line %1.").arg(insertedLineNumber)
        : tr("Inserted scrap block.");
    refreshToolbarSummary();
}

void MapEditorTab::handleCompleteDraftTriggered()
{
    if (commitInteractiveDrawSession()) {
        return;
    }

    auto *draftRectItem = selectedDraftGeometryItem();
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(draftRectItem);
    if (draftItem == nullptr || draftItem->isDraftComplete()) {
        return;
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    const QVector<QPointF> vertices = sourceVerticesForDraft(draftItem);
    if (vertices.isEmpty()) {
        toolbarStatusNote_ = tr("Complete Draft failed: unable to resolve draft geometry coordinates.");
        refreshToolbarSummary();
        return;
    }

    QString geometryKind;
    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        geometryKind = QStringLiteral("point");
        break;
    case DraftGeometryKind::Line:
        geometryKind = QStringLiteral("line");
        break;
    case DraftGeometryKind::Area:
        geometryKind = QStringLiteral("area");
        break;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    if (!textEditor_->insertDraftGeometry(geometryKind, vertices, &insertedLineNumber, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }

    const QString geometryLabel = draftGeometryLabel(draftItem->kind());
    removeDraftGeometryItem(draftRectItem);
    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(geometryLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(geometryLabel);
    refreshToolbarSummary();
    updateHelpPanel();
}

void MapEditorTab::setInteractiveDrawMode(InteractiveDrawMode mode)
{
    const InteractiveDrawMode previousMode = interactiveDrawMode_;
    clearInteractiveDrawSession(false);
    interactiveDrawMode_ = mode;
    selectModeActive_ = mode == InteractiveDrawMode::None;
    if (previousMode != interactiveDrawMode_) {
        emit modeStatusChanged();
    }
    updateCommandSurfaceState();
}

bool MapEditorTab::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    if (interactiveDrawMode_ == InteractiveDrawMode::None) {
        return false;
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Drawing failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return true;
    }

    if (interactiveDrawMode_ == InteractiveDrawMode::Point) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QVector<QPointF> vertices{sourcePointFromScenePosition(scenePosition)};
        if (!textEditor_->insertDraftGeometry(QStringLiteral("point"), vertices, &insertedLineNumber, &errorMessage)) {
            toolbarStatusNote_ = errorMessage.isEmpty()
                ? tr("Point insert failed.")
                : tr("Point insert failed: %1").arg(errorMessage);
        } else {
            toolbarStatusNote_ = insertedLineNumber > 0
                ? tr("Inserted point at source line %1.").arg(insertedLineNumber)
                : tr("Inserted point.");
        }
        refreshToolbarSummary();
        return true;
    }

    if (interactiveDrawMode_ == InteractiveDrawMode::Area) {
        captureInteractiveLineAnchor(scenePosition, std::nullopt);
        interactiveDrawHoverActive_ = false;
        toolbarStatusNote_ = tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                                 .arg(interactiveDrawLineVertices_.size());
        refreshToolbarSummary();
        updateCommandSurfaceState();
        return true;
    }

    return false;
}

bool MapEditorTab::commitInteractiveDrawSession()
{
    const InteractiveDrawMode modeAtCommit = interactiveDrawMode_;
    if (modeAtCommit != InteractiveDrawMode::Line
        && modeAtCommit != InteractiveDrawMode::Area) {
        return false;
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return true;
    }

    const int minVertexCount = modeAtCommit == InteractiveDrawMode::Line ? 2 : 3;
    const int capturedVertices = interactiveDrawLineVertices_.size();
    if (capturedVertices < minVertexCount) {
        toolbarStatusNote_ = modeAtCommit == InteractiveDrawMode::Line
            ? tr("Line mode needs at least 2 vertices before completion.")
            : tr("Area mode needs at least 3 vertices before completion.");
        refreshToolbarSummary();
        return true;
    }

    if (modeAtCommit == InteractiveDrawMode::Line) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QStringList coordinateRows = lineCoordinateRowsForInteractiveDraft();
        if (!textEditor_->insertDraftLineGeometry(coordinateRows, &insertedLineNumber, &errorMessage)) {
            toolbarStatusNote_ = errorMessage.isEmpty()
                ? tr("Complete Draft failed.")
                : tr("Complete Draft failed: %1").arg(errorMessage);
            refreshToolbarSummary();
            return true;
        }
        toolbarStatusNote_ = insertedLineNumber > 0
            ? tr("Complete Draft wrote line geometry at source line %1.").arg(insertedLineNumber)
            : tr("Complete Draft wrote line geometry to source.");
    } else {
        QString errorMessage;
        int insertedLineNumber = 0;
        if (!textEditor_->insertDraftAreaGeometry(areaCoordinateRowsForInteractiveDraft(),
                                                  &insertedLineNumber,
                                                  &errorMessage)) {
            toolbarStatusNote_ = errorMessage.isEmpty()
                ? tr("Complete Draft failed.")
                : tr("Complete Draft failed: %1").arg(errorMessage);
            refreshToolbarSummary();
            return true;
        }
        toolbarStatusNote_ = insertedLineNumber > 0
            ? tr("Complete Draft wrote area geometry at source line %1.").arg(insertedLineNumber)
            : tr("Complete Draft wrote area geometry to source.");
    }

    clearInteractiveDrawSession(false);
    interactiveDrawMode_ = modeAtCommit;
    selectModeActive_ = false;
    toolbarStatusNote_ = modeAtCommit == InteractiveDrawMode::Line
        ? tr("Line committed. Line mode is still active for the next object.")
        : tr("Area committed. Area mode is still active for the next object.");
    refreshToolbarSummary();
    updateHelpPanel();
    updateCommandSurfaceState();
    return true;
}

void MapEditorTab::clearInteractiveDrawSession(bool clearMode)
{
    interactiveDrawSourceVertices_.clear();
    interactiveDrawSceneVertices_.clear();
    interactiveDrawLineVertices_.clear();
    interactiveDrawStrokeActive_ = false;
    interactiveDrawAnchorPressActive_ = false;
    interactiveDrawAnchorDragActive_ = false;
    interactiveDrawControlDragActive_ = false;
    interactiveDrawHoverActive_ = false;
    if (mapView_ != nullptr && mapView_->viewport() != nullptr && !mapPanActive_) {
        mapView_->viewport()->unsetCursor();
    }

    if (mapScene_ != nullptr) {
        if (interactiveDrawPreviewPath_ != nullptr) {
            mapScene_->removeItem(interactiveDrawPreviewPath_);
            delete interactiveDrawPreviewPath_;
            interactiveDrawPreviewPath_ = nullptr;
        }
        for (QGraphicsItem *marker : std::as_const(interactiveDrawPreviewMarkers_)) {
            if (marker != nullptr) {
                mapScene_->removeItem(marker);
                delete marker;
            }
        }
    }
    interactiveDrawPreviewMarkers_.clear();
    interactiveDrawPreviewPath_ = nullptr;

    if (clearMode) {
        const bool modeChanged = interactiveDrawMode_ != InteractiveDrawMode::None;
        interactiveDrawMode_ = InteractiveDrawMode::None;
        selectModeActive_ = true;
        if (modeChanged) {
            emit modeStatusChanged();
        }
    }
}

void MapEditorTab::updateInteractiveDrawPreview()
{
    if (mapScene_ == nullptr) {
        return;
    }

    if (interactiveDrawPreviewPath_ != nullptr) {
        mapScene_->removeItem(interactiveDrawPreviewPath_);
        delete interactiveDrawPreviewPath_;
        interactiveDrawPreviewPath_ = nullptr;
    }
    for (QGraphicsItem *marker : std::as_const(interactiveDrawPreviewMarkers_)) {
        if (marker != nullptr) {
            mapScene_->removeItem(marker);
            delete marker;
        }
    }
    interactiveDrawPreviewMarkers_.clear();

    if (interactiveDrawMode_ != InteractiveDrawMode::Line
        && interactiveDrawMode_ != InteractiveDrawMode::Area
        && interactiveDrawMode_ != InteractiveDrawMode::Freehand) {
        return;
    }

    QColor accent;
    if (interactiveDrawMode_ == InteractiveDrawMode::Line) {
        accent = QColor(QStringLiteral("#ffb15a"));
    } else if (interactiveDrawMode_ == InteractiveDrawMode::Area) {
        accent = QColor(QStringLiteral("#ff7f8f"));
    } else {
        accent = QColor(QStringLiteral("#66d38f"));
    }
    accent.setAlpha(220);
    QColor controlColor = QColor(QStringLiteral("#33a8ff"));
    controlColor.setAlpha(225);

    QPainterPath path;
    QVector<QPointF> anchorMarkers;
    QVector<QPointF> controlMarkers;
    QVector<QLineF> controlConnectors;
    if (interactiveDrawMode_ == InteractiveDrawMode::Line
        || interactiveDrawMode_ == InteractiveDrawMode::Area) {
        struct DraftPreviewVertex
        {
            QPointF anchorScene;
            std::optional<QPointF> incomingControlScene;
            std::optional<QPointF> outgoingControlScene;
        };

        QVector<DraftPreviewVertex> previewVertices;
        previewVertices.reserve(interactiveDrawLineVertices_.size() + 1);
        for (const InteractiveLineDraftVertex &vertex : std::as_const(interactiveDrawLineVertices_)) {
            DraftPreviewVertex previewVertex;
            previewVertex.anchorScene = vertex.anchorScene;
            previewVertex.incomingControlScene = vertex.incomingControlScene;
            previewVertex.outgoingControlScene = vertex.outgoingControlScene;
            previewVertices.append(previewVertex);
            anchorMarkers.append(vertex.anchorScene);
        }

        auto appendBezierControlsForLastSegment = [&previewVertices](const QPointF &dragScenePoint) {
            if (previewVertices.size() < 2) {
                return;
            }
            DraftPreviewVertex &previous = previewVertices[previewVertices.size() - 2];
            DraftPreviewVertex &current = previewVertices[previewVertices.size() - 1];
            // Treat drag location as a quadratic control point, then elevate to cubic.
            // This avoids midpoint-coupled handles that feel artificially parallel.
            constexpr qreal quadraticToCubicFactor = 2.0 / 3.0;
            const QPointF quadraticControlScene = dragScenePoint;
            previous.outgoingControlScene = previous.anchorScene
                + ((quadraticControlScene - previous.anchorScene) * quadraticToCubicFactor);
            current.incomingControlScene = current.anchorScene
                + ((quadraticControlScene - current.anchorScene) * quadraticToCubicFactor);
            if (previous.incomingControlScene.has_value()) {
                const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(previous.anchorScene,
                                                                                    previous.outgoingControlScene.value(),
                                                                                    previous.incomingControlScene);
                if (mirrored.has_value()) {
                    previous.incomingControlScene = mirrored.value();
                }
            }
        };

        if (interactiveDrawAnchorPressActive_) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = interactiveDrawAnchorPressScenePoint_;
            previewVertices.append(candidate);
            anchorMarkers.append(candidate.anchorScene);
            if (interactiveDrawAnchorDragActive_) {
                appendBezierControlsForLastSegment(interactiveDrawAnchorDragScenePoint_);
            }
        } else if (interactiveDrawHoverActive_ && !previewVertices.isEmpty()) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = interactiveDrawHoverScenePoint_;
            previewVertices.append(candidate);
        }

        if (!previewVertices.isEmpty()) {
            path.moveTo(previewVertices.first().anchorScene);
            for (int index = 1; index < previewVertices.size(); ++index) {
                const DraftPreviewVertex &previous = previewVertices.at(index - 1);
                const DraftPreviewVertex &current = previewVertices.at(index);
                const QPointF control1 = previous.outgoingControlScene.value_or(previous.anchorScene);
                const QPointF control2 = current.incomingControlScene.value_or(current.anchorScene);
                const bool curved = previous.outgoingControlScene.has_value() || current.incomingControlScene.has_value();
                if (curved) {
                    path.cubicTo(control1, control2, current.anchorScene);
                } else {
                    path.lineTo(current.anchorScene);
                }
            }

            for (const DraftPreviewVertex &vertex : std::as_const(previewVertices)) {
                if (vertex.incomingControlScene.has_value()) {
                    controlConnectors.append(QLineF(vertex.anchorScene, vertex.incomingControlScene.value()));
                    controlMarkers.append(vertex.incomingControlScene.value());
                }
                if (vertex.outgoingControlScene.has_value()) {
                    controlConnectors.append(QLineF(vertex.anchorScene, vertex.outgoingControlScene.value()));
                    controlMarkers.append(vertex.outgoingControlScene.value());
                }
            }
        }
        if (interactiveDrawMode_ == InteractiveDrawMode::Area && previewVertices.size() >= 3) {
            const DraftPreviewVertex &first = previewVertices.first();
            const DraftPreviewVertex &last = previewVertices.last();

            std::optional<QPointF> closingOutgoing = last.outgoingControlScene;
            if (!closingOutgoing.has_value() && last.incomingControlScene.has_value()) {
                closingOutgoing = mirroredSmoothControlPoint(last.anchorScene,
                                                             last.incomingControlScene.value(),
                                                             std::nullopt);
            }

            std::optional<QPointF> closingIncoming = first.incomingControlScene;
            if (!closingIncoming.has_value() && first.outgoingControlScene.has_value()) {
                closingIncoming = mirroredSmoothControlPoint(first.anchorScene,
                                                             first.outgoingControlScene.value(),
                                                             std::nullopt);
            }

            const bool curvedClose = closingOutgoing.has_value() || closingIncoming.has_value();
            if (curvedClose) {
                path.cubicTo(closingOutgoing.value_or(last.anchorScene),
                             closingIncoming.value_or(first.anchorScene),
                             first.anchorScene);
                if (closingOutgoing.has_value()) {
                    controlConnectors.append(QLineF(last.anchorScene, closingOutgoing.value()));
                    controlMarkers.append(closingOutgoing.value());
                }
                if (closingIncoming.has_value()) {
                    controlConnectors.append(QLineF(first.anchorScene, closingIncoming.value()));
                    controlMarkers.append(closingIncoming.value());
                }
            } else {
                path.lineTo(first.anchorScene);
            }
        }
    } else {
        if (interactiveDrawSceneVertices_.isEmpty()) {
            return;
        }
        QVector<QPointF> displayVertices = interactiveDrawSceneVertices_;
        if (interactiveDrawHoverActive_) {
            displayVertices.append(interactiveDrawHoverScenePoint_);
        }

        path = QPainterPath(displayVertices.first());
        for (int index = 1; index < displayVertices.size(); ++index) {
            path.lineTo(displayVertices.at(index));
        }
        if (interactiveDrawMode_ == InteractiveDrawMode::Area && displayVertices.size() >= 3) {
            path.closeSubpath();
        }
        anchorMarkers = interactiveDrawSceneVertices_;
    }

    interactiveDrawPreviewPath_ = new QGraphicsPathItem(path);
    interactiveDrawPreviewPath_->setPen(QPen(accent, 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    interactiveDrawPreviewPath_->setBrush(Qt::NoBrush);
    interactiveDrawPreviewPath_->setZValue(28.0);
    interactiveDrawPreviewPath_->setAcceptedMouseButtons(Qt::NoButton);
    mapScene_->addItem(interactiveDrawPreviewPath_);

    for (const QPointF &vertex : std::as_const(anchorMarkers)) {
        auto *marker = new QGraphicsEllipseItem(QRectF(vertex.x() - 3.2, vertex.y() - 3.2, 6.4, 6.4));
        marker->setPen(QPen(accent.darker(130), 1.0));
        marker->setBrush(QBrush(accent));
        marker->setZValue(28.5);
        marker->setAcceptedMouseButtons(Qt::NoButton);
        mapScene_->addItem(marker);
        interactiveDrawPreviewMarkers_.append(marker);
    }

    for (const QLineF &connector : std::as_const(controlConnectors)) {
        auto *connectorItem = new QGraphicsLineItem(connector);
        connectorItem->setPen(QPen(controlColor.lighter(115), 1.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        connectorItem->setZValue(28.4);
        connectorItem->setAcceptedMouseButtons(Qt::NoButton);
        mapScene_->addItem(connectorItem);
        interactiveDrawPreviewMarkers_.append(connectorItem);
    }

    for (const QPointF &control : std::as_const(controlMarkers)) {
        auto *marker = new QGraphicsEllipseItem(QRectF(control.x() - 4.0, control.y() - 4.0, 8.0, 8.0));
        marker->setPen(QPen(controlColor.darker(150), 1.2));
        marker->setBrush(QBrush(controlColor));
        marker->setZValue(28.6);
        marker->setAcceptedMouseButtons(Qt::NoButton);
        mapScene_->addItem(marker);
        interactiveDrawPreviewMarkers_.append(marker);
    }
}

QPointF MapEditorTab::sourcePointFromScenePosition(const QPointF &scenePosition) const
{
    if (textEditor_ == nullptr) {
        return scenePosition;
    }

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid()) {
        return scenePosition;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QRectF sourceBounds = geometryBoundsForFeatures(features);
    if (!sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return scenePosition;
    }

    return previewToSourcePoint(scenePosition, sourceBounds, previewBounds);
}

bool MapEditorTab::hasCompletableInteractiveDrawSession() const
{
    if (interactiveDrawMode_ == InteractiveDrawMode::Line) {
        return interactiveDrawLineVertices_.size() >= 2;
    }
    if (interactiveDrawMode_ == InteractiveDrawMode::Area) {
        return interactiveDrawLineVertices_.size() >= 3;
    }
    return false;
}

QStringList MapEditorTab::lineCoordinateRowsForInteractiveDraft() const
{
    QStringList rows;
    if (interactiveDrawLineVertices_.isEmpty()) {
        return rows;
    }

    rows.reserve(interactiveDrawLineVertices_.size());
    const auto formatPoint = [](const QPointF &point) {
        return QStringLiteral("%1 %2")
            .arg(formatSourceCoordinate(point.x()), formatSourceCoordinate(point.y()));
    };

    rows.append(formatPoint(interactiveDrawLineVertices_.first().anchorSource));
    for (int index = 1; index < interactiveDrawLineVertices_.size(); ++index) {
        const InteractiveLineDraftVertex &previous = interactiveDrawLineVertices_.at(index - 1);
        const InteractiveLineDraftVertex &current = interactiveDrawLineVertices_.at(index);
        QStringList tokens;
        if (previous.outgoingControlSource.has_value() || current.incomingControlSource.has_value()) {
            const QPointF control1 = previous.outgoingControlSource.value_or(previous.anchorSource);
            const QPointF control2 = current.incomingControlSource.value_or(current.anchorSource);
            tokens << formatSourceCoordinate(control1.x())
                   << formatSourceCoordinate(control1.y())
                   << formatSourceCoordinate(control2.x())
                   << formatSourceCoordinate(control2.y());
        }
        tokens << formatSourceCoordinate(current.anchorSource.x())
               << formatSourceCoordinate(current.anchorSource.y());
        rows.append(tokens.join(QLatin1Char(' ')));
    }

    return rows;
}

QStringList MapEditorTab::areaCoordinateRowsForInteractiveDraft() const
{
    QStringList rows = lineCoordinateRowsForInteractiveDraft();
    if (interactiveDrawLineVertices_.size() < 3) {
        return rows;
    }

    const InteractiveLineDraftVertex &first = interactiveDrawLineVertices_.first();
    const InteractiveLineDraftVertex &last = interactiveDrawLineVertices_.last();

    std::optional<QPointF> closingOutgoing = last.outgoingControlSource;
    if (!closingOutgoing.has_value() && last.incomingControlSource.has_value()) {
        closingOutgoing = mirroredSmoothControlPoint(last.anchorSource,
                                                     last.incomingControlSource.value(),
                                                     std::nullopt);
    }

    std::optional<QPointF> closingIncoming = first.incomingControlSource;
    if (!closingIncoming.has_value() && first.outgoingControlSource.has_value()) {
        closingIncoming = mirroredSmoothControlPoint(first.anchorSource,
                                                     first.outgoingControlSource.value(),
                                                     std::nullopt);
    }

    if (!closingOutgoing.has_value() && !closingIncoming.has_value()) {
        return rows;
    }

    QStringList tokens;
    const QPointF control1 = closingOutgoing.value_or(last.anchorSource);
    const QPointF control2 = closingIncoming.value_or(first.anchorSource);
    tokens << formatSourceCoordinate(control1.x())
           << formatSourceCoordinate(control1.y())
           << formatSourceCoordinate(control2.x())
           << formatSourceCoordinate(control2.y())
           << formatSourceCoordinate(first.anchorSource.x())
           << formatSourceCoordinate(first.anchorSource.y());
    rows.append(tokens.join(QLatin1Char(' ')));
    return rows;
}

void MapEditorTab::captureInteractiveLineAnchor(const QPointF &anchorScenePoint,
                                                const std::optional<QPointF> &dragScenePoint)
{
    InteractiveLineDraftVertex vertex;
    vertex.anchorScene = anchorScenePoint;
    vertex.anchorSource = sourcePointFromScenePosition(anchorScenePoint);
    interactiveDrawLineVertices_.append(vertex);

    if (dragScenePoint.has_value() && interactiveDrawLineVertices_.size() >= 2) {
        InteractiveLineDraftVertex &previous = interactiveDrawLineVertices_[interactiveDrawLineVertices_.size() - 2];
        InteractiveLineDraftVertex &current = interactiveDrawLineVertices_.last();
        constexpr qreal quadraticToCubicFactor = 2.0 / 3.0;
        const QPointF quadraticControlScene = dragScenePoint.value();
        previous.outgoingControlScene = previous.anchorScene
            + ((quadraticControlScene - previous.anchorScene) * quadraticToCubicFactor);
        current.incomingControlScene = current.anchorScene
            + ((quadraticControlScene - current.anchorScene) * quadraticToCubicFactor);
        previous.outgoingControlSource = sourcePointFromScenePosition(previous.outgoingControlScene.value());
        current.incomingControlSource = sourcePointFromScenePosition(current.incomingControlScene.value());
        if (previous.incomingControlScene.has_value()) {
            const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(previous.anchorScene,
                                                                                previous.outgoingControlScene.value(),
                                                                                previous.incomingControlScene);
            if (mirrored.has_value()) {
                previous.incomingControlScene = mirrored.value();
                previous.incomingControlSource = sourcePointFromScenePosition(mirrored.value());
            }
        }
    }

    updateInteractiveDrawPreview();
}

std::optional<MapEditorTab::InteractiveLineControlHandleRef> MapEditorTab::interactiveLineControlAt(
    const QPointF &scenePosition,
    qreal sceneRadius) const
{
    if (sceneRadius <= 0.0) {
        return std::nullopt;
    }

    qreal bestDistance = sceneRadius;
    std::optional<InteractiveLineControlHandleRef> bestHandle;
    for (int index = 0; index < interactiveDrawLineVertices_.size(); ++index) {
        const InteractiveLineDraftVertex &vertex = interactiveDrawLineVertices_.at(index);
        if (vertex.incomingControlScene.has_value()) {
            const qreal distance = QLineF(scenePosition, vertex.incomingControlScene.value()).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestHandle = InteractiveLineControlHandleRef{index, InteractiveLineControlHandleRef::Kind::Incoming};
            }
        }
        if (vertex.outgoingControlScene.has_value()) {
            const qreal distance = QLineF(scenePosition, vertex.outgoingControlScene.value()).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestHandle = InteractiveLineControlHandleRef{index, InteractiveLineControlHandleRef::Kind::Outgoing};
            }
        }
    }

    return bestHandle;
}

bool MapEditorTab::setInteractiveLineControlScenePoint(const InteractiveLineControlHandleRef &handle,
                                                       const QPointF &scenePoint)
{
    if (handle.vertexIndex < 0 || handle.vertexIndex >= interactiveDrawLineVertices_.size()) {
        return false;
    }

    InteractiveLineDraftVertex &vertex = interactiveDrawLineVertices_[handle.vertexIndex];
    if (handle.kind == InteractiveLineControlHandleRef::Kind::Incoming) {
        vertex.incomingControlScene = scenePoint;
        vertex.incomingControlSource = sourcePointFromScenePosition(scenePoint);
        if (vertex.outgoingControlScene.has_value()) {
            const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(vertex.anchorScene,
                                                                                scenePoint,
                                                                                vertex.outgoingControlScene);
            if (mirrored.has_value()) {
                vertex.outgoingControlScene = mirrored.value();
                vertex.outgoingControlSource = sourcePointFromScenePosition(mirrored.value());
            }
        }
        return true;
    }

    vertex.outgoingControlScene = scenePoint;
    vertex.outgoingControlSource = sourcePointFromScenePosition(scenePoint);
    if (vertex.incomingControlScene.has_value()) {
        const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(vertex.anchorScene,
                                                                            scenePoint,
                                                                            vertex.incomingControlScene);
        if (mirrored.has_value()) {
            vertex.incomingControlScene = mirrored.value();
            vertex.incomingControlSource = sourcePointFromScenePosition(mirrored.value());
        }
    }
    return true;
}

bool MapEditorTab::commitInteractiveDrawVertices(const QString &geometryKind,
                                                 const QVector<QPointF> &vertices,
                                                 const QString &successLabel)
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        return false;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    if (!textEditor_->insertDraftGeometry(geometryKind, vertices, &insertedLineNumber, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(successLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(successLabel);
    return true;
}

bool MapEditorTab::cancelInteractiveDrawingToSelectMode()
{
    if (interactiveDrawMode_ == InteractiveDrawMode::None) {
        return false;
    }

    const InteractiveDrawMode modeAtCancel = interactiveDrawMode_;
    const bool isLineOrArea = modeAtCancel == InteractiveDrawMode::Line
        || modeAtCancel == InteractiveDrawMode::Area;
    bool committedLineOrAreaDraft = false;
    if (isLineOrArea && hasCompletableInteractiveDrawSession()) {
        if (modeAtCancel == InteractiveDrawMode::Line) {
            QString errorMessage;
            int insertedLineNumber = 0;
            if (!textEditor_->insertDraftLineGeometry(lineCoordinateRowsForInteractiveDraft(),
                                                      &insertedLineNumber,
                                                      &errorMessage)) {
                toolbarStatusNote_ = errorMessage.isEmpty()
                    ? tr("Complete Draft failed.")
                    : tr("Complete Draft failed: %1").arg(errorMessage);
                refreshToolbarSummary();
                updateCommandSurfaceState();
                updateHelpPanel();
                return false;
            }
        } else {
            QString errorMessage;
            int insertedLineNumber = 0;
            if (!textEditor_->insertDraftAreaGeometry(areaCoordinateRowsForInteractiveDraft(),
                                                      &insertedLineNumber,
                                                      &errorMessage)) {
                toolbarStatusNote_ = errorMessage.isEmpty()
                    ? tr("Complete Draft failed.")
                    : tr("Complete Draft failed: %1").arg(errorMessage);
                refreshToolbarSummary();
                updateCommandSurfaceState();
                updateHelpPanel();
                return false;
            }
        }
        committedLineOrAreaDraft = true;
    }

    clearInteractiveDrawSession(true);
    interactiveDrawStrokeActive_ = false;
    if (mapView_ != nullptr) {
        mapView_->setDragMode(QGraphicsView::NoDrag);
    }

    if (committedLineOrAreaDraft) {
        toolbarStatusNote_ = tr("Selection mode: draft committed.");
    } else if (isLineOrArea) {
        toolbarStatusNote_ = tr("Selection mode: draft canceled.");
    } else {
        toolbarStatusNote_ = tr("Selection mode: drawing canceled.");
    }
    refreshToolbarSummary();
    updateCommandSurfaceState();
    updateHelpPanel();
    return true;
}

void MapEditorTab::updateCommandSurfaceState()
{
    if (undoButton_ != nullptr) {
        undoButton_->setEnabled(undoStack_ != nullptr && undoStack_->canUndo());
    }
    if (redoButton_ != nullptr) {
        redoButton_->setEnabled(undoStack_ != nullptr && undoStack_->canRedo());
    }
    if (zoomLabel_ != nullptr) {
        zoomLabel_->setText(tr("%1%").arg(qRound(zoomFactor_ * 100.0)));
    }
    if (pointButton_ != nullptr) {
        const bool mapReady = mapScene_ != nullptr;
        selectButton_->setEnabled(mapReady);
        pointButton_->setEnabled(mapReady);
        lineButton_->setEnabled(mapReady);
        freehandLineButton_->setEnabled(mapReady);
        smartTraceLineButton_->setEnabled(mapReady);
        areaButton_->setEnabled(mapReady);
        insertScrapButton_->setEnabled(mapReady);
        completeDraftButton_->setEnabled(mapReady
                                         && (selectedDraftGeometryItem() != nullptr
                                             || hasCompletableInteractiveDrawSession()));
        fitBackgroundButton_->setEnabled(mapReady);
    }
    if (touchControlsButton_ != nullptr) {
        touchControlsButton_->setChecked(touchFriendlyControlsEnabled_);
        touchControlsButton_->setText(touchFriendlyControlsEnabled_
                                          ? tr("Touch Controls: On")
                                          : tr("Touch Controls: Off"));
    }
    if (cancelDrawShortcut_ != nullptr) {
        cancelDrawShortcut_->setEnabled(interactiveDrawMode_ != InteractiveDrawMode::None);
    }
    if (commitDrawShortcut_ != nullptr) {
        commitDrawShortcut_->setEnabled(interactiveDrawMode_ == InteractiveDrawMode::Line
                                        || interactiveDrawMode_ == InteractiveDrawMode::Area);
    }
    refreshBackgroundLayerControls();
    refreshToolbarSummary();
}

void MapEditorTab::updateHelpPanel()
{
    if (helpBrowser_ == nullptr) {
        return;
    }

    if (helpPanel_ != nullptr) {
        helpPanel_->setToolTip(helpCollapsed_ ? tr("Expand map help") : tr("Map help and selection guidance"));
    }

    if (mapScene_ == nullptr) {
        helpBrowser_->setHtml(mapWorkspaceHelpHtml());
        return;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.isEmpty()) {
        helpBrowser_->setHtml(mapWorkspaceHelpHtml());
        return;
    }

    if (auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(selectedItems.first())) {
        QString html;
        html += QStringLiteral("<h3 style='margin:0 0 6px 0;'>Draft Geometry</h3>");
        html += QStringLiteral("<p style='margin:0 0 4px 0;'><strong>Kind:</strong> %1</p>").arg(draftGeometryLabel(draftItem->kind()).toHtmlEscaped());
        html += QStringLiteral("<p style='margin:0 0 4px 0;'><strong>Status:</strong> %1</p>").arg(draftItem->isDraftComplete() ? tr("Complete") : tr("Draft"));
        html += QStringLiteral("<p style='margin:8px 0 0 0;'>Drag the item to reposition it, or use Complete Draft to finish it.</p>");
        helpBrowser_->setHtml(html);
        return;
    }

    if (auto *card = dynamic_cast<MapCardItem *>(selectedItems.first())) {
        QString html;
        html += QStringLiteral("<h3 style='margin:0 0 6px 0;'>%1</h3>").arg(card->category().toHtmlEscaped());
        html += QStringLiteral("<p style='margin:0 0 4px 0;'><strong>Source line:</strong> %1</p>").arg(card->lineNumber());
        html += QStringLiteral("<p style='margin:8px 0 0 0;'>Select a card to jump to the source line and keep the text and map in sync.</p>");
        helpBrowser_->setHtml(html);
        return;
    }

    helpBrowser_->setHtml(mapWorkspaceHelpHtml());
}

void MapEditorTab::clearMapScene()
{
    mapItemsByLine_.clear();
    interactiveDrawStrokeActive_ = false;
    interactiveDrawPreviewPath_ = nullptr;
    interactiveDrawPreviewMarkers_.clear();
    if (mapScene_ == nullptr) {
        return;
    }
    QScopedValueRollback<bool> selectionGuard(updatingSelection_, true);

    QVector<QGraphicsRectItem *> preservedDrafts;
    QVector<QGraphicsPixmapItem *> preservedBackgrounds;
    const QList<QGraphicsItem *> items = mapScene_->items();
    for (QGraphicsItem *item : items) {
        if (auto *backgroundItem = dynamic_cast<QGraphicsPixmapItem *>(item)) {
            mapScene_->removeItem(backgroundItem);
            preservedBackgrounds.append(backgroundItem);
            continue;
        }
        if (auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item)) {
            mapScene_->removeItem(draftItem);
            preservedDrafts.append(draftItem);
            continue;
        }

        mapScene_->removeItem(item);
        delete item;
    }

    draftGeometryItems_ = preservedDrafts;
    backgroundImageItems_ = preservedBackgrounds;
}

void MapEditorTab::clearDraftGeometryItems()
{
    if (mapScene_ != nullptr) {
        for (QGraphicsRectItem *item : std::as_const(draftGeometryItems_)) {
            if (item != nullptr) {
                mapScene_->removeItem(item);
                delete item;
            }
        }
    }

    draftGeometryItems_.clear();
    nextDraftGeometryId_ = 1;
}

void MapEditorTab::clearBackgroundImageItems()
{
    if (mapScene_ != nullptr) {
        for (QGraphicsPixmapItem *item : std::as_const(backgroundImageItems_)) {
            if (item != nullptr) {
                mapScene_->removeItem(item);
                delete item;
            }
        }
    }

    backgroundImageItems_.clear();
    selectedBackgroundLayerIndex_ = -1;
    refreshBackgroundLayerControls();
}

void MapEditorTab::restoreDraftGeometryItems()
{
    if (mapScene_ == nullptr) {
        return;
    }

    for (QGraphicsRectItem *item : std::as_const(draftGeometryItems_)) {
        if (item == nullptr || mapScene_->items().contains(item)) {
            continue;
        }

        mapScene_->addItem(item);
        item->setZValue(20.0);
    }
}

void MapEditorTab::restoreBackgroundImageItems()
{
    if (mapScene_ == nullptr) {
        return;
    }

    for (QGraphicsPixmapItem *item : std::as_const(backgroundImageItems_)) {
        if (item == nullptr || mapScene_->items().contains(item)) {
            continue;
        }

        mapScene_->addItem(item);
    }

    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(selectedBackgroundLayerIndex_);
    refreshBackgroundLayerControls();
}

void MapEditorTab::fitMapToView(bool includeBackgroundImages)
{
    if (mapScene_ == nullptr || mapView_ == nullptr) {
        return;
    }

    QRectF fitBounds = mapGeometryFitBounds();
    if (includeBackgroundImages) {
        const QRectF backgroundBounds = mapBackgroundFitBounds();
        if (backgroundBounds.isValid()) {
            fitBounds = fitBounds.isValid() ? fitBounds.united(backgroundBounds) : backgroundBounds;
        }
    }
    if (!fitBounds.isValid()) {
        if (mapScene_->items().isEmpty()) {
            return;
        }

        fitBounds = mapScene_->itemsBoundingRect();
    }

    mapView_->resetTransform();
    mapView_->fitInView(fitBounds.adjusted(-24, -24, 24, 24), Qt::KeepAspectRatio);
    autoFitEnabled_ = true;
    syncZoomFactorFromView();
    updateCommandSurfaceState();
}

void MapEditorTab::syncZoomFactorFromView()
{
    if (mapView_ == nullptr) {
        zoomFactor_ = 1.0;
        return;
    }

    const QTransform viewTransform = mapView_->transform();
    const qreal scaleX = std::hypot(viewTransform.m11(), viewTransform.m21());
    zoomFactor_ = scaleX > 0.0 ? scaleX : 1.0;
}

void MapEditorTab::applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition)
{
    if (mapView_ == nullptr || factor <= 0.0) {
        return;
    }

    syncZoomFactorFromView();
    const qreal targetZoomFactor = qBound(0.1, zoomFactor_ * factor, 50.0);
    if (qFuzzyCompare(targetZoomFactor, zoomFactor_)) {
        return;
    }

    const qreal appliedFactor = targetZoomFactor / zoomFactor_;
    const QPoint viewportPoint = viewportPosition.toPoint();
    const QPointF scenePointBefore = mapView_->mapToScene(viewportPoint);
    mapView_->scale(appliedFactor, appliedFactor);
    const QPointF scenePointAfter = mapView_->mapToScene(viewportPoint);
    const QPointF sceneDelta = scenePointAfter - scenePointBefore;
    mapView_->translate(sceneDelta.x(), sceneDelta.y());

    autoFitEnabled_ = false;
    syncZoomFactorFromView();
    updateCommandSurfaceState();
}

void MapEditorTab::refreshToolbarSummary()
{
    if (summaryLabel_ == nullptr) {
        return;
    }

    QString summary = toolbarStatusNote_.trimmed();
    if (summary.isEmpty()) {
        summary = tr("Ready");
    }

    if (fitBackgroundRequested_) {
        summary += tr(" (fit includes background layers)");
    }

    if (!backgroundImageItems_.isEmpty()) {
        summary += tr(" | Backgrounds: %1").arg(backgroundImageItems_.size());
    }
    if (touchFriendlyControlsEnabled_) {
        summary += tr(" | Touch Controls: On");
    }

    summaryLabel_->setText(summary);
}

QRectF MapEditorTab::mapGeometryFitBounds() const
{
    if (mapScene_ == nullptr) {
        return QRectF();
    }

    QRectF bounds;
    bool hasBounds = false;
    const QList<QGraphicsItem *> items = mapScene_->items();
    for (QGraphicsItem *item : items) {
        if (item == nullptr) {
            continue;
        }
        if (item->data(kMapItemRole).toInt() != kMapItemGeometryValue) {
            continue;
        }

        const QRectF itemBounds = item->sceneBoundingRect();
        if (!itemBounds.isValid()) {
            continue;
        }

        if (!hasBounds) {
            bounds = itemBounds;
            hasBounds = true;
        } else {
            bounds = bounds.united(itemBounds);
        }
    }

    return hasBounds ? bounds : QRectF();
}

QRectF MapEditorTab::mapPreviewBounds() const
{
    if (mapScene_ == nullptr) {
        return QRectF();
    }

    const QRectF sceneFrame = mapScene_->sceneRect();
    if (sceneFrame.width() <= 80.0 || sceneFrame.height() <= 80.0) {
        return QRectF();
    }

    const QRectF geometryCanvas = sceneFrame.adjusted(24.0, 24.0, -24.0, -24.0);
    return geometryCanvas.adjusted(20.0, 20.0, -20.0, -20.0);
}

void MapEditorTab::adjustMapZoom(qreal factor)
{
    if (mapView_ == nullptr) {
        return;
    }

    applyZoomAtViewportPosition(factor, mapView_->viewport()->rect().center());
}

void MapEditorTab::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    if (undoStack_ == nullptr || oldPosition == newPosition) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(mapItemsByLine_.value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    undoStack_->push(createMapCardMoveCommand(card, oldPosition, newPosition));
}

void MapEditorTab::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    if (undoStack_ == nullptr || oldVisible == newVisible) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(mapItemsByLine_.value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    undoStack_->push(createMapCardVisibilityCommand(card, oldVisible, newVisible));
}

void MapEditorTab::recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint)
{
    if (lineNumber <= 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || textEditor_ == nullptr) {
        return;
    }

    auto statusCallback = [this](const QString &statusMessage) {
        toolbarStatusNote_ = statusMessage;
        refreshToolbarSummary();
    };

    if (undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->push(new MapPointGeometryMoveCommand(textEditor_,
                                                         lineNumber,
                                                         oldPoint,
                                                         newPoint,
                                                         statusCallback));
        flushPendingMapSceneRefreshAfterCommand();
        return;
    }

    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    MapPointGeometryMoveCommand directCommand(textEditor_, lineNumber, oldPoint, newPoint, statusCallback);
    directCommand.redo();
    flushPendingMapSceneRefreshAfterCommand();
}

void MapEditorTab::recordLineAreaVertexMove(int lineNumber,
                                            const QString &kind,
                                            int vertexIndex,
                                            const QPointF &oldPoint,
                                            const QPointF &newPoint)
{
    if (lineNumber <= 0 || vertexIndex < 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || textEditor_ == nullptr) {
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    const QString rewriteKind = normalizedKind.startsWith(QStringLiteral("line"))
        ? QStringLiteral("line")
        : (normalizedKind.startsWith(QStringLiteral("area")) ? QStringLiteral("area") : normalizedKind);
    if (rewriteKind != QStringLiteral("line") && rewriteKind != QStringLiteral("area")) {
        toolbarStatusNote_ = tr("Geometry move failed: unsupported geometry kind '%1'.").arg(kind);
        refreshToolbarSummary();
        return;
    }

    QVector<MapLineAreaVertexMoveCommand::SecondaryMove> secondaryMoves;
    if (rewriteKind == QStringLiteral("line")) {
        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(), lineNumber);
        if (lineFeature.has_value()) {
            secondaryMoves = coupledMovesForLineVertex(lineFeature.value(), vertexIndex, oldPoint, newPoint).secondaryMoves;
            for (auto it = secondaryMoves.begin(); it != secondaryMoves.end();) {
                if (it->vertexIndex == vertexIndex) {
                    it = secondaryMoves.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    auto statusCallback = [this](const QString &statusMessage) {
        toolbarStatusNote_ = statusMessage;
        refreshToolbarSummary();
    };

    if (undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
        undoStack_->push(new MapLineAreaVertexMoveCommand(textEditor_,
                                                          lineNumber,
                                                          rewriteKind,
                                                          vertexIndex,
                                                          oldPoint,
                                                          newPoint,
                                                          secondaryMoves,
                                                          statusCallback));
        flushPendingMapSceneRefreshAfterCommand();
        return;
    }

    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    MapLineAreaVertexMoveCommand directCommand(textEditor_,
                                               lineNumber,
                                               rewriteKind,
                                               vertexIndex,
                                               oldPoint,
                                               newPoint,
                                               secondaryMoves,
                                               statusCallback);
    directCommand.redo();
    flushPendingMapSceneRefreshAfterCommand();
}

bool MapEditorTab::insertLineVertexFromSelection()
{
    if (mapScene_ == nullptr || textEditor_ == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(), lineNumber);
    if (!lineFeature.has_value()) {
        toolbarStatusNote_ = tr("Insert vertex failed: line geometry could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (anchorIndex < 0) {
        toolbarStatusNote_ = tr("Insert vertex failed: selected line anchor could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    int segmentStartIndex = -1;
    if (anchorIndex < lineFeature->lineVertices.size() - 1) {
        segmentStartIndex = anchorIndex;
    } else if (anchorIndex > 0) {
        segmentStartIndex = anchorIndex - 1;
    }
    if (segmentStartIndex < 0) {
        toolbarStatusNote_ = tr("Insert vertex failed: line needs at least one editable segment.");
        refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    int insertedIndex = -1;
    if (!insertLineVertexByDeCasteljau(&editedVertices, segmentStartIndex, 0.5, &insertedIndex)) {
        toolbarStatusNote_ = tr("Insert vertex failed: segment split could not be computed.");
        refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!textEditor_->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Insert vertex failed.")
            : tr("Insert vertex failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return true;
    }

    toolbarStatusNote_ = insertedIndex >= 0
        ? tr("Inserted line vertex %1 on source line %2.").arg(insertedIndex + 1).arg(lineNumber)
        : tr("Inserted line vertex on source line %1.").arg(lineNumber);
    refreshToolbarSummary();
    return true;
}

bool MapEditorTab::removeLineVertexFromSelection()
{
    if (mapScene_ == nullptr || textEditor_ == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(), lineNumber);
    if (!lineFeature.has_value()) {
        toolbarStatusNote_ = tr("Delete vertex failed: line geometry could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (anchorIndex < 0) {
        toolbarStatusNote_ = tr("Delete vertex failed: selected line anchor could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    if (!removeLineVertexWithReconnect(&editedVertices, anchorIndex)) {
        toolbarStatusNote_ = tr("Delete vertex failed: only middle anchors can be removed while keeping a valid line.");
        refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!textEditor_->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Delete vertex failed.")
            : tr("Delete vertex failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return true;
    }

    toolbarStatusNote_ = tr("Removed line vertex %1 on source line %2.").arg(anchorIndex + 1).arg(lineNumber);
    refreshToolbarSummary();
    return true;
}

bool MapEditorTab::toggleLineVertexSmoothFromSelection()
{
    if (mapScene_ == nullptr || textEditor_ == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(), lineNumber);
    if (!lineFeature.has_value()) {
        toolbarStatusNote_ = tr("Toggle smooth failed: line geometry could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (ownerIndex < 0 || ownerIndex >= lineFeature->lineVertices.size()) {
        toolbarStatusNote_ = tr("Toggle smooth failed: selected line vertex could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    MapGeometryFeature::TH2LineVertex &target = editedVertices[ownerIndex];
    target.isSmooth = !target.isSmooth;
    if (target.isSmooth) {
        if (target.incomingControl.has_value() && !target.outgoingControl.has_value()) {
            const QPointF vector = target.incomingControl.value() - target.anchor;
            target.outgoingControl = target.anchor - vector;
        } else if (target.outgoingControl.has_value() && !target.incomingControl.has_value()) {
            const QPointF vector = target.outgoingControl.value() - target.anchor;
            target.incomingControl = target.anchor - vector;
        }
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!textEditor_->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Toggle smooth failed.")
            : tr("Toggle smooth failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return true;
    }

    toolbarStatusNote_ = target.isSmooth
        ? tr("Line vertex %1 on source line %2 set to smooth.").arg(ownerIndex + 1).arg(lineNumber)
        : tr("Line vertex %1 on source line %2 set to corner (smooth off).").arg(ownerIndex + 1).arg(lineNumber);
    refreshToolbarSummary();
    return true;
}

QGraphicsRectItem *MapEditorTab::selectedDraftGeometryItem() const
{
    if (mapScene_ == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (auto *draftItem = dynamic_cast<QGraphicsRectItem *>(item)) {
            if (dynamic_cast<MapDraftGeometryItem *>(draftItem) != nullptr) {
                return draftItem;
            }
        }
    }

    return nullptr;
}

QGraphicsRectItem *MapEditorTab::createDraftGeometryItem(DraftGeometryKind kind)
{
    return new MapDraftGeometryItem(nextDraftGeometryId_++, kind);
}

void MapEditorTab::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftItem->setPos(position);
    draftItem->setMoveCommittedCallback([this, draftItem](int, const QPointF &oldPosition, const QPointF &newPosition) {
        recordDraftMove(draftItem, oldPosition, newPosition);
    });
    draftItem->setVisibilityCommittedCallback([this, draftItem](int, bool oldVisible, bool newVisible) {
        recordDraftVisibility(draftItem, oldVisible, newVisible);
    });
    if (undoStack_ != nullptr) {
        undoStack_->push(createMapDraftAddCommand(mapScene_, &draftGeometryItems_, draftItem, position));
    } else {
        if (mapScene_ != nullptr) {
            mapScene_->addItem(draftItem);
        }
        draftGeometryItems_.append(draftItem);
    }

    updateCommandSurfaceState();
}

void MapEditorTab::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftGeometryItems_.removeAll(draftItem);
    if (mapScene_ != nullptr) {
        mapScene_->removeItem(draftItem);
    }
    delete draftItem;
}

QVector<QPointF> MapEditorTab::sourceVerticesForDraft(const QGraphicsRectItem *item) const
{
    auto *draftItem = dynamic_cast<const MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return {};
    }

    const QRectF itemRect = draftItem->rect();
    const QPointF itemPosition = draftItem->pos();
    const QPointF itemCenter = itemPosition + itemRect.center();
    QVector<QPointF> previewVertices;

    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        previewVertices.append(itemCenter);
        break;
    case DraftGeometryKind::Line:
        previewVertices.append(itemPosition + QPointF(itemRect.left() + 16.0, itemRect.center().y()));
        previewVertices.append(itemPosition + QPointF(itemRect.right() - 16.0, itemRect.center().y()));
        break;
    case DraftGeometryKind::Area:
        previewVertices.append(itemPosition + QPointF(itemRect.center().x(), itemRect.top() + 16.0));
        previewVertices.append(itemPosition + QPointF(itemRect.right() - 16.0, itemRect.bottom() - 16.0));
        previewVertices.append(itemPosition + QPointF(itemRect.left() + 16.0, itemRect.bottom() - 16.0));
        break;
    }

    if (previewVertices.isEmpty()) {
        return {};
    }

    const QRectF previewBounds = mapPreviewBounds();
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_ != nullptr ? textEditor_->text() : QString());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QRectF sourceBounds = geometryBoundsForFeatures(features);
    if (!previewBounds.isValid() || !sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return previewVertices;
    }

    QVector<QPointF> sourceVertices;
    sourceVertices.reserve(previewVertices.size());
    for (const QPointF &previewVertex : std::as_const(previewVertices)) {
        sourceVertices.append(previewToSourcePoint(previewVertex, sourceBounds, previewBounds));
    }

    return sourceVertices;
}

QPointF MapEditorTab::previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const
{
    if (!sourceBounds.isValid() || !previewBounds.isValid() || previewBounds.width() < 0.001 || previewBounds.height() < 0.001) {
        return previewPoint;
    }

    // Use the shared inverse transform without clamping so drafted points
    // outside the fitted geometry strip keep their relative shape instead of
    // collapsing onto one boundary axis.
    return mapGeometryPreviewToSource(previewPoint, sourceBounds, previewBounds);
}

void MapEditorTab::recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || undoStack_ == nullptr || oldPosition == newPosition) {
        return;
    }

    undoStack_->push(createMapDraftMoveCommand(draftItem, oldPosition, newPosition));
}

void MapEditorTab::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || undoStack_ == nullptr || oldVisible == newVisible) {
        return;
    }

    undoStack_->push(createMapDraftVisibilityCommand(draftItem, oldVisible, newVisible));
}

void MapEditorTab::selectMapLine(int lineNumber)
{
    if (mapScene_ == nullptr) {
        return;
    }

    pendingMapClickSelection_ = false;
    pendingMapClickLineNumber_ = 0;
    pendingMapClickSourceVertexIndex_ = -1;
    pendingMapClickGeometryKind_.clear();
    updatingSelection_ = true;
    mapScene_->clearSelection();

    auto selectedItemIt = mapItemsByLine_.find(lineNumber);
    if (selectedItemIt != mapItemsByLine_.end() && selectedItemIt.value() != nullptr) {
        selectedItemIt.value()->setSelected(true);
        if (!autoFitEnabled_) {
            mapView_->centerOn(selectedItemIt.value());
        }
    }

    updatingSelection_ = false;
    updateGeometrySelectionPresentation();
}

void MapEditorTab::selectMapLines(const QSet<int> &lineNumbers)
{
    if (mapScene_ == nullptr) {
        return;
    }

    pendingMapClickSelection_ = false;
    pendingMapClickLineNumber_ = 0;
    pendingMapClickSourceVertexIndex_ = -1;
    pendingMapClickGeometryKind_.clear();
    updatingSelection_ = true;
    mapScene_->clearSelection();

    QList<int> sortedLines = lineNumbers.values();
    std::sort(sortedLines.begin(), sortedLines.end());
    QGraphicsItem *firstSelectedItem = nullptr;
    for (const int lineNumber : std::as_const(sortedLines)) {
        auto selectedItemIt = mapItemsByLine_.find(lineNumber);
        if (selectedItemIt == mapItemsByLine_.end() || selectedItemIt.value() == nullptr) {
            continue;
        }
        selectedItemIt.value()->setSelected(true);
        if (firstSelectedItem == nullptr) {
            firstSelectedItem = selectedItemIt.value();
        }
    }

    if (firstSelectedItem != nullptr && !autoFitEnabled_ && mapView_ != nullptr) {
        mapView_->centerOn(firstSelectedItem);
    }

    updatingSelection_ = false;
    updateGeometrySelectionPresentation();
}

}
