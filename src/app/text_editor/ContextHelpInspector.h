#pragma once

#include <QWidget>

#include <QString>

class QLabel;
class QEvent;
class QTextBrowser;

namespace TherionStudio
{

class ContextHelpInspector final : public QWidget
{
    Q_OBJECT

public:
    explicit ContextHelpInspector(QWidget *parent = nullptr, const QString &title = QString());

    QTextBrowser *browser() const;
    QLabel *titleLabel() const;

    void setTitle(const QString &title);
    void setHtml(const QString &html);

protected:
    void changeEvent(QEvent *event) override;

private:
    void syncVisibleHelpFromBrowser();
    void updateHelpTextPalette();

    QLabel *titleLabel_ = nullptr;
    QLabel *helpTextLabel_ = nullptr;
    QTextBrowser *browser_ = nullptr;
};

}
