#include "BlockEditorOptionTableDelegate.h"

#include <QCompleter>
#include <QLineEdit>

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
    lineEdit->setCompleter(completer);
    return editor;
}
}
