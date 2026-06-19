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

TherionSourceDiagnostic diagnosticByCode(const TherionSourceValidationResult &result,
                                         const QString &code)
{
    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return diagnostic;
        }
    }
    return {};
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

    void warnsAndFixesEmptyScrapLineObjects()
    {
        const QString contents = QStringLiteral("scrap s\n"
                                                "line wall\n"
                                                "endline\n"
                                                "point 0 0 station -name a\n"
                                                "endscrap\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

        const TherionSourceDiagnostic diagnostic = diagnosticByCode(result, QStringLiteral("empty-scrap-object"));
        QCOMPARE(diagnostic.code, QStringLiteral("empty-scrap-object"));
        QCOMPARE(diagnostic.severity, TherionSourceDiagnosticSeverity::Warning);
        QCOMPARE(diagnostic.lineNumber, 2);
        QCOMPARE(diagnostic.currentText,
                 QStringLiteral("line wall\n"
                                "endline\n"));
        QVERIFY(diagnostic.hasFix);
        QCOMPARE(TherionSourceValidator::applyFixes(contents, {diagnostic.fix}),
                 QStringLiteral("scrap s\n"
                                "point 0 0 station -name a\n"
                                "endscrap\n"));
    }

    void warnsAndFixesEmptyScrapAreaObjectsWithCrLf()
    {
        const QString contents = QStringLiteral("scrap s\r\n"
                                                "area water\r\n"
                                                "\r\n"
                                                "endarea\r\n"
                                                "endscrap\r\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

        QCOMPARE(result.diagnostics.size(), 1);
        const TherionSourceDiagnostic diagnostic = result.diagnostics.constFirst();
        QCOMPARE(diagnostic.code, QStringLiteral("empty-scrap-object"));
        QCOMPARE(diagnostic.lineNumber, 2);
        QCOMPARE(diagnostic.currentText,
                 QStringLiteral("area water\r\n"
                                "\r\n"
                                "endarea\r\n"));
        QVERIFY(diagnostic.hasFix);
        QCOMPARE(TherionSourceValidator::applyFixes(contents, {diagnostic.fix}),
                 QStringLiteral("scrap s\r\n"
                                "endscrap\r\n"));
    }

    void keepsEmptyCenterlineBlocksAndCommentedScrapObjects()
    {
        const QString contents = QStringLiteral("centerline\n"
                                                "line wall\n"
                                                "endline\n"
                                                "endcenterline\n"
                                                "scrap s\n"
                                                "line wall\n"
                                                "  # keep note\n"
                                                "endline\n"
                                                "endscrap\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

        QVERIFY(!diagnosticCodes(result).contains(QStringLiteral("empty-scrap-object")));
    }
};

int runTherionSourceValidatorProjectionTest(int argc, char **argv)
{
    TherionSourceValidatorProjectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceValidatorProjectionTest.moc"
