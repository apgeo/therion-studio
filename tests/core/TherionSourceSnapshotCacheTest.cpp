#include "../../src/core/TherionSourceSnapshotCache.h"
#include "../../src/core/TherionCommandSyntax.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionSourceSnapshotCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void cachesSourceDocumentsByRevisionMetadata();
    void doesNotCacheUnrevisionedSourceDocuments();
    void doesNotCacheUnrevisionedLogicalDocuments();
    void invalidatesSourceDocumentsWhenEncodingChanges();
    void invalidatesLogicalDocumentsWhenSourceRevisionChanges();
    void invalidatesLogicalDocumentsAcrossUndoRedoRevisions();
    void invalidatesLogicalDocumentsWhenDocumentTypeChanges();
    void invalidatesCatalogLogicalDocumentsWhenCatalogRevisionChanges();
    void doesNotReuseCatalogLogicalDocumentsWithoutCatalogRevisionKey();
    void clearInvalidatesCachedSnapshots();
};

TherionSourceDocumentMetadata metadataForRevision(int revisionId)
{
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionSource;
    metadata.encodingName = QStringLiteral("UTF-8");
    metadata.revisionId = revisionId;
    return metadata;
}

TherionSourceValidationCatalog catalogWithSourceCommand()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {QStringLiteral("source")};
    catalog.commandDocumentTypes.insert(QStringLiteral("source"), {QStringLiteral("thconfig")});
    return catalog;
}

void TherionSourceSnapshotCacheTest::cachesSourceDocumentsByRevisionMetadata()
{
    TherionSourceSnapshotCache cache;
    const TherionSourceDocumentMetadata metadata = metadataForRevision(10);

    const TherionSourceDocument &first = cache.sourceDocument(QStringLiteral("survey a\n"), metadata);
    const TherionSourceDocument &second = cache.sourceDocument(QStringLiteral("survey ignored\n"), metadata);

    QCOMPARE(&second, &first);
    QCOMPARE(second.toText(), QStringLiteral("survey a\n"));

    TherionSourceDocumentMetadata nextMetadata = metadata;
    nextMetadata.revisionId = 11;
    const TherionSourceDocument &third = cache.sourceDocument(QStringLiteral("survey b\n"), nextMetadata);

    QCOMPARE(third.toText(), QStringLiteral("survey b\n"));
    QCOMPARE(third.revisionId(), 11);
}

void TherionSourceSnapshotCacheTest::doesNotCacheUnrevisionedSourceDocuments()
{
    TherionSourceSnapshotCache cache;
    const TherionSourceDocumentMetadata metadata;

    const TherionSourceDocument &first = cache.sourceDocument(QStringLiteral("survey a\n"), metadata);
    const QString firstText = first.toText();
    const TherionSourceDocument &second = cache.sourceDocument(QStringLiteral("survey b\n"), metadata);

    QCOMPARE(firstText, QStringLiteral("survey a\n"));
    QCOMPARE(second.toText(), QStringLiteral("survey b\n"));
}

void TherionSourceSnapshotCacheTest::doesNotCacheUnrevisionedLogicalDocuments()
{
    TherionSourceSnapshotCache cache;
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;
    const TherionSourceValidationCatalog catalog = catalogWithSourceCommand();

    const TherionSourceLogicalDocument &first = cache.logicalDocument(
        QStringLiteral("source first.th\n"),
        catalog,
        metadata,
        TherionSourceSnapshotCatalogKey::none());
    const QString firstReference = first.commands().constFirst().parsed.tokens.at(1);
    const bool firstKnown = first.commands().constFirst().metadata.catalogCommandKnown;

    const TherionSourceLogicalDocument &second = cache.logicalDocument(
        QStringLiteral("source second.th\n"),
        catalog,
        metadata,
        TherionSourceSnapshotCatalogKey::none());

    QCOMPARE(firstReference, QStringLiteral("first.th"));
    QVERIFY(firstKnown);
    QCOMPARE(second.commands().constFirst().parsed.tokens.at(1), QStringLiteral("second.th"));
    QVERIFY(second.commands().constFirst().metadata.catalogCommandKnown);
    QVERIFY(second.metadata().sourceType == TherionSourceDocumentType::TherionConfig);
}

