#include "MapEditorTab.h"

#include <QFrame>
#include <QEvent>
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
#include <QPushButton>
#include <QScrollBar>
#include <QSplitterHandle>
#include <QTabletEvent>
#include <QTextBrowser>
#include <QToolButton>
#include <QPointer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QFont>

#include <cmath>
#include <optional>

#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"
#include "TextEditorTab.h"
#include "../core/TherionDocumentParser.h"

namespace TherionStudio
{
namespace
{
constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;
QRectF fittedPreviewBoundsForSource(const QRectF &sourceBounds, const QRectF &previewBounds)
{
    if (!sourceBounds.isValid() || !previewBounds.isValid()) {
        return previewBounds;
    }

    const qreal sourceWidth = qMax(1.0, sourceBounds.width());
    const qreal sourceHeight = qMax(1.0, sourceBounds.height());
    const qreal previewWidth = qMax(1.0, previewBounds.width());
    const qreal previewHeight = qMax(1.0, previewBounds.height());
    const qreal scale = qMin(previewWidth / sourceWidth, previewHeight / sourceHeight);
    const qreal fittedWidth = sourceWidth * scale;
    const qreal fittedHeight = sourceHeight * scale;
    const qreal left = previewBounds.left() + ((previewWidth - fittedWidth) / 2.0);
    const qreal top = previewBounds.top() + ((previewHeight - fittedHeight) / 2.0);
    return QRectF(left, top, fittedWidth, fittedHeight);
}

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
        if (!applyPoint(oldPoint_, QStringLiteral("Reverted point geometry at source line %1.").arg(lineNumber_))) {
            setObsolete(true);
        }
    }

    void redo() override
    {
        if (!applyPoint(newPoint_, QStringLiteral("Updated point geometry at source line %1.").arg(lineNumber_))) {
            setObsolete(true);
        }
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
        QVector<SecondaryMove> reversedSecondaryMoves;
        reversedSecondaryMoves.reserve(secondaryMoves_.size());
        for (const SecondaryMove &move : std::as_const(secondaryMoves_)) {
            SecondaryMove reversedMove = move;
            reversedMove.oldPoint = move.newPoint;
            reversedMove.newPoint = move.oldPoint;
            reversedSecondaryMoves.append(reversedMove);
        }

        if (!applyVertexWithSecondaries(oldPoint_,
                                        reversedSecondaryMoves,
                                        QStringLiteral("Reverted %1 vertex %2 at source line %3.")
                                            .arg(kind_)
                                            .arg(vertexIndex_ + 1)
                                            .arg(lineNumber_))) {
            setObsolete(true);
        }
    }

    void redo() override
    {
        if (!applyVertexWithSecondaries(newPoint_,
                                        secondaryMoves_,
                                        QStringLiteral("Updated %1 vertex %2 at source line %3.")
                                            .arg(kind_)
                                            .arg(vertexIndex_ + 1)
                                            .arg(lineNumber_))) {
            setObsolete(true);
        }
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
};

enum class LineVertexRole
{
    None,
    Anchor,
    IncomingControl,
    OutgoingControl
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
    if (sourceVertexIndex < 0 || lineFeature.lineVertices.isEmpty()) {
        return result;
    }

    constexpr qreal kEpsilon = 1e-6;
    auto appendSecondaryMove = [&](int vertexIndex, const QPointF &from, const QPointF &to) {
        if (vertexIndex < 0 || !sourcePointsDifferForCommands(from, to)) {
            return;
        }
        MapLineAreaVertexMoveCommand::SecondaryMove move;
        move.vertexIndex = vertexIndex;
        move.oldPoint = from;
        move.newPoint = to;
        result.secondaryMoves.append(move);
    };

    for (const MapGeometryFeature::TH2LineVertex &vertex : lineFeature.lineVertices) {
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex) {
            const QPointF delta = newPoint - oldPoint;
            if (!delta.isNull()) {
                if (vertex.incomingSourceVertexIndex >= 0) {
                    const QPointF oldIncoming = vertex.incomingControl.value_or(vertex.anchor);
                    appendSecondaryMove(vertex.incomingSourceVertexIndex, oldIncoming, oldIncoming + delta);
                }
                if (vertex.outgoingSourceVertexIndex >= 0) {
                    const QPointF oldOutgoing = vertex.outgoingControl.value_or(vertex.anchor);
                    appendSecondaryMove(vertex.outgoingSourceVertexIndex, oldOutgoing, oldOutgoing + delta);
                }
            }
            return result;
        }

