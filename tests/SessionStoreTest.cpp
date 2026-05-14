#include "../src/core/SessionStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

int runTouchControlsSettingRoundTripTest()
{
    QTemporaryDir temporarySettingsDir;
    if (!expect(temporarySettingsDir.isValid(), "Temporary settings directory creation failed.")) {
        return 1;
    }

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporarySettingsDir.path());

    QCoreApplication::setOrganizationName(QStringLiteral("Therion Studio Test"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("therionstudio.test"));
    QCoreApplication::setApplicationName(QStringLiteral("Therion Studio SessionStore Test"));

    SessionStore::setTherionMapTouchFriendlyControlsEnabled(false);
    if (!expect(!SessionStore::therionMapTouchFriendlyControlsEnabled(),
                "Touch-friendly controls flag should read back false after false write.")) {
        return 1;
    }

    SessionStore::setTherionMapTouchFriendlyControlsEnabled(true);
    if (!expect(SessionStore::therionMapTouchFriendlyControlsEnabled(),
                "Touch-friendly controls flag should read back true after true write.")) {
        return 1;
    }

    SessionStore::setTherionMapTouchFriendlyControlsEnabled(false);
    if (!expect(!SessionStore::therionMapTouchFriendlyControlsEnabled(),
                "Touch-friendly controls flag should read back false after resetting.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    return runTouchControlsSettingRoundTripTest();
}
