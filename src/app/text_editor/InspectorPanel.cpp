#include "InspectorPanel.h"

#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace TherionStudio
{

InspectorPanel::InspectorPanel(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    setProperty("preserveNativeChildControls", true);
    setAttribute(Qt::WA_StyledBackground, true);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral(
        "TherionStudio--InspectorPanel {"
        " background-color: palette(base);"
        " border: none;"
        "}"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("inspectorPanelTabs"));
    if (QTabBar *tabBar = tabs_->tabBar(); tabBar != nullptr) {
        tabBar->setDrawBase(true);
    }
    tabs_->installEventFilter(this);
    layout->addWidget(tabs_, 1);

    leftEdge_ = new QFrame(tabs_);
    leftEdge_->setObjectName(QStringLiteral("inspectorPanelLeftEdge"));
    leftEdge_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    leftEdge_->setFrameShape(QFrame::NoFrame);
    leftEdge_->setFixedWidth(1);
    leftEdge_->setStyleSheet(QStringLiteral(
        "QFrame#inspectorPanelLeftEdge {"
        " background-color: palette(mid);"
        " border: none;"
        "}"));
    leftEdge_->raise();
}

QTabWidget *InspectorPanel::tabs() const
{
    return tabs_;
}

QFrame *InspectorPanel::leftEdge() const
{
    return leftEdge_;
}

QWidget *InspectorPanel::addPlainTab(const QString &title)
{
    auto *page = new QWidget(tabs_);
    page->setBackgroundRole(QPalette::Base);
    page->setAutoFillBackground(true);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);
    tabs_->addTab(page, title);
    return page;
}

QWidget *InspectorPanel::addScrollTab(const QString &title, const QString &objectName)
{
    auto *scrollArea = new QScrollArea(tabs_);
    configureScrollArea(scrollArea);

    auto *page = new QWidget(scrollArea);
    if (!objectName.isEmpty()) {
        page->setObjectName(objectName);
    }
    page->setBackgroundRole(QPalette::Base);
    page->setAutoFillBackground(true);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    scrollArea->setWidget(page);
    tabs_->addTab(scrollArea, title);
    return page;
}

QFrame *InspectorPanel::createSection(QWidget *parent,
                                      const QString &title,
                                      QVBoxLayout **contentLayout,
                                      QLabel **titleLabelOut)
{
    auto *section = new QFrame(parent);
    section->setFrameShape(QFrame::StyledPanel);
    auto *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(8, 8, 8, 8);
    sectionLayout->setSpacing(6);

    auto *titleLabel = new QLabel(title, section);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    sectionLayout->addWidget(titleLabel);

    if (titleLabelOut != nullptr) {
        *titleLabelOut = titleLabel;
    }
    if (contentLayout != nullptr) {
        *contentLayout = sectionLayout;
    }
    return section;
}

void InspectorPanel::updateLeftEdgeGeometry()
{
    if (tabs_ == nullptr || leftEdge_ == nullptr) {
        return;
    }

    QStyleOptionTabWidgetFrame option;
    option.initFrom(tabs_);
    option.lineWidth = tabs_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, tabs_);
    if (QTabBar *tabBar = tabs_->tabBar(); tabBar != nullptr) {
        option.shape = tabBar->shape();
        option.tabBarSize = tabBar->size();
        option.selectedTabRect = tabBar->tabRect(tabBar->currentIndex());
    }

    const QRect paneRect = tabs_->style()->subElementRect(QStyle::SE_TabWidgetTabPane, &option, tabs_);
    const int paneTop = qMax(0, paneRect.top());
    const int paneHeight = qMax(0, paneRect.height());
    leftEdge_->setGeometry(paneRect.left(), paneTop, 1, paneHeight);
    leftEdge_->raise();
    leftEdge_->setVisible(paneHeight > 0);
}

bool InspectorPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabs_) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
            updateLeftEdgeGeometry();
            break;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void InspectorPanel::configureScrollArea(QScrollArea *scrollArea)
{
    if (scrollArea == nullptr) {
        return;
    }

    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    if (QWidget *viewport = scrollArea->viewport(); viewport != nullptr) {
        viewport->setBackgroundRole(QPalette::Base);
        viewport->setAutoFillBackground(true);
    }
}

}