        LineVertexRole role = LineVertexRole::None;
        if (vertex.incomingSourceVertexIndex == sourceVertexIndex) {
            role = LineVertexRole::IncomingControl;
        } else if (vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
            role = LineVertexRole::OutgoingControl;
        }
        if (role == LineVertexRole::None || !vertex.isSmooth) {
            continue;
        }

        const QPointF anchor = vertex.anchor;
        const QPointF vector = newPoint - anchor;
        qreal vectorLength = std::hypot(vector.x(), vector.y());
        QPointF direction(0.0, 0.0);
        if (vectorLength > kEpsilon) {
            direction = QPointF(vector.x() / vectorLength, vector.y() / vectorLength);
        }

        if (role == LineVertexRole::IncomingControl) {
            if (vertex.outgoingSourceVertexIndex < 0) {
                return result;
            }

            const QPointF oldOpposite = vertex.outgoingControl.value_or(anchor);
            const qreal oldOppositeLength = std::hypot(oldOpposite.x() - anchor.x(), oldOpposite.y() - anchor.y());
            const qreal targetLength = vertex.outgoingControl.has_value() ? oldOppositeLength : vectorLength;
            const QPointF newOpposite = (vectorLength > kEpsilon)
                ? QPointF(anchor.x() - (direction.x() * targetLength),
                          anchor.y() - (direction.y() * targetLength))
                : anchor;
            appendSecondaryMove(vertex.outgoingSourceVertexIndex, oldOpposite, newOpposite);
            return result;
        }

        if (role == LineVertexRole::OutgoingControl) {
            if (vertex.incomingSourceVertexIndex < 0) {
                return result;
            }

            const QPointF oldOpposite = vertex.incomingControl.value_or(anchor);
            const qreal oldOppositeLength = std::hypot(oldOpposite.x() - anchor.x(), oldOpposite.y() - anchor.y());
            const qreal targetLength = vertex.incomingControl.has_value() ? oldOppositeLength : vectorLength;
            const QPointF newOpposite = (vectorLength > kEpsilon)
                ? QPointF(anchor.x() - (direction.x() * targetLength),
                          anchor.y() - (direction.y() * targetLength))
                : anchor;
            appendSecondaryMove(vertex.incomingSourceVertexIndex, oldOpposite, newOpposite);
            return result;
        }
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

    QStringList tokens;
    tokens.reserve(lineVertices.size() * 6);
    const auto appendPoint = [&tokens](const QPointF &point) {
        tokens.append(formatSourceCoordinate(point.x()));
        tokens.append(formatSourceCoordinate(point.y()));
    };

    appendPoint(lineVertices.first().anchor);
    for (int index = 1; index < lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previous = lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &current = lineVertices.at(index);
        const bool cubic = previous.outgoingControl.has_value() || current.incomingControl.has_value();
        if (cubic) {
            appendPoint(previous.outgoingControl.value_or(previous.anchor));
            appendPoint(current.incomingControl.value_or(current.anchor));
        }
        appendPoint(current.anchor);
    }

    constexpr int kTokensPerRow = 10;
    for (int index = 0; index < tokens.size(); index += kTokensPerRow) {
        rows.append(tokens.mid(index, kTokensPerRow).join(QLatin1Char(' ')));
    }
    return rows;
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
                primaryPointerInteractionActive_ = true;
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

