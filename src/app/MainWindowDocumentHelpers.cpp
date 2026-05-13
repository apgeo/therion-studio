#include "MainWindowDocumentHelpers.h"

#include <QTabBar>
#include <QStyle>
#include <QWidget>

#include "TextEditorTab.h"
#include "MapEditorTab.h"

QString documentPathForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->filePath();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->filePath();
    }

    return QString();
}

QString documentDisplayNameForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->displayName();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->displayName();
    }

    return QString();
}

void documentGoToLineForWidget(QWidget *widget, int lineNumber)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->goToLine(lineNumber);
        return;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->goToLine(lineNumber);
    }
}

int documentCurrentLineNumberForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->currentLineNumber();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->currentLineNumber();
    }

    return 0;
}

bool documentIsDirtyForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->isDirty();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->isDirty();
    }

    return false;
}

bool documentSaveForWidget(QWidget *widget, QString *errorMessage)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->save(errorMessage);
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->save(errorMessage);
    }

    if (errorMessage != nullptr) {
        *errorMessage = QObject::tr("The current document cannot be saved.");
    }

    return false;
}

void documentSetProjectRootPathForWidget(QWidget *widget, const QString &projectRootPath)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->setProjectRootPath(projectRootPath);
        return;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->setProjectRootPath(projectRootPath);
    }
}

void documentShowFindBarForWidget(QWidget *widget, bool replaceMode)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->showFindBar(replaceMode);
        return;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->showFindBar(replaceMode);
    }
}

void updateSidebarModeTabIcons(QTabBar *tabBar, int)
{
    if (tabBar == nullptr) {
        return;
    }

    QStyle *style = tabBar->style();
    tabBar->setTabIcon(0, style->standardIcon(QStyle::SP_DirIcon));
    tabBar->setTabIcon(1, style->standardIcon(QStyle::SP_FileDialogDetailedView));
    tabBar->setTabIcon(2, style->standardIcon(QStyle::SP_FileIcon));
}