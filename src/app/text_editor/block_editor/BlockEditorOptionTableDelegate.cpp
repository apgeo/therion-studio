#include "BlockEditorOptionTableDelegate.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFontMetrics>
#include <QLineEdit>
#include <QScrollBar>
#include <QStringListModel>

namespace TherionStudio
{
BlockEditorOptionTableDelegate::BlockEditorOptionTableDelegate(SuggestionsProvider provider, QObject *parent)
    : QStyledItemDelegate(parent)
    , provider_(std::move(provider))
{
}

QWidget *BlockEditorOptionTableDelegate::createEditor(QWidget *parent,
                                                      const QStyleOptionViewItem &option,
                                                      const QModelIndex &index) const
{
    QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
    auto *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (lineEdit == nullptr || !provider_) {
        return editor;
    }

    const QStringList suggestions = provider_(index);
    if (suggestions.isEmpty()) {
        return editor;
    }

    auto *completer = new QCompleter(suggestions, lineEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);

    if (QAbstractItemView *popup = completer->popup(); popup != nullptr) {
        popup->setTextElideMode(Qt::ElideNone);
        popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        popup->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        const QFontMetrics fm(popup->font());
        int maxItemWidth = 0;
        for (const QString &item : suggestions) {
            maxItemWidth = qMax(maxItemWidth, fm.horizontalAdvance(item));
        }
        const int popupPaddingWidth = 36;
        const int popupWidth = qMax(lineEdit->width(), maxItemWidth + popupPaddingWidth);
        popup->setMinimumWidth(popupWidth);
    }

    lineEdit->setCompleter(completer);
    QObject::connect(lineEdit, &QLineEdit::textEdited, lineEdit, [lineEdit, completer](const QString &text) {
        if (completer == nullptr || lineEdit == nullptr) {
            return;
        }
        completer->setCompletionPrefix(text);
        completer->complete();
    });
    return editor;
}
}
