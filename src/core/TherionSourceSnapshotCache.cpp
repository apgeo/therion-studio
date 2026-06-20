#include "TherionSourceSnapshotCache.h"

#include <utility>

namespace TherionStudio
{
TherionSourceSnapshotCatalogKey TherionSourceSnapshotCatalogKey::none()
{
    return {};
}

TherionSourceSnapshotCatalogKey TherionSourceSnapshotCatalogKey::fromRevision(int revisionId)
{
    return {
        .enabled = true,
        .revisionId = revisionId,
    };
}

bool TherionSourceSnapshotCache::SourceKey::isCacheable() const
{
    return revisionId > 0;
}

bool TherionSourceSnapshotCache::SourceKey::operator==(const SourceKey &other) const
{
    return sourceType == other.sourceType
        && encodingName == other.encodingName
        && revisionId == other.revisionId;
}

bool TherionSourceSnapshotCache::LogicalKey::isCacheable() const
{
    return sourceKey.isCacheable();
}

bool TherionSourceSnapshotCache::LogicalKey::operator==(const LogicalKey &other) const
{
    return sourceKey == other.sourceKey
        && catalogKey.enabled == other.catalogKey.enabled
        && catalogKey.revisionId == other.catalogKey.revisionId;
}

const TherionSourceDocument &TherionSourceSnapshotCache::sourceDocument(
    const QString &contents,
    const TherionSourceDocumentMetadata &metadata)
{
    const SourceKey key = sourceKeyForMetadata(metadata);
    if (!key.isCacheable()) {
        transientSourceEntry_ = SourceEntry{
            .key = key,
            .document = TherionSourceDocument::fromText(contents, metadata),
        };
        return transientSourceEntry_->document;
    }

    if (!sourceEntry_.has_value() || !(sourceEntry_->key == key)) {
        sourceEntry_ = SourceEntry{
            .key = key,
            .document = TherionSourceDocument::fromText(contents, metadata),
        };
    }
    return sourceEntry_->document;
}

const TherionSourceLogicalDocument &TherionSourceSnapshotCache::logicalDocument(
    const QString &contents,
    const TherionSourceDocumentMetadata &metadata)
{
    const LogicalKey key = logicalKeyForMetadata(metadata, TherionSourceSnapshotCatalogKey::none());
    if (!key.isCacheable()) {
        transientLogicalEntry_ = LogicalEntry{
            .key = key,
            .document = TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(contents, metadata)),
        };
        return transientLogicalEntry_->document;
    }

    if (!logicalEntry_.has_value() || !(logicalEntry_->key == key)) {
        logicalEntry_ = LogicalEntry{
            .key = key,
            .document = TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(contents, metadata)),
        };
    }
    return logicalEntry_->document;
}

const TherionSourceLogicalDocument &TherionSourceSnapshotCache::logicalDocument(
    const QString &contents,
    const TherionSourceValidationCatalog &catalog,
    const TherionSourceDocumentMetadata &metadata,
    TherionSourceSnapshotCatalogKey catalogKey)
{
    const LogicalKey key = logicalKeyForMetadata(metadata, catalogKey);
    if (!key.isCacheable() || !catalogKey.enabled) {
        transientLogicalEntry_ = LogicalEntry{
            .key = key,
            .document = TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(contents, metadata),
                                                                        catalog),
        };
        return transientLogicalEntry_->document;
    }

    if (!logicalEntry_.has_value() || !(logicalEntry_->key == key)) {
        logicalEntry_ = LogicalEntry{
            .key = key,
            .document = TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(contents, metadata),
                                                                        catalog),
        };
    }
    return logicalEntry_->document;
}

void TherionSourceSnapshotCache::clear()
{
    sourceEntry_.reset();
    logicalEntry_.reset();
    transientSourceEntry_.reset();
    transientLogicalEntry_.reset();
}

TherionSourceSnapshotCache::SourceKey TherionSourceSnapshotCache::sourceKeyForMetadata(
    const TherionSourceDocumentMetadata &metadata)
{
    return {
        .sourceType = metadata.sourceType,
        .encodingName = metadata.encodingName,
        .revisionId = metadata.revisionId,
    };
}

TherionSourceSnapshotCache::LogicalKey TherionSourceSnapshotCache::logicalKeyForMetadata(
    const TherionSourceDocumentMetadata &metadata,
    TherionSourceSnapshotCatalogKey catalogKey)
{
    return {
        .sourceKey = sourceKeyForMetadata(metadata),
        .catalogKey = std::move(catalogKey),
    };
}
}
