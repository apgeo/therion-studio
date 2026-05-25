#include "app/MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QObject>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>
#include <QStyleFactory>
#include <QTranslator>

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
        "}"
        "QStatusBar {"
        "  border-top: 1px solid palette(mid);"
        "}"
        "QStatusBar::item {"
        "  border: 0px;"
        "}");
}

bool effectiveDarkAppearance()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = styleHints->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            return true;
        }
        if (scheme == Qt::ColorScheme::Light) {
            return false;
        }
    }
#endif

    const QColor windowColor = QApplication::palette().color(QPalette::Window);
    return windowColor.isValid() && windowColor.lightnessF() < 0.5;
}

QPalette applicationPaletteForAppearance(bool darkAppearance)
{
    QPalette palette = QApplication::style() != nullptr
        ? QApplication::style()->standardPalette()
        : QPalette();

    if (!darkAppearance) {
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#0a84ff")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::Link, QColor(QStringLiteral("#0066cc")));
        return palette;
    }

    const QColor window(QStringLiteral("#20242c"));
    const QColor base(QStringLiteral("#171b22"));
    const QColor alternateBase(QStringLiteral("#232934"));
    const QColor text(QStringLiteral("#e8edf4"));
    const QColor mutedText(QStringLiteral("#a8b1c0"));
    const QColor button(QStringLiteral("#2a303b"));
    const QColor disabledText(QStringLiteral("#8f9bad"));
    const QColor light(QStringLiteral("#3d4654"));
    const QColor midlight(QStringLiteral("#647185"));
    const QColor mid(QStringLiteral("#46505f"));
    const QColor dark(QStringLiteral("#11161d"));
    const QColor shadow(QStringLiteral("#080b10"));
    const QColor highlight(QStringLiteral("#2f80ed"));
    const QColor highlightedText(QStringLiteral("#ffffff"));

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternateBase);
    palette.setColor(QPalette::ToolTipBase, alternateBase);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ff6b6b")));
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#75b7ff")));
    palette.setColor(QPalette::Light, light);
    palette.setColor(QPalette::Midlight, midlight);
    palette.setColor(QPalette::Mid, mid);
    palette.setColor(QPalette::Dark, dark);
    palette.setColor(QPalette::Shadow, shadow);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, highlightedText);
    palette.setColor(QPalette::PlaceholderText, mutedText);

    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(QStringLiteral("#3a4250")));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);
    return palette;
}

void applyApplicationAppearance(QApplication &application)
{
    application.setPalette(applicationPaletteForAppearance(effectiveDarkAppearance()));
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
            application_->setStyleSheet(applicationChromeStyleSheet());
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
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/app/app-icon.png")));

    QTranslator applicationTranslator;
    if (applicationTranslator.load(QLocale(),
                                   QStringLiteral("therion_studio"),
                                   QStringLiteral("_"),
                                   QStringLiteral(":/i18n"))) {
        application.installTranslator(&applicationTranslator);
    }

    applyPlatformStyle(application);
    applyApplicationAppearance(application);
    ApplicationAppearanceWatcher appearanceWatcher(&application, &application);
    application.installEventFilter(&appearanceWatcher);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        QObject::connect(styleHints, &QStyleHints::colorSchemeChanged, &application, [&application](Qt::ColorScheme) {
            applyApplicationAppearance(application);
        });
    }
#endif

    auto *window = new MainWindow;
    window->show();

    return application.exec();
}
