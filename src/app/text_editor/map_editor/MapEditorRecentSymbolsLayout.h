#pragma once

#include <QLayout>
#include <QList>

namespace TherionStudio
{

class MapEditorRecentSymbolsLayout final : public QLayout
{
public:
    explicit MapEditorRecentSymbolsLayout(QWidget *parent = nullptr);
    ~MapEditorRecentSymbolsLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;

    QList<QLayoutItem *> items_;
};

}
