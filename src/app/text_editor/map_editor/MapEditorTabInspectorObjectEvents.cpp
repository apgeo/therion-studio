#include "MapEditorTab.h"

#include "MapEditorInspectorObjectController.h"

#include <QColor>
#include <QEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTreeView>
#include <QWidget>

#include <algorithm>

namespace TherionStudio
{
namespace
{
constexpr int kInspectorObjectCategoryRole = Qt::UserRole + 702;
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectDragColumn = 1;
constexpr int kInspectorObjectVisibilityColumn = 2;
constexpr int kInspectorObjectDeleteColumn = 3;
constexpr int kInspectorDragAutoScrollMargin = 24;

void hideDropIndicator(QWidget *indicator)
{
    if (indicator != nullptr) {
        indicator->hide();
    }
}

bool isScrapCategory(const QModelIndex &index)
{
    return index.data(kInspectorObjectCategoryRole).toString() == QStringLiteral("Scraps");
}

bool isMovableObjectIndex(const QModelIndex &index)
{
    return index.isValid()
        && index.data(kInspectorSourceLineRole).toInt() > 0
        && !isScrapCategory(index);
}

bool isDragAffordanceColumn(int column)
{
    return column == kInspectorObjectNameColumn || column == kInspectorObjectDragColumn;
}

void autoscrollDragViewport(QTreeView *tree, const QPoint &viewportPos)
{
    if (tree == nullptr || tree->viewport() == nullptr || tree->verticalScrollBar() == nullptr) {
        return;
    }

    QScrollBar *scrollBar = tree->verticalScrollBar();
    const int step = std::max(1, scrollBar->singleStep());
    if (viewportPos.y() < kInspectorDragAutoScrollMargin) {
        scrollBar->setValue(scrollBar->value() - step);
    } else if (viewportPos.y() > tree->viewport()->height() - kInspectorDragAutoScrollMargin) {
        scrollBar->setValue(scrollBar->value() + step);
    }
}

QString dropIndicatorStyleSheet(const QColor &color)
{
    return QStringLiteral("background-color: rgba(%1, %2, %3, 230);"
                          "border: 1px solid rgba(%1, %2, %3, 255);"
                          "border-radius: 1px;")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());
}
}

