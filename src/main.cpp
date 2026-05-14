#include "app/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

namespace
{
QString preferredPlatformStyle()
{
#if defined(Q_OS_MACOS)
    return QStringLiteral("fusion");
#elif defined(Q_OS_WIN)
    return QStringLiteral("fusion");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("fusion");
#else
    return QString();
#endif
}

QString fallbackPlatformStyle()
{
#if defined(Q_OS_MACOS)
    return QStringLiteral("macintosh");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#else
    return QString();
#endif
}

void applyPlatformStyle(QApplication &application)
{
    const QStringList styleKeys = QStyleFactory::keys();
    const QStringList preferredKeys = {preferredPlatformStyle(), fallbackPlatformStyle()};

    for (const QString &preferredKey : preferredKeys) {
        if (preferredKey.isEmpty()) {
            continue;
        }

        for (const QString &styleKey : styleKeys) {
            if (styleKey.compare(preferredKey, Qt::CaseInsensitive) != 0) {
                continue;
            }

            if (QStyle *style = QStyleFactory::create(styleKey)) {
                application.setStyle(style);
            }
            return;
        }
    }
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Therion Studio"));
    application.setOrganizationName(QStringLiteral("Therion Studio"));
    application.setOrganizationDomain(QStringLiteral("therionstudio.example"));

    applyPlatformStyle(application);
    application.setStyleSheet(
        QStringLiteral(
            "QWidget#SidebarContainer {"
            "  border: 0px;"
            "}"
            "QFrame#SidebarActivityRail {"
            "  border: 0px;"
            "  border-right: 1px solid palette(mid);"
            "}"
            "QStatusBar {"
            "  border-top: 1px solid palette(mid);"
            "}"
            "QStatusBar::item {"
            "  border: 0px;"
            "}"));

    auto *window = new MainWindow;
    window->show();

    return application.exec();
}
