#pragma once

#include <QString>
#include <QWidget>

namespace TherionStudio
{
class MapEditorStylePreviewWidget final : public QWidget
{
public:
    explicit MapEditorStylePreviewWidget(QWidget *parent = nullptr);

    void setStyleSelection(const QString &commandKind,
                           const QString &rawType,
                           const QString &subtype);
    void clearStyleSelection();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString commandKind_;
    QString rawType_;
    QString subtype_;
};
}
