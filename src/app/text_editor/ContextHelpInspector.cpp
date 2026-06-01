#include "ContextHelpInspector.h"

#include "InspectorPanel.h"
#include "TextEditorSurfaceStyler.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QSizePolicy>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

namespace TherionStudio
{
ContextHelpInspector::ContextHelpInspector(QWidget *parent, const QString &title)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QVBoxLayout *sectionLayout = nullptr;
    auto *section = InspectorPanel::createSection(this,
                                                  title.trimmed().isEmpty() ? tr("Context Help") : title,
                                                  &sectionLayout,
                                                  &titleLabel_);
    section->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    browser_ = new QTextBrowser(section);
    browser_->hide();
    browser_->setFrameShape(QFrame::NoFrame);
    browser_->setOpenLinks(false);
    browser_->setOpenExternalLinks(false);
    browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser_->setContentsMargins(0, 0, 0, 0);
    browser_->setFixedSize(0, 0);
    syncTextBrowserSurfaceToParent(browser_);
    if (QTextDocument *document = browser_->document(); document != nullptr) {
        document->setDocumentMargin(0);
        connect(document, &QTextDocument::contentsChanged, this, [this]() {
            syncVisibleHelpFromBrowser();
        });
    }

    helpTextLabel_ = new QLabel(section);
    helpTextLabel_->setTextFormat(Qt::RichText);
    helpTextLabel_->setWordWrap(true);
    helpTextLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    helpTextLabel_->setOpenExternalLinks(false);
    helpTextLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    helpTextLabel_->setContentsMargins(0, 0, 0, 0);
    updateHelpTextPalette();

    sectionLayout->addWidget(helpTextLabel_);
    layout->addWidget(section, 0, Qt::AlignTop);
}

QTextBrowser *ContextHelpInspector::browser() const
{
    return browser_;
}

QLabel *ContextHelpInspector::titleLabel() const
{
    return titleLabel_;
}

void ContextHelpInspector::setTitle(const QString &title)
{
    if (titleLabel_ == nullptr) {
        return;
    }
    const QString normalized = title.trimmed();
    titleLabel_->setText(normalized.isEmpty() ? tr("Context Help") : normalized);
}

void ContextHelpInspector::setHtml(const QString &html)
{
    if (browser_ != nullptr) {
        browser_->setHtml(html);
    }
}

void ContextHelpInspector::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)) {
        updateHelpTextPalette();
    }
}

void ContextHelpInspector::syncVisibleHelpFromBrowser()
{
    if (browser_ == nullptr || helpTextLabel_ == nullptr) {
        return;
    }
    helpTextLabel_->setText(browser_->toHtml());
}

void ContextHelpInspector::updateHelpTextPalette()
{
    if (helpTextLabel_ == nullptr) {
        return;
    }
    helpTextLabel_->setPalette(palette());
    helpTextLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        " background-color: palette(base);"
        " color: palette(text);"
        " border: none;"
        "}"));
    if (QTextDocument *document = browser_ != nullptr ? browser_->document() : nullptr; document != nullptr) {
        document->setDocumentMargin(0);
    }
    syncVisibleHelpFromBrowser();
}

}
