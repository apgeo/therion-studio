#include "../../src/core/TherionCommandSyntax.h"
#include "../../src/core/TherionSourceDocument.h"
#include "../../src/core/TherionSourceLogicalDocument.h"
#include "../../src/core/TherionSourceValidator.h"

#include <QtTest/QtTest>

namespace
{
using namespace TherionStudio;

QStringList diagnosticCodes(const TherionSourceValidationResult &result)
{
    QStringList codes;
    codes.reserve(result.diagnostics.size());
    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        codes.append(diagnostic.code);
    }
    codes.sort(Qt::CaseInsensitive);
    return codes;
}

TherionSourceValidationCatalog basicCatalog()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {
        QStringLiteral("area"),
        QStringLiteral("endarea"),
        QStringLiteral("endscrap"),
        QStringLiteral("line"),
        QStringLiteral("scrap"),
    };
    catalog.commandOptionNames.insert(QStringLiteral("line"), {QStringLiteral("-id")});
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-id")),
                                                 QStringLiteral("1"));
    return catalog;
}
}

class TherionSourceValidatorProjectionTest final : public QObject
{
    Q_OBJECT

private slots:
    void projectionValidationMatchesTextValidation()
    {
        const QString contents = QStringLiteral("scrap s\n"
                                                "line wall -id a\n"
                                                "endline\n"
                                                "line wall -id a\n"
                                                "endline\n"
                                                "endscrap\n");
        const TherionSourceValidationCatalog catalog = basicCatalog();

        const TherionSourceValidationResult textResult =
            TherionSourceValidator::validate(contents, catalog);
        const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
        const TherionSourceLogicalDocument logicalDocument =
            TherionSourceLogicalDocument::fromSourceDocument(sourceDocument, catalog);
        const TherionSourceValidationResult projectionResult =
            TherionSourceValidator::validate(sourceDocument, logicalDocument, catalog);

        QCOMPARE(diagnosticCodes(projectionResult), diagnosticCodes(textResult));
        QVERIFY(diagnosticCodes(projectionResult).contains(QStringLiteral("duplicate-object-id")));
    }

    void projectionValidationPreservesBlockDiagnostics()
    {
        const QString contents = QStringLiteral("scrap s\n"
                                                "line wall\n"
                                                "endline\n");
        const TherionSourceValidationCatalog catalog = basicCatalog();

        const TherionSourceValidationResult textResult =
            TherionSourceValidator::validate(contents, catalog);
        const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
        const TherionSourceLogicalDocument logicalDocument =
            TherionSourceLogicalDocument::fromSourceDocument(sourceDocument, catalog);
        const TherionSourceValidationResult projectionResult =
            TherionSourceValidator::validate(sourceDocument, logicalDocument, catalog);

        QCOMPARE(diagnosticCodes(projectionResult), diagnosticCodes(textResult));
        QVERIFY(diagnosticCodes(projectionResult).contains(QStringLiteral("unclosed-block")));
    }
};

int runTherionSourceValidatorProjectionTest(int argc, char **argv)
{
    TherionSourceValidatorProjectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceValidatorProjectionTest.moc"
