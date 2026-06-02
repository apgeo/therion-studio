#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QEvent;
class QLayout;
class QLineEdit;
class QToolButton;

namespace TherionStudio
{
class BlockEditorTokenTagEditor final : public QWidget
{
public:
    explicit BlockEditorTokenTagEditor(QWidget *parent = nullptr);

    std::function<void()> onTokensChanged;
    std::function<void()> onFocusContextChanged;

    void setPlaceholderText(const QString &text);
    void setSuggestions(const QStringList &values);
    void setTokens(const QStringList &tokens);
    QStringList tokens() const;
    bool hasEditorFocus() const;

private:
    static bool containsTokenCaseInsensitive(const QStringList &tokens, const QString &token);

    void rebuildCompleter();
    void refreshInputPlaceholder();
    bool appendToken(const QString &token);
    bool commitInputTokens();
    void rebuildChips();
    bool eventFilter(QObject *watched, QEvent *event) override;

    QWidget *chipsContainer_ = nullptr;
    QLayout *chipsLayout_ = nullptr;
    QLineEdit *input_ = nullptr;
    QString placeholderText_;
    QStringList suggestions_;
    QStringList tokens_;
    QList<QToolButton *> chipButtons_;
};
}
