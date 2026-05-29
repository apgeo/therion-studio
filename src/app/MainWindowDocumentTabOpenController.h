#pragma once

#include <QString>

#include <functional>

namespace TherionStudio
{
class MainWindowDocumentTabOpenController final
{
public:
    struct ActivateExistingTabRequest
    {
        int tabIndex = -1;
    };

    struct AttachNewTabRequest
    {
        QString tabTitle;
        QString openedDocumentPath;
        int currentLineNumber = 0;
        QString consoleOpenedLine;
    };

    struct Actions
    {
        std::function<void()> removeWelcomeTabIfPresent;
        std::function<int(const QString &)> addTab;
        std::function<void(int)> setCurrentTabIndex;
        std::function<void(const QString &, int)> handleCurrentLineChanged;
        std::function<void()> updateCurrentTabTitle;
        std::function<void()> refreshDocumentStatusWidgets;
        std::function<void()> refreshWorkspaceModeSwitcher;
        std::function<void()> persistOpenDocuments;
        std::function<void(const QString &)> appendConsoleLine;
    };

    static bool activateExistingTab(const ActivateExistingTabRequest &request,
                                    const Actions &actions);

    static bool attachNewTab(const AttachNewTabRequest &request,
                             const Actions &actions);
};
}
