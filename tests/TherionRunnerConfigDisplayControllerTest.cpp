#include "../src/app/TherionRunnerConfigDisplayController.h"

#include <QDir>
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

int runEmptyResolvedPathShowsNoConfigTest()
{
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(QString());

    if (!expect(!result.hasResolvedConfigPath, "Empty config path should not produce a resolved config state.")) {
        return 1;
    }
    if (!expect(result.configName.isEmpty() && result.configPath.isEmpty() && result.configDirectory.isEmpty(),
                "Empty config path should not produce config display fields.")) {
        return 1;
    }

    return 0;
}

int runResolvedConfigDisplayFieldsTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("custom.thconfig"));
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(configPath);

    if (!expect(result.hasResolvedConfigPath, "Resolved config path should produce a resolved config state.")) {
        return 1;
    }
    if (!expect(result.configName == QStringLiteral("custom.thconfig"),
                "Resolved config name should come from the config path.")) {
        return 1;
    }
    if (!expect(result.configPath == configPath,
                "Resolved config path should be preserved for display.")) {
        return 1;
    }
    if (!expect(result.configDirectory == tempDir.path(),
                "Resolved config directory should come from the config path.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runEmptyResolvedPathShowsNoConfigTest() != 0) {
        return 1;
    }
    if (runResolvedConfigDisplayFieldsTest() != 0) {
        return 1;
    }

    return 0;
}
