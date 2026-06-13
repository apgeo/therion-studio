#include "MainWindow.h"

#include <QDir>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>

#include "MainWindowDocumentHelpers.h"
#include "text_editor/map_editor/MapEditorTab.h"

bool MainWindow::confirmCloseTab(int index)
{
    QWidget *tabWidget = editorTabs_->widget(index);
    return confirmCloseDocumentWidget(tabWidget);
}

bool MainWindow::confirmCloseDocumentWidget(QWidget *documentWidget)
{
    if (documentWidget == nullptr || documentWidget->property("therionStudioWelcomeTab").toBool() || !documentIsDirtyForWidget(documentWidget)) {
        return true;
    }

    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Warning);
    prompt.setWindowTitle(tr("Unsaved Changes"));
    prompt.setText(tr("The document %1 has unsaved changes.").arg(documentDisplayNameForWidget(documentWidget)));
    prompt.setInformativeText(tr("Do you want to save your changes before closing this document?"));
    QPushButton *saveButton = prompt.addButton(tr("Save"), QMessageBox::AcceptRole);
    QPushButton *discardButton = prompt.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    prompt.addButton(QMessageBox::Cancel);
    prompt.setDefaultButton(saveButton);
    prompt.exec();

    if (prompt.clickedButton() == saveButton) {
        QString errorMessage;
        if (!saveDocumentWidget(documentWidget, &errorMessage)) {
            QMessageBox::warning(this, tr("Save"), errorMessage);
            return false;
        }

        if (editorTabs_->indexOf(documentWidget) >= 0) {
            updateTabTitle(documentWidget);
        }
        return true;
    }

    if (prompt.clickedButton() == discardButton) {
        return true;
    }

    return false;
}

bool MainWindow::confirmCloseDirtyDocuments()
{
    QList<QWidget *> documents;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget != nullptr && !tabWidget->property("therionStudioWelcomeTab").toBool()) {
            documents.append(tabWidget);
        }
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab != nullptr) {
            documents.append(detachedTab);
        }
    }

    for (QWidget *documentWidget : documents) {
        if (!confirmCloseDocumentWidget(documentWidget)) {
            return false;
        }
    }

    return true;
}

void MainWindow::clearDocumentTabs()
{
    clearingDocumentTabs_ = true;

    const auto detachedPaths = detachedMapWindowsByPath_.keys();
    for (const QString &path : detachedPaths) {
        if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
            if (window->isVisible()) {
                window->close();
            } else {
                window->deleteLater();
            }
        }
    }
    detachedMapWindowsByPath_.clear();
    detachedMapPathsByTab_.clear();

    while (editorTabs_->count() > 0) {
        QWidget *tabWidget = editorTabs_->widget(0);
        editorTabs_->removeTab(0);
        if (tabWidget != nullptr) {
            tabWidget->deleteLater();
        }
    }

    addWelcomeTab();
    clearingDocumentTabs_ = false;
    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
}

void MainWindow::updateProjectActionState()
{
    const bool hasOpenProject = !projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists();
    if (openProjectAction_ != nullptr) {
        openProjectAction_->setEnabled(!hasOpenProject);
    }
    if (newProjectMenu_ != nullptr) {
        newProjectMenu_->menuAction()->setEnabled(!hasOpenProject);
    }
    if (newFileMenu_ != nullptr) {
        newFileMenu_->menuAction()->setEnabled(hasOpenProject);
    }
    if (projectFromTemplateAction_ != nullptr) {
        projectFromTemplateAction_->setEnabled(!hasOpenProject);
    }
    if (emptyProjectAction_ != nullptr) {
        emptyProjectAction_->setEnabled(!hasOpenProject);
    }
    if (newTherionSourceAction_ != nullptr) {
        newTherionSourceAction_->setEnabled(hasOpenProject);
    }
    if (newTherionMapAction_ != nullptr) {
        newTherionMapAction_->setEnabled(hasOpenProject);
    }
    if (newTherionConfigAction_ != nullptr) {
        newTherionConfigAction_->setEnabled(hasOpenProject);
    }
    if (closeProjectAction_ != nullptr) {
        closeProjectAction_->setEnabled(hasOpenProject);
    }
    refreshRecentProjectsUi();
    refreshRecentFilesUi();
}
