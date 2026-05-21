#include "BlockEditorToolboxList.h"

#include "BlockEditorDragMime.h"

#include <QAbstractItemView>
#include <QDrag>
#include <QListWidgetItem>
#include <QMimeData>

namespace TherionStudio
{
BlockEditorToolboxList::BlockEditorToolboxList(QWidget *parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void BlockEditorToolboxList::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    QListWidgetItem *item = currentItem();
    if (item == nullptr) {
        return;
    }

    const QString kind = item->data(Qt::UserRole).toString().trimmed();
    if (kind.isEmpty()) {
        return;
    }

    auto *mimeData = new QMimeData;
    mimeData->setData(QString::fromUtf8(kBlockEditorDragMimeType), kind.toUtf8());

    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
}
}
