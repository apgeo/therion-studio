#include "../src/platform/ApplicationIdentity.h"

#include <QCoreApplication>
#include <QSettings>

#include <iostream>

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const TherionStudio::Platform::ApplicationIdentity &identity =
        TherionStudio::Platform::applicationIdentity();
    QCoreApplication::setOrganizationName(identity.organizationName);
    QCoreApplication::setOrganizationDomain(identity.organizationDomain);
    QCoreApplication::setApplicationName(identity.applicationName);

    bool ok = true;
    ok &= expect(identity.organizationName == QStringLiteral("lblazek"),
                 "Expected release organization name to be lblazek.");
    ok &= expect(identity.organizationDomain == QStringLiteral("lblazek.com"),
                 "Expected release organization domain to be lblazek.com.");
    ok &= expect(identity.applicationName == QStringLiteral("therionstudio"),
                 "Expected stable settings application name to be therionstudio.");
    ok &= expect(identity.applicationDisplayName == QStringLiteral("Therion Studio"),
                 "Expected display name to remain Therion Studio.");
    ok &= expect(!identity.organizationDomain.endsWith(QStringLiteral(".example")),
                 "Release builds must not use an example organization domain.");
#if defined(Q_OS_MACOS)
    ok &= expect(QSettings().fileName().endsWith(
                     QStringLiteral("/Library/Preferences/com.lblazek.therionstudio.plist")),
                 "Expected macOS settings to use com.lblazek.therionstudio.plist.");
    const QSettings legacySettings(QSettings::NativeFormat,
                                   QSettings::UserScope,
                                   QStringLiteral("therionstudio.example"),
                                   QStringLiteral("Therion Studio"));
    ok &= expect(legacySettings.fileName().endsWith(
                     QStringLiteral("/Library/Preferences/com.therionstudio-example.Therion Studio.plist")),
                 "Expected legacy macOS settings to resolve to the previous .example plist.");
#endif

    return ok ? 0 : 1;
}
