#include "TextEditorValidationCatalog.h"

#include "TextEditorCommandMetadata.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "raw_editor/RawEditorCommandMetadataLoader.h"

#include <QJsonObject>

namespace TherionStudio
{
TherionSourceValidationCatalog validationCatalogFromCommandMetadata(const TextEditorCommandMetadata &metadata)
{
    TherionSourceValidationCatalog catalog;
    for (const QString &commandName : metadata.commandCompletionTokens) {
        catalog.commandNames.insert(commandName.trimmed().toLower());
    }

    for (auto iterator = metadata.commandOptionTokens.cbegin(); iterator != metadata.commandOptionTokens.cend(); ++iterator) {
        const QString commandName = iterator.key().trimmed().toLower();
        QSet<QString> optionNames;
        for (const QString &optionName : iterator.value()) {
            QString normalizedOption = optionName.trimmed().toLower();
            if (!normalizedOption.startsWith(QLatin1Char('-'))) {
                normalizedOption.prepend(QLatin1Char('-'));
            }
            optionNames.insert(normalizedOption);
        }
        catalog.commandOptionNames.insert(commandName, optionNames);
    }

    catalog.commandRequiredPositionalCount = metadata.commandRequiredPositionalCount;
    catalog.commandArgumentAllowedValuesByKey = metadata.commandArgumentValueTokens;
    catalog.commandTypeValues = metadata.commandTypeValueTokens;
    catalog.commandOptionAllowedValuesByKey = metadata.commandOptionValueTokens;
    catalog.commandOptionValueArityTokens = metadata.commandOptionValueArityTokens;
    catalog.commandOptionFixedArityByKey = metadata.commandOptionFixedArityByKey;
    return catalog;
}

TherionSourceValidationCatalog validationCatalogFromCommandCatalog(const QJsonObject &catalogObject)
{
    TextEditorCommandMetadata metadata;
    RawEditorCommandMetadataContext context;
    context.metadata = &metadata;
    context.normalizedDirectiveToken = [](const QString &directive) {
        return BlockEditorDirectiveRules::normalizeDirective(directive);
    };

    RawEditorCommandMetadataLoader(std::move(context)).applyCatalogCommandsMetadata(catalogObject);
    return validationCatalogFromCommandMetadata(metadata);
}
}