void TherionSourceSnapshotCacheTest::invalidatesSourceDocumentsWhenEncodingChanges()
{
    TherionSourceSnapshotCache cache;
    TherionSourceDocumentMetadata utf8Metadata = metadataForRevision(1);
    utf8Metadata.encodingName = QStringLiteral("UTF-8");
    TherionSourceDocumentMetadata latin2Metadata = utf8Metadata;
    latin2Metadata.encodingName = QStringLiteral("ISO-8859-2");

    const TherionSourceDocument &utf8 =
        cache.sourceDocument(QStringLiteral("encoding utf-8\nsurvey a\n"), utf8Metadata);
    const QString utf8EncodingName = utf8.metadata().encodingName;
    const TherionSourceDocument &latin2 =
        cache.sourceDocument(QStringLiteral("encoding iso8859-2\nsurvey a\n"), latin2Metadata);

    QCOMPARE(utf8EncodingName, QStringLiteral("UTF-8"));
    QCOMPARE(latin2.metadata().encodingName, QStringLiteral("ISO-8859-2"));
    QCOMPARE(latin2.toText(), QStringLiteral("encoding iso8859-2\nsurvey a\n"));
}

void TherionSourceSnapshotCacheTest::invalidatesLogicalDocumentsWhenSourceRevisionChanges()
{
    TherionSourceSnapshotCache cache;
    const TherionSourceLogicalDocument &first =
        cache.logicalDocument(QStringLiteral("survey a\n"), metadataForRevision(1));
    const TherionSourceLogicalDocument &second =
        cache.logicalDocument(QStringLiteral("survey ignored\n"), metadataForRevision(1));

    QCOMPARE(&second, &first);
    QCOMPARE(second.commands().constFirst().parsed.tokens.at(1), QStringLiteral("a"));

    const TherionSourceLogicalDocument &third =
        cache.logicalDocument(QStringLiteral("survey b\n"), metadataForRevision(2));

    QCOMPARE(third.commands().constFirst().parsed.tokens.at(1), QStringLiteral("b"));
    QCOMPARE(third.metadata().revisionId, 2);
}

void TherionSourceSnapshotCacheTest::invalidatesLogicalDocumentsAcrossUndoRedoRevisions()
{
    TherionSourceSnapshotCache cache;

    const TherionSourceLogicalDocument &initial =
        cache.logicalDocument(QStringLiteral("survey initial\nendsurvey\n"), metadataForRevision(1));
    QCOMPARE(initial.commands().constFirst().parsed.tokens.at(1), QStringLiteral("initial"));

    const TherionSourceLogicalDocument &edited =
        cache.logicalDocument(QStringLiteral("survey edited\nendsurvey\n"), metadataForRevision(2));
    QCOMPARE(edited.commands().constFirst().parsed.tokens.at(1), QStringLiteral("edited"));

    const TherionSourceLogicalDocument &undone =
        cache.logicalDocument(QStringLiteral("survey initial\nendsurvey\n"), metadataForRevision(3));
    QCOMPARE(undone.commands().constFirst().parsed.tokens.at(1), QStringLiteral("initial"));

    const TherionSourceLogicalDocument &redone =
        cache.logicalDocument(QStringLiteral("survey edited\nendsurvey\n"), metadataForRevision(4));
    QCOMPARE(redone.commands().constFirst().parsed.tokens.at(1), QStringLiteral("edited"));
    QCOMPARE(redone.metadata().revisionId, 4);
}

