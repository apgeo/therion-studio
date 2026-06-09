#pragma once

#include "../../core/TherionSourceValidator.h"

class QJsonObject;

namespace TherionStudio
{
struct TextEditorCommandMetadata;

[[nodiscard]] TherionSourceValidationCatalog validationCatalogFromCommandMetadata(const TextEditorCommandMetadata &metadata);
[[nodiscard]] TherionSourceValidationCatalog validationCatalogFromCommandCatalog(const QJsonObject &catalogObject);
}