std::optional<bool> MapEditorTab::handleInspectorObjectViewportEvent(QEvent *event)
{
    if (event == nullptr || mapObjectsTree_ == nullptr) {
        return std::nullopt;
    }

    if (event->type() == QEvent::MouseMove && inspectorObjectPressedNameIndex_.isValid()) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!(mouseEvent->buttons() & Qt::LeftButton)) {
            return std::nullopt;
        }

        mapObjectsTree_->viewport()->setCursor(Qt::ClosedHandCursor);
        autoscrollDragViewport(mapObjectsTree_, mouseEvent->pos());
        const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
        const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
        if (objectIndex.isValid() && objectIndex != inspectorObjectPressedNameIndex_) {
            const QRect targetRect = mapObjectsTree_->visualRect(objectIndex);
            const bool targetIsScrap = isScrapCategory(objectIndex);
            const bool afterTarget = targetRect.isValid() && mouseEvent->pos().y() > targetRect.center().y();
            if (inspectorObjectDropIndicator_ == nullptr
                || inspectorObjectDropIndicator_->parentWidget() != mapObjectsTree_->viewport()) {
                inspectorObjectDropIndicator_ = new QWidget(mapObjectsTree_->viewport());
                inspectorObjectDropIndicator_->setAttribute(Qt::WA_TransparentForMouseEvents);
                inspectorObjectDropIndicator_->setAutoFillBackground(false);
                inspectorObjectDropIndicator_->setObjectName(QStringLiteral("mapObjectsTreeDropIndicator"));
            }
            inspectorObjectDropIndicator_->setStyleSheet(dropIndicatorStyleSheet(
                mapObjectsTree_->palette().color(QPalette::Highlight)));
            const int indicatorHeight = 3;
            const int indicatorX = std::max(0, targetRect.left());
            const int indicatorY = std::clamp(((afterTarget || targetIsScrap) ? targetRect.bottom() : targetRect.top())
                                                  - (indicatorHeight / 2),
                                              0,
                                              std::max(0, mapObjectsTree_->viewport()->height() - indicatorHeight));
            inspectorObjectDropIndicator_->setGeometry(indicatorX,
                                                       indicatorY,
                                                       std::max(0, mapObjectsTree_->viewport()->width() - indicatorX),
                                                       indicatorHeight);
            inspectorObjectDropIndicator_->raise();
            inspectorObjectDropIndicator_->show();
            toolbarStatusNote_ = targetIsScrap
                ? tr("Move object: release to place inside target scrap.")
                : (afterTarget
                       ? tr("Move object: release to place after target object.")
                       : tr("Move object: release to place before target object."));
            refreshToolbarSummary();
            event->accept();
            return true;
        }

        hideDropIndicator(inspectorObjectDropIndicator_);
        return std::nullopt;
    }

    if (event->type() == QEvent::MouseMove) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
        const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
        if (isDragAffordanceColumn(index.column()) && isMovableObjectIndex(objectIndex)) {
            mapObjectsTree_->viewport()->setCursor(Qt::OpenHandCursor);
        } else {
            mapObjectsTree_->viewport()->unsetCursor();
        }
        return std::nullopt;
    }

    if (event->type() == QEvent::Leave) {
        hideDropIndicator(inspectorObjectDropIndicator_);
        mapObjectsTree_->viewport()->unsetCursor();
        return std::nullopt;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return std::nullopt;
        }
        hideDropIndicator(inspectorObjectDropIndicator_);
        mapObjectsTree_->viewport()->unsetCursor();

        const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
        const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
        const QPersistentModelIndex sourceIndex = inspectorObjectPressedNameIndex_;
        inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
        inspectorObjectPressedWasSelected_ = false;
        if (sourceIndex.isValid() && objectIndex.isValid() && sourceIndex != objectIndex) {
            const QRect targetRect = mapObjectsTree_->visualRect(objectIndex);
            const bool afterTarget = targetRect.isValid() && mouseEvent->pos().y() > targetRect.center().y();
            if (MapEditorInspectorObjectController(inspectorObjectContext()).moveInspectorObject(sourceIndex, objectIndex, afterTarget)) {
                event->accept();
                return true;
            }
        }

        if (index.column() == kInspectorObjectNameColumn
            && objectIndex.isValid()
            && mapObjectsModel_ != nullptr
            && mapObjectsModel_->rowCount(objectIndex) > 0
            && mouseEvent->pos().x() < mapObjectsTree_->visualRect(objectIndex).left() + mapObjectsTree_->indentation()) {
            return std::nullopt;
        }
        if (index.isValid()
            && (index.column() == kInspectorObjectNameColumn
                || index.column() == kInspectorObjectDragColumn
                || index.column() == kInspectorObjectVisibilityColumn
                || index.column() == kInspectorObjectDeleteColumn)) {
            event->accept();
            return true;
        }
        return std::nullopt;
    }

    if (event->type() != QEvent::MouseButtonPress) {
        return std::nullopt;
    }

    inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
    inspectorObjectPressedWasSelected_ = false;

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() != Qt::LeftButton || mapObjectsTree_->selectionModel() == nullptr) {
        return std::nullopt;
    }

    const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
    const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
    if (index.column() == kInspectorObjectNameColumn
        && objectIndex.isValid()
        && mapObjectsModel_ != nullptr
        && mapObjectsModel_->rowCount(objectIndex) > 0
        && mouseEvent->pos().x() < mapObjectsTree_->visualRect(objectIndex).left() + mapObjectsTree_->indentation()) {
        return std::nullopt;
    }
    if (!index.isValid()
        || (index.column() != kInspectorObjectNameColumn
            && index.column() != kInspectorObjectDragColumn
            && index.column() != kInspectorObjectVisibilityColumn
            && index.column() != kInspectorObjectDeleteColumn)) {
        return std::nullopt;
    }

    if (index.column() == kInspectorObjectNameColumn || index.column() == kInspectorObjectDragColumn) {
        const QPersistentModelIndex pressedObjectIndex(objectIndex);
        const bool pressedWasSelected = mapObjectsTree_->selectionModel()->isSelected(objectIndex);
        const bool sourceIsScrap = isScrapCategory(objectIndex);
        if (!sourceIsScrap) {
            inspectorObjectPressedNameIndex_ = QPersistentModelIndex(objectIndex);
        }
        inspectorObjectPressedWasSelected_ = pressedWasSelected;
        if (index.column() == kInspectorObjectNameColumn) {
            handleInspectorObjectClicked(index);
        }
        inspectorObjectPressedNameIndex_ = sourceIsScrap ? QPersistentModelIndex() : pressedObjectIndex;
        inspectorObjectPressedWasSelected_ = pressedWasSelected;
    } else {
        handleInspectorObjectClicked(index);
    }
    event->accept();
    return true;
}
}
