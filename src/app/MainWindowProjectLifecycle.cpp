#include "MainWindow.h"

#include <QDir>
#include <QMainWindow>
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
    if (documentWidget == nullptr || documentPathForWidget(documentWidget).isEmpty() || !documentIsDirtyForWidget(documentWidget)) {
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
        if (!documentSaveForWidget(documentWidget, &errorMessage)) {
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
        if (tabWidget != nullptr && !documentPathForWidget(tabWidget).isEmpty()) {
            documents.append(tabWidget);
        }
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab != nullptr && !documentPathForWidget(detachedTab).isEmpty()) {
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
    if (closeProjectAction_ != nullptr) {
        closeProjectAction_->setEnabled(hasOpenProject);
    }
    if (workspaceOpenProjectButton_ != nullptr) {
        workspaceOpenProjectButton_->setEnabled(!hasOpenProject);
    }
    if (workspaceCloseProjectButton_ != nullptr) {
        workspaceCloseProjectButton_->setEnabled(hasOpenProject);
    }
}
