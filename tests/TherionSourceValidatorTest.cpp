#include "../src/core/TherionSourceValidator.h"
#include "../src/core/TherionCommandSyntax.h"

#include <QCoreApplication>

#include <cstdio>
#include <cstdlib>

using TherionStudio::TherionSourceValidationResult;
using TherionStudio::TherionSourceValidationCatalog;
using TherionStudio::TherionSourceValidator;
using TherionStudio::TherionSourceDiagnosticSeverity;
using TherionStudio::TherionSourceDiagnostic;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionSourceValidatorTest failed: %s\n", message);
        std::exit(1);
    }
}

QString diagnosticSourceRange(const TherionSourceDiagnostic &diagnostic)
{
    if (diagnostic.columnNumber <= 0 || diagnostic.columnLength <= 0) {
        return QString();
    }
    return diagnostic.currentText.mid(diagnostic.columnNumber - 1, diagnostic.columnLength);
}

void reportsAndFixesMalformedClipTokens()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off \"-clip off\" \"-clip off\"\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 1,
            "Malformed quoted -clip off tokens should produce one line-level diagnostic.");
    require(result.diagnostics.first().lineNumber == 1,
            "Malformed quoted -clip off diagnostic should point at the source line.");
    require(result.diagnostics.first().severity == TherionSourceDiagnosticSeverity::Error,
            "Malformed quoted -clip off tokens should be reported as an error.");
    require(result.diagnostics.first().suggestedText == QStringLiteral("line rock-border -close on -clip off"),
            "Malformed quoted -clip off tokens should be removed from the suggested line.");
    require(diagnosticSourceRange(result.diagnostics.first()).startsWith(QStringLiteral("\"-clip off\"")),
            "Malformed quoted -clip off diagnostics should point at the first malformed token.");

    const QString fixed = TherionSourceValidator::applyFixes(contents, {result.diagnostics.first().fix});
    require(fixed == QStringLiteral("line rock-border -close on -clip off\nendline\n"),
            "Applying malformed quoted -clip off fix should preserve line endings and remove duplicate tokens.");
}

void reportsAndFixesDuplicateOptionRows()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off -clip off\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 1,
            "Duplicate option/value pairs should produce one line-level diagnostic.");
    require(result.diagnostics.first().suggestedText == QStringLiteral("line rock-border -close on -clip off"),
            "Duplicate option/value pairs should be removed from the suggested line.");
    require(diagnosticSourceRange(result.diagnostics.first()) == QStringLiteral("-clip off"),
            "Duplicate option diagnostics should point at the duplicate option/value token range.");
}

void reportsAndFixesOptionLikeSubtype()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off -subtype \"-clip off\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 1,
            "Option-like subtype values should produce one line-level diagnostic.");
    require(result.diagnostics.first().suggestedText == QStringLiteral("line rock-border -close on -clip off"),
            "Option-like subtype values should be removed from the suggested line.");
}

void keepsQuotedTextValuesStartingWithDash()
{
    const QString contents = QStringLiteral("line label -text \"-clip off\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.isEmpty(),
            "Quoted text values that start with '-' should not be reported as malformed option tokens.");
}

TherionSourceValidationCatalog basicCatalog()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {
        QStringLiteral("encoding"),
        QStringLiteral("survey"),
        QStringLiteral("input"),
        QStringLiteral("map"),
        QStringLiteral("export"),
        QStringLiteral("centerline"),
        QStringLiteral("revise"),
        QStringLiteral("scrap"),
        QStringLiteral("line"),
        QStringLiteral("point"),
        QStringLiteral("area"),
    };
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("survey"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("input"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("map"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("export"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("revise"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("scrap"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("line"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("point"), 3);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("area"), 1);
    catalog.commandOptionNames.insert(QStringLiteral("survey"), {QStringLiteral("-title")});
    catalog.commandOptionNames.insert(QStringLiteral("map"), {QStringLiteral("-title")});
    catalog.commandOptionNames.insert(QStringLiteral("export"), {QStringLiteral("-output"), QStringLiteral("-o"), QStringLiteral("-layout")});
    catalog.commandOptionNames.insert(QStringLiteral("revise"), {QStringLiteral("-stations")});
    catalog.commandOptionNames.insert(QStringLiteral("point"), {QStringLiteral("-text")});
    catalog.commandOptionNames.insert(QStringLiteral("line"), {QStringLiteral("-close"), QStringLiteral("-clip")});
    catalog.commandOptionNames.insert(QStringLiteral("scrap"), {QStringLiteral("-projection"), QStringLiteral("-scale")});
    catalog.commandTypeValues.insert(QStringLiteral("line"), {QStringLiteral("wall"), QStringLiteral("border")});
    catalog.commandTypeValues.insert(QStringLiteral("area"), {QStringLiteral("water"), QStringLiteral("sand")});
    catalog.commandArgumentAllowedValuesByKey.insert(TherionStudio::commandArgumentValueKey(QStringLiteral("line"), 0),
                                                     {QStringLiteral("wall"), QStringLiteral("border")});
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-close")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionAllowedValuesByKey.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-close")),
                                                   {QStringLiteral("on"), QStringLiteral("off"), QStringLiteral("auto")});
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-projection")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-scale")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("export"), QStringLiteral("-o")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("export"), QStringLiteral("-layout")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionFixedArityByKey.insert(TherionStudio::commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-text")),
                                                0);
    return catalog;
}

