#include "app/MainWindow.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QObject>
#include <QStyleHints>
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

QString applicationChromeStyleSheet()
{
    return QStringLiteral(
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
        "}");
}

void applyApplicationChromeStyle(QApplication &application)
{
    application.setStyleSheet(applicationChromeStyleSheet());
}

class ApplicationAppearanceWatcher final : public QObject
{
public:
    explicit ApplicationAppearanceWatcher(QApplication *application, QObject *parent = nullptr)
        : QObject(parent)
        , application_(application)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == application_ && event != nullptr && event->type() == QEvent::ApplicationPaletteChange) {
            applyApplicationChromeStyle(*application_);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QApplication *application_ = nullptr;
};
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Therion Studio"));
    application.setOrganizationName(QStringLiteral("Therion Studio"));
    application.setOrganizationDomain(QStringLiteral("therionstudio.example"));

    applyPlatformStyle(application);
    applyApplicationChromeStyle(application);
    ApplicationAppearanceWatcher appearanceWatcher(&application, &application);
    application.installEventFilter(&appearanceWatcher);
    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        QObject::connect(styleHints, &QStyleHints::colorSchemeChanged, &application, [&application](Qt::ColorScheme) {
            applyApplicationChromeStyle(application);
        });
    }

    auto *window = new MainWindow;
    window->show();

    return application.exec();
}
