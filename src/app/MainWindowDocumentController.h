#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace TherionStudio
{
class MainWindowDocumentController final
{
public:
    struct CloseTabRequest
    {
        int tabIndex = -1;
    };

    struct CloseTabEntry
    {
        int tabIndex = -1;
        QString documentPath;
    };

    struct CloseAllRequest
    {
        QList<CloseTabEntry> tabEntries;
        QStringList detachedDocumentPaths;
    };

    struct SaveAllRequest
    {
        QStringList documentPaths;
    };

    struct SaveActiveRequest
    {
        bool hasActiveDocument = false;
        QString activeDocumentPath;
    };

    struct Actions
    {
        std::function<bool(int)> confirmCloseTab;
        std::function<bool(const QString &)> confirmCloseDocumentPath;
        std::function<bool(int)> closeTab;
        std::function<bool(const QString &)> closeDetachedDocumentPath;
        std::function<bool()> hasNoAttachedTabs;
        std::function<void()> ensureWelcomeTab;
        std::function<void()> refreshDocumentStatusWidgets;
        std::function<void()> persistOpenDocuments;
        std::function<void(const QString &, int)> showStatusBarMessage;
        std::function<void(const QString &, const QString &)> showWarningDialog;
        std::function<void(const QString &)> showComingSoon;
        std::function<bool(const QString &, QString *)> saveDocument;
        std::function<bool(const QString &)> documentIsDirty;
        std::function<bool(const QString &)> isAttachedDocument;
        std::function<void(const QString &)> updateTabTitle;
        std::function<QString(const QString &)> documentDisplayName;
    };

    static bool closeTab(const CloseTabRequest &request, const Actions &actions);
    static bool closeAll(const CloseAllRequest &request, const Actions &actions);
    static bool saveActive(const SaveActiveRequest &request, const Actions &actions);
    static bool saveAll(const SaveAllRequest &request, const Actions &actions);
};
}