bool containsDiagnostic(const TherionSourceValidationResult &result, const QString &code)
{
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

const TherionSourceDiagnostic *diagnosticForCode(const TherionSourceValidationResult &result,
                                                 const QString &code)
{
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return &diagnostic;
        }
    }
    return nullptr;
}

TherionSourceDiagnosticSeverity severityForDiagnostic(const TherionSourceValidationResult &result, const QString &code)
{
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return diagnostic.severity;
        }
    }
    std::fprintf(stderr, "TherionSourceValidatorTest failed: missing diagnostic %s\n", qPrintable(code));
    std::exit(1);
}

void reportsUnknownCommandWithoutSafeFix()
{
    const QString contents = QStringLiteral("scrap test\n"
                                            "bogus value\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Unknown command should produce a review-only diagnostic.");
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == QStringLiteral("unknown-command")) {
            require(!diagnostic.hasFix, "Unknown command should not be marked as a safe fix.");
        }
    }
}

void reportsUnknownOptionAndMissingOptionValue()
{
    const QString contents = QStringLiteral("line wall -bogus x -clip\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-option")),
            "Unknown option should produce a diagnostic.");
    const TherionSourceDiagnostic *unknownOption =
        diagnosticForCode(result, QStringLiteral("unknown-option"));
    require(unknownOption != nullptr && diagnosticSourceRange(*unknownOption) == QStringLiteral("-bogus"),
            "Unknown option diagnostics should point at the unknown option token.");
    require(severityForDiagnostic(result, QStringLiteral("unknown-option")) == TherionSourceDiagnosticSeverity::Warning,
            "Unknown option should remain a warning because the catalog may be incomplete.");
    require(containsDiagnostic(result, QStringLiteral("missing-option-value")),
            "Known option without a required value should produce a diagnostic.");
    const TherionSourceDiagnostic *missingOptionValue =
        diagnosticForCode(result, QStringLiteral("missing-option-value"));
    require(missingOptionValue != nullptr && diagnosticSourceRange(*missingOptionValue) == QStringLiteral("-clip"),
            "Missing option value diagnostics should point at the option token.");
    require(severityForDiagnostic(result, QStringLiteral("missing-option-value")) == TherionSourceDiagnosticSeverity::Error,
            "Known option without a required value should be reported as an error.");
}

void reportsMissingRequiredArgument()
{
    const QString contents = QStringLiteral("scrap -projection plan\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("missing-argument")),
            "Missing required positional argument should produce a diagnostic.");
    require(severityForDiagnostic(result, QStringLiteral("missing-argument")) == TherionSourceDiagnosticSeverity::Error,
            "Missing required positional argument should be reported as an error.");
}

void acceptsBracketedOptionValueAsSingleLogicalValue()
{
    const QString contents = QStringLiteral("scrap vchod_02.s -projection plan -scale [0 0 1600 0 0.0 0.0 40.64 0.0 m]\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Bracketed option values should count as one logical value for fixed arity validation.");
}

void acceptsLineContinuationAfterOptionValue()
{
    const QString contents =
        QStringLiteral("survey 1318 -title \"1318 Vetrna propast\" \\")
        + QLatin1Char('\n')
        + QStringLiteral("  -person-rename \"Lenka Souckova\" \"Lenka Blazkova\"\n"
                         "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Line continuation markers should not count as option values.");
}

void doesNotTreatZeroFixedArityAsValidationError()
{
    const QString contents = QStringLiteral("point 389.5 355.5 label -text \"Sifon I\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Fixed arity zero from catalog metadata is not reliable enough to reject existing option values.");
}

void acceptsOptionAliasesExtractedFromCatalogSignature()
{
    const QStringList aliases = TherionStudio::extractOptionKeysFromSignatureAliases(QStringLiteral("output/o <file>"));
    require(aliases.contains(QStringLiteral("-output")),
            "Option alias extraction should include the full option name.");
    require(aliases.contains(QStringLiteral("-o")),
            "Option alias extraction should include the short option alias.");

    const TherionSourceValidationResult result =
        TherionSourceValidator::validate(QStringLiteral("export map -o map.pdf -layout moj\n"), basicCatalog());
    require(!containsDiagnostic(result, QStringLiteral("unknown-option")),
            "Known option aliases such as export -o should not be reported as unknown options.");
}

