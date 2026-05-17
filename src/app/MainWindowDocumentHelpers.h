#pragma once

#include <QString>

class QTabBar;
class QWidget;

QString documentPathForWidget(QWidget *widget);
QString documentDisplayNameForWidget(QWidget *widget);
void documentGoToLineForWidget(QWidget *widget, int lineNumber);
int documentCurrentLineNumberForWidget(QWidget *widget);
bool documentIsDirtyForWidget(QWidget *widget);
bool documentSaveForWidget(QWidget *widget, QString *errorMessage);
void documentSetProjectRootPathForWidget(QWidget *widget, const QString &projectRootPath);
void documentShowFindBarForWidget(QWidget *widget, bool replaceMode);
bool documentCanUndoForWidget(QWidget *widget);
bool documentCanRedoForWidget(QWidget *widget);
bool documentUndoForWidget(QWidget *widget);
bool documentRedoForWidget(QWidget *widget);
void updateSidebarModeTabIcons(QTabBar *tabBar, int activeIndex);
