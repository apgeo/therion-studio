#pragma once

#include <QStyledItemDelegate>
#include <QStringList>

#include <functional>

namespace TherionStudio
{
class CommandOptionTableDelegate final : public QStyledItemDelegate
{
public:
    using SuggestionsProvider = std::function<QStringList(const QModelIndex &)>;

    explicit CommandOptionTableDelegate(SuggestionsProvider provider, QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

private:
    SuggestionsProvider provider_;
};
}