            const bool shouldZoom = cmdModifier || !preciseScroll;
            if (shouldZoom) {
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
            if (!selectModeActive_ || primaryPointerInteractionActive_) {
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
            break;
        case QEvent::Resize:
            if (autoFitEnabled_ && mapView_->isVisible()) {
                fitMapToView(fitBackgroundRequested_);
            }
            break;
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
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

void MapEditorTab::installHelpBorderToggle()
{
    if (mapHelpSplitter_ == nullptr || helpBorderToggleButton_ != nullptr) {
        return;
    }

    auto *handle = mapHelpSplitter_->handle(1);
    if (handle == nullptr) {
        return;
    }

    auto *handleLayout = new QHBoxLayout(handle);
    handleLayout->setContentsMargins(0, 0, 4, 0);
    handleLayout->setSpacing(0);
    handleLayout->addStretch(1);

    helpBorderToggleButton_ = new QToolButton(handle);
    helpBorderToggleButton_->setAutoRaise(true);
    helpBorderToggleButton_->setFocusPolicy(Qt::NoFocus);
    helpBorderToggleButton_->setFixedSize(12, 12);
    helpBorderToggleButton_->setToolTip(tr("Collapse or expand help pane"));
    connect(helpBorderToggleButton_, &QToolButton::clicked, this, [this]() {
        setHelpCollapsed(!helpCollapsed_);
    });

    handleLayout->addWidget(helpBorderToggleButton_);
    refreshHelpBorderToggle();
}

void MapEditorTab::refreshHelpBorderToggle()
{
    if (helpBorderToggleButton_ == nullptr) {
        return;
    }

    helpBorderToggleButton_->setArrowType(helpCollapsed_ ? Qt::RightArrow : Qt::DownArrow);
}

void MapEditorTab::refreshMapScene()
{
    if (mapScene_ == nullptr) {
        return;
    }

    if (undoStack_ != nullptr) {
        undoStack_->clear();
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
    if (autoFitEnabled_) {
        fitMapToView(fitBackgroundRequested_);
    } else {
        syncZoomFactorFromView();
    }
    refreshStatus();
    updateCommandSurfaceState();
    updateHelpPanel();
}

void MapEditorTab::handleMapSceneSelectionChanged()
{
    if (updatingSelection_ || mapScene_ == nullptr) {
        updateHelpPanel();
        return;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.isEmpty()) {
        updateHelpPanel();
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(selectedItems.first());
    if (card != nullptr) {
        if (card->lineNumber() > 0 && card->lineNumber() != currentLineNumber()) {
            textEditor_->goToLine(card->lineNumber());
        }

        updateCommandSurfaceState();
        updateHelpPanel();
        return;
    }

    updateCommandSurfaceState();
    updateHelpPanel();
}

void MapEditorTab::handleAddPointTriggered()
{
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Point mode: click Point again to add another draft point.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Point), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(160.0, 160.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Line mode: each action inserts one draft line card.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Line), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(180.0, 180.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Freehand mode: inserted a draft line. Complete Draft commits one undo step.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Line), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(185.0, 185.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleAddSmartTraceLineTriggered()
{
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Smart Trace mode: inserted a trace-ready draft line. Complete Draft commits one undo step.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Line), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(190.0, 190.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Area mode: each action inserts one draft area card.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Area), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(200.0, 200.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
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
        completeDraftButton_->setEnabled(mapReady && selectedDraftGeometryItem() != nullptr);
        fitBackgroundButton_->setEnabled(mapReady);
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
    if (mapScene_ == nullptr) {
        return;
    }

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
        summary = tr("TH2 map workspace scaffolding");
    }

    if (fitBackgroundRequested_) {
        summary += tr(" (fit includes background layers)");
    }

    if (!backgroundImageItems_.isEmpty()) {
        summary += tr(" | Backgrounds: %1").arg(backgroundImageItems_.size());
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
        undoStack_->push(new MapPointGeometryMoveCommand(textEditor_,
                                                         lineNumber,
                                                         oldPoint,
                                                         newPoint,
                                                         statusCallback));
        return;
    }

    MapPointGeometryMoveCommand directCommand(textEditor_, lineNumber, oldPoint, newPoint, statusCallback);
    directCommand.redo();
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
        undoStack_->push(new MapLineAreaVertexMoveCommand(textEditor_,
                                                          lineNumber,
                                                          rewriteKind,
                                                          vertexIndex,
                                                          oldPoint,
                                                          newPoint,
                                                          secondaryMoves,
                                                          statusCallback));
        return;
    }

    MapLineAreaVertexMoveCommand directCommand(textEditor_,
                                               lineNumber,
                                               rewriteKind,
                                               vertexIndex,
                                               oldPoint,
                                               newPoint,
                                               secondaryMoves,
                                               statusCallback);
    directCommand.redo();
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

    const QRectF fittedBounds = fittedPreviewBoundsForSource(sourceBounds, previewBounds);
    const qreal clampedX = qBound(fittedBounds.left(), previewPoint.x(), fittedBounds.right());
    const qreal clampedY = qBound(fittedBounds.top(), previewPoint.y(), fittedBounds.bottom());
    const qreal normalizedX = qBound(0.0, (clampedX - fittedBounds.left()) / qMax(1.0, fittedBounds.width()), 1.0);
    const qreal normalizedY = qBound(0.0, (fittedBounds.bottom() - clampedY) / qMax(1.0, fittedBounds.height()), 1.0);

    return QPointF(sourceBounds.left() + (normalizedX * sourceBounds.width()),
                   sourceBounds.top() + (normalizedY * sourceBounds.height()));
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
}

}
