#include "InspectorPanel.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
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
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    selector_ = new QWidget(this);
    selector_->setObjectName(QStringLiteral("inspectorPanelSegmentedControl"));
    selectorLayout_ = new QHBoxLayout(selector_);
    selectorLayout_->setContentsMargins(0, 0, 0, 0);
    selectorLayout_->setSpacing(0);
    selectorButtons_ = new QButtonGroup(selector_);
    selectorButtons_->setExclusive(true);
    selector_->setStyleSheet(QStringLiteral(
        "QPushButton#inspectorSegmentButton {"
        " background-color: palette(window);"
        " border: 1px solid palette(mid);"
        " border-left-width: 0;"
        " border-radius: 0;"
        " padding: 4px 10px;"
        "}"
        "QPushButton#inspectorSegmentButton[firstSegment=\"true\"] {"
        " border-left-width: 1px;"
        " border-top-left-radius: 4px;"
        " border-bottom-left-radius: 4px;"
        "}"
        "QPushButton#inspectorSegmentButton[lastSegment=\"true\"] {"
        " border-top-right-radius: 4px;"
        " border-bottom-right-radius: 4px;"
        "}"
        "QPushButton#inspectorSegmentButton:checked {"
        " background-color: palette(base);"
        " font-weight: 600;"
        "}"));
    selector_->hide();
    layout->addWidget(selector_);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("inspectorPanelTabs"));
    tabs_->setStyleSheet(QStringLiteral(
        "QTabWidget#inspectorPanelTabs {"
        " border: none;"
        "}"
        "QTabWidget#inspectorPanelTabs::pane {"
        " border: none;"
        " margin: 0;"
        " padding: 0;"
        "}"));
    if (QTabBar *tabBar = tabs_->tabBar(); tabBar != nullptr) {
        tabBar->setDrawBase(true);
        tabBar->hide();
    }
    connect(tabs_, &QTabWidget::currentChanged, this, &InspectorPanel::syncSelectorFromTabs);
    connect(selectorButtons_, &QButtonGroup::idClicked, this, [this](int index) {
        if (tabs_ != nullptr && index >= 0 && index < tabs_->count()) {
            tabs_->setCurrentIndex(index);
        }
    });
    tabs_->installEventFilter(this);
    layout->addWidget(tabs_, 1);

    leftEdge_ = new QFrame(tabs_);
    leftEdge_->setObjectName(QStringLiteral("inspectorPanelLeftEdge"));
    leftEdge_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    leftEdge_->setFrameShape(QFrame::NoFrame);
    leftEdge_->setFixedWidth(1);
    leftEdge_->hide();
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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    tabs_->addTab(page, title);
    rebuildSelector();
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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    scrollArea->setWidget(page);
    tabs_->addTab(scrollArea, title);
    rebuildSelector();
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
    sectionLayout->setContentsMargins(6, 6, 6, 6);
    sectionLayout->setSpacing(4);

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
    if (leftEdge_ != nullptr) {
        leftEdge_->hide();
    }
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

void InspectorPanel::rebuildSelector()
{
    if (tabs_ == nullptr || selector_ == nullptr || selectorLayout_ == nullptr || selectorButtons_ == nullptr) {
        return;
    }
    if (QTabBar *tabBar = tabs_->tabBar(); tabBar != nullptr) {
        tabBar->hide();
    }

    const QList<QAbstractButton *> buttons = selectorButtons_->buttons();
    for (QAbstractButton *button : buttons) {
        selectorButtons_->removeButton(button);
        selectorLayout_->removeWidget(button);
        delete button;
    }

    const int count = tabs_->count();
    for (int index = 0; index < count; ++index) {
        auto *button = new QPushButton(tabs_->tabText(index), selector_);
        button->setObjectName(QStringLiteral("inspectorSegmentButton"));
        button->setCheckable(true);
        button->setMinimumHeight(28);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setProperty("firstSegment", index == 0);
        button->setProperty("lastSegment", index == count - 1);
        selectorButtons_->addButton(button, index);
        selectorLayout_->addWidget(button);
    }

    selector_->setVisible(count > 1);
    syncSelectorFromTabs();
    updateLeftEdgeGeometry();
}

void InspectorPanel::syncSelectorFromTabs()
{
    if (tabs_ == nullptr || selectorButtons_ == nullptr) {
        return;
    }

    if (QAbstractButton *button = selectorButtons_->button(tabs_->currentIndex()); button != nullptr) {
        button->setChecked(true);
    }
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
