#include "MapEditorRecentSymbolsLayout.h"

#include <QLayoutItem>
#include <QMargins>
#include <QRect>
#include <QSize>

#include <algorithm>

namespace TherionStudio
{

MapEditorRecentSymbolsLayout::MapEditorRecentSymbolsLayout(QWidget *parent)
    : QLayout(parent)
{
    setContentsMargins(0, 0, 0, 0);
    setSpacing(4);
}

MapEditorRecentSymbolsLayout::~MapEditorRecentSymbolsLayout()
{
    while (QLayoutItem *item = takeAt(0)) {
        delete item;
    }
}

void MapEditorRecentSymbolsLayout::addItem(QLayoutItem *item)
{
    items_.append(item);
}

int MapEditorRecentSymbolsLayout::count() const
{
    return items_.size();
}

Qt::Orientations MapEditorRecentSymbolsLayout::expandingDirections() const
{
    return {};
}

bool MapEditorRecentSymbolsLayout::hasHeightForWidth() const
{
    return true;
}

int MapEditorRecentSymbolsLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

QLayoutItem *MapEditorRecentSymbolsLayout::itemAt(int index) const
{
    return index >= 0 && index < items_.size() ? items_.at(index) : nullptr;
}

QSize MapEditorRecentSymbolsLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : items_) {
        if (item == nullptr || item->isEmpty()) {
            continue;
        }
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

void MapEditorRecentSymbolsLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize MapEditorRecentSymbolsLayout::sizeHint() const
{
    return minimumSize();
}

QLayoutItem *MapEditorRecentSymbolsLayout::takeAt(int index)
{
    if (index < 0 || index >= items_.size()) {
        return nullptr;
    }
    return items_.takeAt(index);
}

int MapEditorRecentSymbolsLayout::doLayout(const QRect &rect, bool testOnly) const
{
    const QMargins margins = contentsMargins();
    const QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;
    const int itemSpacing = spacing();
    const int maxItemWidth = std::max(0, effectiveRect.width());

    for (QLayoutItem *item : items_) {
        if (item == nullptr || item->isEmpty()) {
            continue;
        }

        QSize itemSize = item->sizeHint();
        itemSize.setWidth(std::min(itemSize.width(), maxItemWidth));
        const int nextX = x + itemSize.width() + itemSpacing;
        if (nextX - itemSpacing > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y += lineHeight + itemSpacing;
            lineHeight = 0;
        }

        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), itemSize));
        }
        x += itemSize.width() + itemSpacing;
        lineHeight = std::max(lineHeight, itemSize.height());
    }
    return y + lineHeight - rect.y() + margins.bottom();
}

}
