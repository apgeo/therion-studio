#include "BlockEditorTokenTagEditor.h"

#include "../../../core/TherionDocumentParser.h"

#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QRegularExpression>
#include <QTimer>
#include <QToolButton>
#include <QAbstractItemView>

#include <algorithm>

namespace TherionStudio
{
BlockEditorTokenTagEditor::BlockEditorTokenTagEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    input_ = new QLineEdit(this);
    input_->setObjectName(QStringLiteral("tokenTagEditorInput"));
    input_->installEventFilter(this);
    layout->addWidget(input_, 1);

    connect(input_, &QLineEdit::returnPressed, this, [this]() {
        commitInputTokens();
    });
    connect(input_, &QLineEdit::selectionChanged, this, [this]() {
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    });
    connect(input_, &QLineEdit::cursorPositionChanged, this, [this](int, int) {
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    });
}

void BlockEditorTokenTagEditor::setPlaceholderText(const QString &text)
{
    placeholderText_ = text;
    refreshInputPlaceholder();
}

void BlockEditorTokenTagEditor::setSuggestions(const QStringList &values)
{
    suggestions_.clear();
    for (const QString &value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            suggestions_.append(normalized);
        }
    }
    suggestions_.removeDuplicates();
    std::sort(suggestions_.begin(), suggestions_.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    rebuildCompleter();
}

void BlockEditorTokenTagEditor::setTokens(const QStringList &tokens)
{
    QStringList normalized;
    for (const QString &token : tokens) {
        const QString trimmed = token.trimmed();
        if (!trimmed.isEmpty() && !containsTokenCaseInsensitive(normalized, trimmed)) {
            normalized.append(trimmed);
        }
    }
    tokens_ = normalized;
    rebuildChips();
    refreshInputPlaceholder();
}

QStringList BlockEditorTokenTagEditor::tokens() const
{
    return tokens_;
}

bool BlockEditorTokenTagEditor::hasEditorFocus() const
{
    if (input_ != nullptr && input_->hasFocus()) {
        return true;
    }
    for (QToolButton *chip : chipButtons_) {
        if (chip != nullptr && chip->hasFocus()) {
            return true;
        }
    }
    return false;
}

bool BlockEditorTokenTagEditor::containsTokenCaseInsensitive(const QStringList &tokens, const QString &token)
{
    for (const QString &existing : tokens) {
        if (QString::compare(existing.trimmed(), token.trimmed(), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void BlockEditorTokenTagEditor::rebuildCompleter()
{
    if (input_ == nullptr) {
        return;
    }
    if (suggestions_.isEmpty()) {
        input_->setCompleter(nullptr);
        return;
    }
    auto *completer = new QCompleter(suggestions_, input_);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    input_->setCompleter(completer);
    connect(completer,
            QOverload<const QString &>::of(&QCompleter::activated),
            input_,
            [this](const QString &choice) {
                const QString normalized = choice.trimmed();
                if (normalized.isEmpty()) {
                    return;
                }
                appendToken(normalized);
                // Clear after Qt finishes completer insertion to avoid stale token text.
                QTimer::singleShot(0, this, [this]() {
                    if (input_ != nullptr) {
                        input_->clear();
                    }
                });
            });
}

void BlockEditorTokenTagEditor::refreshInputPlaceholder()
{
    if (input_ == nullptr) {
        return;
    }
    input_->setPlaceholderText(tokens_.isEmpty() ? placeholderText_ : QString());
}

bool BlockEditorTokenTagEditor::appendToken(const QString &token)
{
    const QString normalized = token.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    if (containsTokenCaseInsensitive(tokens_, normalized)) {
        return false;
    }
    tokens_.append(normalized);
    rebuildChips();
    refreshInputPlaceholder();
    if (onTokensChanged) {
        onTokensChanged();
    }
    return true;
}

bool BlockEditorTokenTagEditor::commitInputTokens()
{
    if (input_ == nullptr) {
        return false;
    }
    const QString rawInput = input_->text().trimmed();
    if (rawInput.isEmpty()) {
        return false;
    }
    QStringList parsedTokens = TherionDocumentParser::tokenizeLine(rawInput);
    if (parsedTokens.isEmpty()) {
        parsedTokens = rawInput.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
    }
    if (parsedTokens.isEmpty()) {
        return false;
    }
    for (const QString &token : parsedTokens) {
        const QString normalized = token.trimmed();
        if (!normalized.isEmpty()) {
            appendToken(normalized);
        }
    }
    input_->clear();
    return true;
}

void BlockEditorTokenTagEditor::rebuildChips()
{
    auto *layout = qobject_cast<QHBoxLayout *>(this->layout());
    if (layout == nullptr || input_ == nullptr) {
        return;
    }
    for (QToolButton *chip : chipButtons_) {
        if (chip == nullptr) {
            continue;
        }
        layout->removeWidget(chip);
        chip->deleteLater();
    }
    chipButtons_.clear();

    for (int index = 0; index < tokens_.size(); ++index) {
        const QString token = tokens_.at(index);
        auto *chip = new QToolButton(this);
        chip->setText(QStringLiteral("%1  ✕").arg(token));
        chip->setProperty("token_index", index);
        chip->setAutoRaise(true);
        chip->setFocusPolicy(Qt::StrongFocus);
        connect(chip, &QToolButton::clicked, this, [this, chip]() {
            const int tokenIndex = chip->property("token_index").toInt();
            if (tokenIndex < 0 || tokenIndex >= tokens_.size()) {
                return;
            }
            tokens_.removeAt(tokenIndex);
            rebuildChips();
            refreshInputPlaceholder();
            if (onTokensChanged) {
                onTokensChanged();
            }
        });
        connect(chip, &QToolButton::pressed, this, [this]() {
            if (onFocusContextChanged) {
                onFocusContextChanged();
            }
        });
        layout->insertWidget(layout->count() - 1, chip);
        chipButtons_.append(chip);
    }
}

bool BlockEditorTokenTagEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != input_ || event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent == nullptr) {
            return QWidget::eventFilter(watched, event);
        }
        if (keyEvent->key() == Qt::Key_Space
            || keyEvent->key() == Qt::Key_Comma
            || keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter
            || keyEvent->key() == Qt::Key_Tab) {
            const bool isCompletionAcceptKey = keyEvent->key() == Qt::Key_Return
                || keyEvent->key() == Qt::Key_Enter
                || keyEvent->key() == Qt::Key_Tab;
            if (isCompletionAcceptKey) {
                if (QCompleter *completer = input_->completer();
                    completer != nullptr && completer->popup() != nullptr && completer->popup()->isVisible()) {
                    return QWidget::eventFilter(watched, event);
                }
            }
            if (commitInputTokens()) {
                return true;
            }
        } else if (keyEvent->key() == Qt::Key_Backspace && input_->text().isEmpty() && !tokens_.isEmpty()) {
            tokens_.removeLast();
            rebuildChips();
            refreshInputPlaceholder();
            if (onTokensChanged) {
                onTokensChanged();
            }
            return true;
        }
    } else if (event->type() == QEvent::FocusIn) {
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    }
    return QWidget::eventFilter(watched, event);
}
}