void TherionSourceSnapshotCacheTest::invalidatesLogicalDocumentsWhenDocumentTypeChanges()
{
    TherionSourceSnapshotCache cache;
    TherionSourceDocumentMetadata sourceMetadata = metadataForRevision(1);
    TherionSourceDocumentMetadata mapMetadata = sourceMetadata;
    mapMetadata.sourceType = TherionSourceDocumentType::TherionMap;

    const TherionSourceLogicalDocument &source =
        cache.logicalDocument(QStringLiteral("survey a\n"), sourceMetadata);
    const TherionSourceDocumentType sourceType = source.metadata().sourceType;
    const TherionSourceLogicalDocument &map =
        cache.logicalDocument(QStringLiteral("scrap s1\n"), mapMetadata);

    QVERIFY(sourceType == TherionSourceDocumentType::TherionSource);
    QVERIFY(map.metadata().sourceType == TherionSourceDocumentType::TherionMap);
    QCOMPARE(map.commands().constFirst().normalizedDirective, QStringLiteral("scrap"));
}

void TherionSourceSnapshotCacheTest::invalidatesCatalogLogicalDocumentsWhenCatalogRevisionChanges()
{
    TherionSourceSnapshotCache cache;
    TherionSourceDocumentMetadata metadata = metadataForRevision(1);
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;
    const TherionSourceValidationCatalog firstCatalog = catalogWithSourceCommand();
    TherionSourceValidationCatalog secondCatalog = firstCatalog;
    secondCatalog.commandNames.clear();

    const TherionSourceLogicalDocument &first = cache.logicalDocument(
        QStringLiteral("source index.th\n"),
        firstCatalog,
        metadata,
        TherionSourceSnapshotCatalogKey::fromRevision(1));
    const TherionSourceLogicalDocument &second = cache.logicalDocument(
        QStringLiteral("source ignored.th\n"),
        firstCatalog,
        metadata,
        TherionSourceSnapshotCatalogKey::fromRevision(1));

    QCOMPARE(&second, &first);
    QCOMPARE(second.commands().constFirst().parsed.tokens.at(1), QStringLiteral("index.th"));
    QVERIFY(second.commands().constFirst().metadata.catalogCommandKnown);

    const TherionSourceLogicalDocument &third = cache.logicalDocument(
        QStringLiteral("source index.th\n"),
        secondCatalog,
        metadata,
        TherionSourceSnapshotCatalogKey::fromRevision(2));

    QVERIFY(!third.commands().constFirst().metadata.catalogCommandKnown);
    QCOMPARE(third.commands().constFirst().parsed.tokens.at(1), QStringLiteral("index.th"));
}

void TherionSourceSnapshotCacheTest::doesNotReuseCatalogLogicalDocumentsWithoutCatalogRevisionKey()
{
    TherionSourceSnapshotCache cache;
    TherionSourceDocumentMetadata metadata = metadataForRevision(1);
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;
    const TherionSourceValidationCatalog firstCatalog = catalogWithSourceCommand();
    TherionSourceValidationCatalog secondCatalog = firstCatalog;
    secondCatalog.commandNames.clear();

    const TherionSourceLogicalDocument &first = cache.logicalDocument(
        QStringLiteral("source index.th\n"),
        firstCatalog,
        metadata,
        TherionSourceSnapshotCatalogKey::none());
    const bool firstKnown = first.commands().constFirst().metadata.catalogCommandKnown;
    const TherionSourceLogicalDocument &second = cache.logicalDocument(
        QStringLiteral("source index.th\n"),
        secondCatalog,
        metadata,
        TherionSourceSnapshotCatalogKey::none());

    QVERIFY(firstKnown);
    QVERIFY(!second.commands().constFirst().metadata.catalogCommandKnown);
}

void TherionSourceSnapshotCacheTest::clearInvalidatesCachedSnapshots()
{
    TherionSourceSnapshotCache cache;
    const TherionSourceDocumentMetadata metadata = metadataForRevision(1);
    const TherionSourceDocument &first = cache.sourceDocument(QStringLiteral("survey a\n"), metadata);
    QCOMPARE(first.toText(), QStringLiteral("survey a\n"));

    cache.clear();

    const TherionSourceDocument &second = cache.sourceDocument(QStringLiteral("survey b\n"), metadata);
    QCOMPARE(second.toText(), QStringLiteral("survey b\n"));
}
}

int runTherionSourceSnapshotCacheTest(int argc, char **argv)
{
    TherionSourceSnapshotCacheTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceSnapshotCacheTest.moc"
