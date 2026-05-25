#include "MainWindow.h"

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QPalette>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTime>
#include <QToolButton>

void MainWindow::initializeDocumentStatusWidgets()
{
    if (statusBar() == nullptr
        || statusMapZoomLabel_ != nullptr
        || statusMapModeLabel_ != nullptr
        || statusCompilerButton_ != nullptr
        || statusDocumentEncodingLabel_ != nullptr) {
        return;
    }

    statusDocumentEncodingLabel_ = new QLabel(statusBar());
    statusDocumentEncodingLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusDocumentEncodingLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusDocumentEncodingLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    statusMapZoomLabel_ = new QLabel(statusBar());
    statusMapZoomLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapZoomLabel_->setAlignment(Qt::AlignCenter);
    statusMapZoomLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapZoomLabel_->setVisible(false);

    statusMapModeLabel_ = new QLabel(statusBar());
    statusMapModeLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapModeLabel_->setAlignment(Qt::AlignCenter);
    statusMapModeLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapModeLabel_->setVisible(false);

    statusCompilerButton_ = new QToolButton(statusBar());
    statusCompilerButton_->setAutoRaise(true);
    statusCompilerButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    statusCompilerButton_->setCursor(Qt::PointingHandCursor);
    statusCompilerButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusCompilerButton_->setToolTip(tr("Open Compiler output"));
    connect(statusCompilerButton_, &QToolButton::clicked, this, [this]() {
        const bool compilerSidebarVisible = sidebarContainer_ != nullptr
            && sidebarContainer_->isVisible()
            && sidebarContentContainer_ != nullptr
            && sidebarContentContainer_->isVisible()
            && !sidebarCollapsed_
            && activeSidebarPane_ == SidebarPane::Console;
        if (compilerSidebarVisible) {
            setSidebarCollapsed(true);
            return;
        }

        showSidebarPane(SidebarPane::Console);
    });

    statusBar()->addPermanentWidget(statusMapZoomLabel_, 0);
    statusBar()->addPermanentWidget(statusMapModeLabel_, 0);
    statusBar()->addPermanentWidget(statusCompilerButton_, 0);
    statusBar()->addPermanentWidget(statusDocumentEncodingLabel_, 0);
    setCompilerStatusIdle();
    refreshDocumentStatusWidgets();
}

void MainWindow::refreshDocumentStatusWidgets()
{
    if (statusMapZoomLabel_ == nullptr
        || statusMapModeLabel_ == nullptr
        || statusDocumentEncodingLabel_ == nullptr) {
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    QString encodingText;
    QString mapModeText;
    QString mapZoomText;
    bool mapModeVisible = false;
    bool mapModeInsertActive = false;

    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        encodingText = textTab->statusEncodingText();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        encodingText = mapTab->statusEncodingText();
        if (!mapTab->isMapPaneDetached()) {
            mapModeText = mapTab->statusModeText();
            mapZoomText = tr("%1%").arg(mapTab->zoomPercent());
            mapModeVisible = true;
            mapModeInsertActive = mapTab->isInsertModeActive();
        }
    }

    if (encodingText.trimmed().isEmpty()) {
        statusDocumentEncodingLabel_->clear();
    } else {
        statusDocumentEncodingLabel_->setText(tr("Encoding: %1").arg(encodingText));
    }

    if (!mapModeVisible) {
        statusMapZoomLabel_->clear();
        statusMapZoomLabel_->setToolTip(QString());
        statusMapZoomLabel_->setVisible(false);
        statusMapModeLabel_->clear();
        statusMapModeLabel_->setToolTip(QString());
        statusMapModeLabel_->setStyleSheet(QString());
        statusMapModeLabel_->setVisible(false);
    } else {
        statusMapZoomLabel_->setText(mapZoomText);
        statusMapZoomLabel_->setToolTip(tr("Map zoom"));
        statusMapZoomLabel_->setVisible(true);

        const QString badgeText = mapModeInsertActive ? tr("Insert") : tr("Select");
        const QString background = mapModeInsertActive
            ? QStringLiteral("#d34a4a")
            : QStringLiteral("#2e9f5c");
        statusMapModeLabel_->setText(badgeText);
        statusMapModeLabel_->setToolTip(mapModeText);
        statusMapModeLabel_->setStyleSheet(QStringLiteral(
                                               "QLabel {"
                                               " color: white;"
                                               " font-weight: 700;"
                                               " background-color: %1;"
                                               " border-radius: 4px;"
                                               " padding: 1px 8px;"
                                               " min-height: 18px;"
                                               "}").arg(background));
        statusMapModeLabel_->setVisible(true);
    }
}

void MainWindow::setCompilerStatusIdle()
{
    updateCompilerStatusButton(tr("Compiler: Idle"),
                               tr("Therion compiler is idle. Click to open Compiler output."),
                               QStringLiteral("#6c757d"));
}

void MainWindow::setCompilerStatusRunning(const QString &configPath)
{
    const QString toolTip = configPath.isEmpty()
        ? tr("Therion is running. Click to open Compiler output.")
        : tr("Therion is running %1. Click to open Compiler output.").arg(QDir::toNativeSeparators(configPath));
    updateCompilerStatusButton(tr("Compiler: Running..."), toolTip, QStringLiteral("#2f80ed"));
}

void MainWindow::setCompilerStatusResult(bool success, const QString &details)
{
    const QString finishedAt = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    const QString statusText = success
        ? tr("Compiler: OK %1").arg(finishedAt)
        : tr("Compiler: Failed %1").arg(finishedAt);
    const QString toolTip = details.trimmed().isEmpty()
        ? tr("Last Therion run finished at %1. Click to open Compiler output.").arg(finishedAt)
        : tr("%1 Click to open Compiler output.").arg(details);
    updateCompilerStatusButton(statusText,
                               toolTip,
                               success ? QStringLiteral("#2e9f5c") : QStringLiteral("#c0392b"));
}

void MainWindow::updateCompilerStatusButton(const QString &text,
                                            const QString &toolTip,
                                            const QString &accentColor)
{
    if (statusCompilerButton_ == nullptr) {
        return;
    }

    statusCompilerButton_->setText(text);
    statusCompilerButton_->setToolTip(toolTip);
    statusCompilerButton_->setStyleSheet(QStringLiteral(
                                             "QToolButton {"
                                             " color: white;"
                                             " font-weight: 700;"
                                             " background-color: %1;"
                                             " border: none;"
                                             " border-radius: 4px;"
                                             " padding: 1px 8px;"
                                             " min-height: 18px;"
                                             "}").arg(accentColor));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcherGeometry();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        refreshWorkspaceIconTheme();
        rebuildStructureSidebar();
        rebuildMapObjectsTree();
        refreshMapBackgroundPanel();
        break;
    default:
        break;
    }
}
