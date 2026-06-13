#include "TextEditorValidationCatalog.h"

#include "TextEditorCommandMetadata.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "raw_editor/RawEditorCommandMetadataLoader.h"

#include "../../core/TherionCommandSyntax.h"

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

    catalog.commandContexts = metadata.blockCommandContextsByKind;
    for (auto iterator = metadata.commandDocumentTypeTokens.cbegin();
         iterator != metadata.commandDocumentTypeTokens.cend();
         ++iterator) {
        QSet<QString> documentTypes;
        for (const QString &documentType : iterator.value()) {
            const QString normalizedDocumentType = documentType.trimmed().toLower();
            if (!normalizedDocumentType.isEmpty()) {
                documentTypes.insert(normalizedDocumentType);
            }
        }
        if (!documentTypes.isEmpty()) {
            catalog.commandDocumentTypes.insert(iterator.key().trimmed().toLower(), documentTypes);
        }
    }
    catalog.commandRequiredPositionalCount = metadata.commandRequiredPositionalCount;
    catalog.commandMaxPositionalCount = metadata.commandMaxPositionalCount;
    catalog.commandArgumentAllowedValuesByKey = metadata.commandArgumentValueTokens;
    catalog.commandTypeValues = metadata.commandTypeValueTokens;
    catalog.commandOptionAllowedValuesByKey = metadata.commandOptionValueTokens;
    for (auto commandIterator = metadata.commandSubtypeByTypeTokens.cbegin();
         commandIterator != metadata.commandSubtypeByTypeTokens.cend();
         ++commandIterator) {
        const QString commandName = commandIterator.key().trimmed().toLower();
        for (auto typeIterator = commandIterator.value().cbegin();
             typeIterator != commandIterator.value().cend();
             ++typeIterator) {
            catalog.commandSubtypeValuesByTypeKey.insert(commandSubtypeValueKey(commandName, typeIterator.key()),
                                                         typeIterator.value());
        }
    }
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