void reportsUnknownArgumentValue()
{
    const QString contents = QStringLiteral("line mystery\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-argument-value")),
            "Unknown first argument value should produce a diagnostic when allowed values are known.");
    const TherionSourceDiagnostic *unknownArgumentValue =
        diagnosticForCode(result, QStringLiteral("unknown-argument-value"));
    require(unknownArgumentValue != nullptr && diagnosticSourceRange(*unknownArgumentValue) == QStringLiteral("mystery"),
            "Unknown argument value diagnostics should point at the argument token.");
}

void reportsUnknownOptionValue()
{
    const QString contents = QStringLiteral("line wall -close maybe\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-option-value")),
            "Unknown option values should produce a diagnostic when allowed values are known.");
    const TherionSourceDiagnostic *unknownOptionValue =
        diagnosticForCode(result, QStringLiteral("unknown-option-value"));
    require(unknownOptionValue != nullptr && diagnosticSourceRange(*unknownOptionValue) == QStringLiteral("maybe"),
            "Unknown option value diagnostics should point at the invalid option value token.");
    require(severityForDiagnostic(result, QStringLiteral("unknown-option-value")) == TherionSourceDiagnosticSeverity::Warning,
            "Unknown option values should remain warnings because catalog values may be incomplete.");
}

void reportsBlockPairProblems()
{
    const TherionSourceValidationResult unclosed =
        TherionSourceValidator::validate(QStringLiteral("scrap test\n"), basicCatalog());
    require(containsDiagnostic(unclosed, QStringLiteral("unclosed-block")),
            "Unclosed block should produce a diagnostic.");
    require(severityForDiagnostic(unclosed, QStringLiteral("unclosed-block")) == TherionSourceDiagnosticSeverity::Error,
            "Unclosed block should be reported as an error.");

    const TherionSourceValidationResult unmatched =
        TherionSourceValidator::validate(QStringLiteral("endscrap\n"), basicCatalog());
    require(containsDiagnostic(unmatched, QStringLiteral("unmatched-block-close")),
            "Unmatched block close should produce a diagnostic.");
    require(severityForDiagnostic(unmatched, QStringLiteral("unmatched-block-close")) == TherionSourceDiagnosticSeverity::Error,
            "Unmatched block close should be reported as an error.");
    require(!containsDiagnostic(unmatched, QStringLiteral("unknown-command")),
            "Known block close directives should not be reported as unknown commands.");
}

void keepsInputAsStandaloneCommandAndMapReferencesAsMapContent()
{
    const QString contents = QStringLiteral("encoding utf-8\n"
                                            "survey 1303_1974 -title \"1303 old map\"\n"
                                            "\n"
                                            "input stara_dvanactka_1974/00index.th\n"
                                            "\n"
                                            "map 1303_1974.m -title \"1303 old map\"\n"
                                            "  stara_dvanactka_1974.m@stara_dvanactka_1974\n"
                                            "endmap\n"
                                            "\n"
                                            "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unclosed-block")),
            "Standalone input commands should not corrupt block matching.");
    require(!containsDiagnostic(result, QStringLiteral("unmatched-block-close")),
            "Standalone input commands should not make later block closes look unmatched.");
    require(!containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Map object reference rows should not be reported as unknown commands.");
}

void keepsCenterlineDataRowsOutOfCommandCatalogValidation()
{
    const QString contents = QStringLiteral("centerline\n"
                                            "  data normal from to compass clino tape\n"
                                            "  1 2 0 0 1\n"
                                            "endcenterline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Centerline survey data rows should not be reported as unknown commands.");
    require(!containsDiagnostic(result, QStringLiteral("unclosed-block")),
            "Centerline blocks should still be matched.");
    require(!containsDiagnostic(result, QStringLiteral("unmatched-block-close")),
            "Centerline block closes should still be matched.");
}

void keepsOneLineReviseFromOpeningBlock()
{
    const QString contents = QStringLiteral("survey cave\n"
                                            "revise hipds@hip.hip_dom -stations 19@dur.dur_dom\n"
                                            "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unclosed-block")),
            "One-line revise commands should not open a block.");
    require(!containsDiagnostic(result, QStringLiteral("unmatched-block-close")),
            "One-line revise commands should not make the enclosing block close look unmatched.");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    reportsAndFixesMalformedClipTokens();
    reportsAndFixesDuplicateOptionRows();
    reportsAndFixesOptionLikeSubtype();
    keepsQuotedTextValuesStartingWithDash();
    reportsUnknownCommandWithoutSafeFix();
    reportsUnknownOptionAndMissingOptionValue();
    reportsMissingRequiredArgument();
    acceptsBracketedOptionValueAsSingleLogicalValue();
    acceptsLineContinuationAfterOptionValue();
    doesNotTreatZeroFixedArityAsValidationError();
    acceptsOptionAliasesExtractedFromCatalogSignature();
    reportsUnknownArgumentValue();
    reportsUnknownOptionValue();
    reportsBlockPairProblems();
    keepsInputAsStandaloneCommandAndMapReferencesAsMapContent();
    keepsCenterlineDataRowsOutOfCommandCatalogValidation();
    keepsOneLineReviseFromOpeningBlock();
    return 0;
}
