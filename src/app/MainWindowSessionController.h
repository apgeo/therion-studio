#pragma once

#include "../core/ISessionStore.h"

#include <QByteArray>
#include <QString>

#include <functional>

namespace TherionStudio
{
class MainWindowSessionController final
{
public:
    struct RestoreSessionActions
    {
        std::function<bool(const QByteArray &)> restoreGeometry;
        std::function<void(const QByteArray &)> restoreState;
        std::function<void()> resizeToDefault;
        std::function<void()> ensureUsableWindowSize;
        std::function<void(const QString &)> applyProjectRootToBrowser;
        std::function<void(const QString &)> appendConsoleLine;
        std::function<void()> loadStructureNameOverrides;
        std::function<void()> syncOpenDocumentsToProjectRoot;
        std::function<void()> rebuildStructureSidebar;
        std::function<void()> refreshTherionConfigDisplay;
        std::function<void()> updateProjectActionState;
        std::function<void()> restoreOpenDocuments;
    };

    static void restoreSession(const ISessionStore &sessionStore,
                               const RestoreSessionActions &actions);
};
}
