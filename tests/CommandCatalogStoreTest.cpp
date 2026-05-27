#include "../src/core/CommandCatalogStore.h"

#include <QFile>
#include <QJsonObject>
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

int runInjectedObjectTest()
{
    QJsonObject root;
    root.insert(QStringLiteral("commands"), QJsonObject{{QStringLiteral("survey"), QJsonObject{}}});

    const CommandCatalogStore store(root);
    if (!expect(store.isCatalogAvailable(), "Injected non-empty catalog should be available.")) {
        return 1;
    }
    if (!expect(store.catalogObject().contains(QStringLiteral("commands")),
                "Injected catalog object should be returned unchanged.")) {
        return 1;
    }

    const CommandCatalogStore emptyStore{QJsonObject()};
    if (!expect(!emptyStore.isCatalogAvailable(), "Injected empty catalog should not be available.")) {
        return 1;
    }

    return 0;
}

int runJsonBytesTest()
{
    const CommandCatalogStore validStore = CommandCatalogStore::fromJsonBytes(
        QByteArrayLiteral("{\"commands\":{\"centerline\":{}}}"));
    if (!expect(validStore.isCatalogAvailable(), "Valid JSON object bytes should produce an available catalog.")) {
        return 1;
    }
    if (!expect(validStore.catalogObject().value(QStringLiteral("commands")).isObject(),
                "Valid JSON object bytes should preserve nested catalog objects.")) {
        return 1;
    }

    const CommandCatalogStore arrayStore = CommandCatalogStore::fromJsonBytes(QByteArrayLiteral("[]"));
    if (!expect(!arrayStore.isCatalogAvailable(), "JSON arrays should not be accepted as catalogs.")) {
        return 1;
    }

    const CommandCatalogStore invalidStore = CommandCatalogStore::fromJsonBytes(QByteArrayLiteral("{"));
    if (!expect(!invalidStore.isCatalogAvailable(), "Invalid JSON should produce an empty unavailable catalog.")) {
        return 1;
    }

    return 0;
}

int runFileTest()
{
    QTemporaryDir temporaryDir;
    if (!expect(temporaryDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString catalogPath = temporaryDir.filePath(QStringLiteral("catalog.json"));
    QFile catalogFile(catalogPath);
    if (!expect(catalogFile.open(QIODevice::WriteOnly), "Temporary catalog file creation failed.")) {
        return 1;
    }
    catalogFile.write(QByteArrayLiteral("{\"commands\":{\"map\":{}}}"));
    catalogFile.close();

    const CommandCatalogStore fileStore = CommandCatalogStore::fromFile(catalogPath);
    if (!expect(fileStore.isCatalogAvailable(), "Valid catalog file should produce an available catalog.")) {
        return 1;
    }
    if (!expect(fileStore.catalogObject().value(QStringLiteral("commands")).toObject().contains(QStringLiteral("map")),
                "Catalog loaded from file should preserve command entries.")) {
        return 1;
    }

    const CommandCatalogStore missingFileStore = CommandCatalogStore::fromFile(
        temporaryDir.filePath(QStringLiteral("missing.json")));
    if (!expect(!missingFileStore.isCatalogAvailable(), "Missing catalog file should produce an unavailable catalog.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int result = runInjectedObjectTest(); result != 0) {
        return result;
    }
    if (const int result = runJsonBytesTest(); result != 0) {
        return result;
    }
    return runFileTest();
}
