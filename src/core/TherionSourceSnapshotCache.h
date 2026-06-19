#pragma once

#include "TherionSourceDocument.h"
#include "TherionSourceLogicalDocument.h"

#include <optional>

namespace TherionStudio
{
struct TherionSourceSnapshotCatalogKey
{
    bool enabled = false;
    int revisionId = 0;

    [[nodiscard]] static TherionSourceSnapshotCatalogKey none();
    [[nodiscard]] static TherionSourceSnapshotCatalogKey fromRevision(int revisionId);
};

class TherionSourceSnapshotCache final
{
public:
    [[nodiscard]] const TherionSourceDocument &sourceDocument(
        const QString &contents,
        const TherionSourceDocumentMetadata &metadata = {});

    [[nodiscard]] const TherionSourceLogicalDocument &logicalDocument(
        const QString &contents,
        const TherionSourceDocumentMetadata &metadata = {});

    [[nodiscard]] const TherionSourceLogicalDocument &logicalDocument(
        const QString &contents,
        const TherionSourceValidationCatalog &catalog,
        const TherionSourceDocumentMetadata &metadata,
        TherionSourceSnapshotCatalogKey catalogKey);

    void clear();

private:
    struct SourceKey
    {
        TherionSourceDocumentType sourceType = TherionSourceDocumentType::Unknown;
        QString encodingName;
        int revisionId = 0;

        [[nodiscard]] bool isCacheable() const;
        [[nodiscard]] bool operator==(const SourceKey &other) const;
    };

    struct LogicalKey
    {
        SourceKey sourceKey;
        TherionSourceSnapshotCatalogKey catalogKey;

        [[nodiscard]] bool isCacheable() const;
        [[nodiscard]] bool operator==(const LogicalKey &other) const;
    };

    struct SourceEntry
    {
        SourceKey key;
        TherionSourceDocument document;
    };

    struct LogicalEntry
    {
        LogicalKey key;
        TherionSourceLogicalDocument document;
    };

    [[nodiscard]] static SourceKey sourceKeyForMetadata(const TherionSourceDocumentMetadata &metadata);
    [[nodiscard]] static LogicalKey logicalKeyForMetadata(const TherionSourceDocumentMetadata &metadata,
                                                         TherionSourceSnapshotCatalogKey catalogKey);

    std::optional<SourceEntry> sourceEntry_;
    std::optional<LogicalEntry> logicalEntry_;
    std::optional<SourceEntry> transientSourceEntry_;
    std::optional<LogicalEntry> transientLogicalEntry_;
};
}
