#include "DocumentFile.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QByteArrayView>
#include <QRegularExpression>
#include <QStringList>
#include <optional>

namespace TherionStudio
{
namespace
{
QString encodingDisplayName(const QString &encodingName)
{
    if (encodingName.trimmed().isEmpty()) {
        return QStringLiteral("UTF-8");
    }

    return encodingName.trimmed().toUpper();
}

QString canonicalEncodingName(QStringDecoder &decoder, const QString &fallbackName)
{
    const char *resolved = decoder.name();
    if (resolved == nullptr || *resolved == '\0') {
        return fallbackName.trimmed();
    }
    return QString::fromLatin1(resolved);
}

QByteArray encodingNameBytes(const QString &encodingName)
{
    return encodingName.trimmed().toLatin1();
}

bool decodeBytesWithName(QByteArrayView data,
                         const QString &encodingName,
                 QString *contents,
                 QString *resolvedEncodingName,
                 QString *encodingLabel)
{
    if (encodingName.trimmed().isEmpty()) {
        return false;
    }

    const QByteArray nameBytes = encodingNameBytes(encodingName);
    QStringDecoder decoder(nameBytes.constData(), QStringConverter::Flag::ConvertInvalidToNull);
    if (!decoder.isValid()) {
        return false;
    }

    QString decoded = decoder.decode(data);
    if (decoder.hasError()) {
        return false;
    }

    if (!decoded.isEmpty() && decoded.front() == QChar(0xFEFF)) {
        decoded.remove(0, 1);
    }

    if (contents != nullptr) {
        *contents = decoded;
    }
    const QString canonicalName = canonicalEncodingName(decoder, encodingName);
    if (resolvedEncodingName != nullptr) {
        *resolvedEncodingName = canonicalName;
    }
    if (encodingLabel != nullptr) {
        *encodingLabel = encodingDisplayName(canonicalName);
    }

    return true;
}

bool decodeBytesWithEnum(QByteArrayView data,
                         QStringConverter::Encoding encoding,
                         QString *contents,
                         QString *resolvedEncodingName,
                         QString *encodingLabel)
{
    const char *name = QStringConverter::nameForEncoding(encoding);
    if (name == nullptr || *name == '\0') {
        return false;
    }
    return decodeBytesWithName(data,
                               QString::fromLatin1(name),
                               contents,
                               resolvedEncodingName,
                               encodingLabel);
}

bool isCentralEuropeanLetter(QChar character)
{
    switch (character.unicode()) {
    case 0x0104: // A with ogonek
    case 0x0105:
    case 0x0106: // C with acute
    case 0x0107:
    case 0x010C: // C with caron
    case 0x010D:
    case 0x010E: // D with caron
    case 0x010F:
    case 0x0118: // E with ogonek
    case 0x0119:
    case 0x011A: // E with caron
    case 0x011B:
    case 0x0139: // L with acute
    case 0x013A:
    case 0x013D: // L with caron
    case 0x013E:
    case 0x0141: // L with stroke
    case 0x0142:
    case 0x0143: // N with acute
    case 0x0144:
    case 0x0147: // N with caron
    case 0x0148:
    case 0x0150: // O with double acute
    case 0x0151:
    case 0x0154: // R with acute
    case 0x0155:
    case 0x0158: // R with caron
    case 0x0159:
    case 0x015A: // S with acute
    case 0x015B:
    case 0x0160: // S with caron
    case 0x0161:
    case 0x0164: // T with caron
    case 0x0165:
    case 0x016E: // U with ring
    case 0x016F:
    case 0x0170: // U with double acute
    case 0x0171:
    case 0x0179: // Z with acute
    case 0x017A:
    case 0x017B: // Z with dot above
    case 0x017C:
    case 0x017D: // Z with caron
    case 0x017E:
        return true;
    default:
        return false;
    }
}

int centralEuropeanTextScore(const QString &text)
{
    int score = 0;
    for (const QChar character : text) {
        if (isCentralEuropeanLetter(character)) {
            score += 4;
            continue;
        }

        const ushort code = character.unicode();
        if (code == 0x00AB || code == 0x00BB || code == 0x00B5 || code == 0x00B8 || code == 0x00B9
            || code == 0x00BC || code == 0x00BE) {
            score -= 3;
            continue;
        }

        if (character.category() == QChar::Other_Control
            && character != QLatin1Char('\n')
            && character != QLatin1Char('\r')
            && character != QLatin1Char('\t')) {
            score -= 5;
        }
    }
    return score;
}

std::optional<QString> therionDeclaredEncoding(QByteArrayView data)
{
    // The Therion `encoding` directive is ASCII-compatible; scan the leading chunk quickly.
    const qsizetype probeLength = qMin<qsizetype>(data.size(), 8192);
    const QString probeText = QString::fromLatin1(data.first(probeLength));
    static const QRegularExpression kEncodingDirectiveRegex(
        QStringLiteral("(?m)^\\s*encoding\\s+([^\\s%#;]+)"));
    const QRegularExpressionMatch match = kEncodingDirectiveRegex.match(probeText);
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    const QString token = match.captured(1).trimmed();
    if (token.isEmpty()) {
        return std::nullopt;
    }

    return token;
}

QStringList encodingCandidatesFromDirectiveToken(const QString &token)
{
    QStringList candidates;
    const QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty()) {
        return candidates;
    }

    auto appendUnique = [&](const QString &value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !candidates.contains(trimmed, Qt::CaseInsensitive)) {
            candidates.append(trimmed);
        }
    };

    appendUnique(normalized);
    appendUnique(normalized.toUpper());

    if (normalized == QStringLiteral("utf8")) {
        appendUnique(QStringLiteral("utf-8"));
    } else if (normalized == QStringLiteral("utf16")) {
        appendUnique(QStringLiteral("utf-16"));
    } else if (normalized == QStringLiteral("latin1")) {
        appendUnique(QStringLiteral("iso-8859-1"));
    } else if (normalized == QStringLiteral("latin2")) {
        appendUnique(QStringLiteral("iso-8859-2"));
    } else if (normalized == QStringLiteral("locale")) {
        if (const char *systemName = QStringConverter::nameForEncoding(QStringConverter::System)) {
            appendUnique(QString::fromLatin1(systemName));
        }
        appendUnique(QStringLiteral("iso-8859-2"));
        appendUnique(QStringLiteral("windows-1250"));
    }

    if (normalized.startsWith(QStringLiteral("cp")) && normalized.size() > 2) {
        const QString suffix = normalized.mid(2);
        appendUnique(QStringLiteral("windows-%1").arg(suffix));
        appendUnique(QStringLiteral("ibm-%1").arg(suffix));
    }

    return candidates;
}
}

bool DocumentFile::readTextFile(const QString &filePath,
                                QString *contents,
                                QString *encodingName,
                                QString *encodingLabel,
                                QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile",
                                                        "Unable to open %1 for reading.")
                                .arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    const QByteArray data = file.readAll();

    auto tryDecodeEnum = [&](QStringConverter::Encoding candidateEncoding) {
        QString decoded;
        QString resolvedName;
        QString label;
        if (!decodeBytesWithEnum(QByteArrayView(data), candidateEncoding, &decoded, &resolvedName, &label)) {
            return false;
        }

        if (contents != nullptr) {
            *contents = decoded;
        }
        if (encodingName != nullptr) {
            *encodingName = resolvedName;
        }
        if (encodingLabel != nullptr) {
            *encodingLabel = label;
        }
        return true;
    };

    auto tryDecodeName = [&](const QString &candidateName) {
        QString decoded;
        QString resolvedName;
        QString label;
        if (!decodeBytesWithName(QByteArrayView(data), candidateName, &decoded, &resolvedName, &label)) {
            return false;
        }

        if (contents != nullptr) {
            *contents = decoded;
        }
        if (encodingName != nullptr) {
            *encodingName = resolvedName;
        }
        if (encodingLabel != nullptr) {
            *encodingLabel = label;
        }
        return true;
    };

    auto tryDecodeCentralEuropeanFallback = [&]() {
        struct Candidate
        {
            QString contents;
            QString encodingName;
            QString encodingLabel;
            int score = 0;
        };

        std::optional<Candidate> bestCandidate;
        const QStringList fallbackNames = {
            QStringLiteral("iso-8859-2"),
            QStringLiteral("windows-1250")
        };
        for (const QString &candidateName : fallbackNames) {
            QString decoded;
            QString resolvedName;
            QString label;
            if (!decodeBytesWithName(QByteArrayView(data), candidateName, &decoded, &resolvedName, &label)) {
                continue;
            }

            Candidate candidate;
            candidate.contents = decoded;
            candidate.encodingName = resolvedName;
            candidate.encodingLabel = label;
            candidate.score = centralEuropeanTextScore(decoded);
            if (candidate.score <= 0) {
                continue;
            }
            if (!bestCandidate.has_value() || candidate.score > bestCandidate->score) {
                bestCandidate = candidate;
            }
        }

        if (!bestCandidate.has_value()) {
            return false;
        }
        if (contents != nullptr) {
            *contents = bestCandidate->contents;
        }
        if (encodingName != nullptr) {
            *encodingName = bestCandidate->encodingName;
        }
        if (encodingLabel != nullptr) {
            *encodingLabel = bestCandidate->encodingLabel;
        }
        return true;
    };

    if (const auto declaredEncoding = therionDeclaredEncoding(QByteArrayView(data)); declaredEncoding.has_value()) {
        const QStringList declaredCandidates = encodingCandidatesFromDirectiveToken(*declaredEncoding);
        for (const QString &candidate : declaredCandidates) {
            if (tryDecodeName(candidate)) {
                return true;
            }
        }
    }

    const auto detectedEncoding = QStringConverter::encodingForData(QByteArrayView(data));
    if (detectedEncoding.has_value() && tryDecodeEnum(*detectedEncoding)) {
        return true;
    }

    if (tryDecodeEnum(QStringConverter::Utf8)) {
        return true;
    }

    if (tryDecodeCentralEuropeanFallback()) {
        return true;
    }

    if (tryDecodeEnum(QStringConverter::System)) {
        return true;
    }

    if (tryDecodeEnum(QStringConverter::Latin1)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile",
                                                    "Unable to decode %1 using supported text encodings.")
                            .arg(QFileInfo(filePath).fileName());
    }
    return false;
}

bool DocumentFile::writeTextFile(const QString &filePath,
                                 const QString &contents,
                                 const QString &encodingName,
                                 QString *errorMessage)
{
    const QString resolvedEncodingName = encodingName.trimmed().isEmpty()
        ? QStringLiteral("UTF-8")
        : encodingName.trimmed();

    const QString normalizedEncoding = resolvedEncodingName.trimmed().toLower();
    QStringConverter::Flags encoderFlags = QStringConverter::Flag::Default;
    if (normalizedEncoding == QStringLiteral("utf16") ||
        normalizedEncoding == QStringLiteral("utf-16") ||
        normalizedEncoding == QStringLiteral("utf-16le") ||
        normalizedEncoding == QStringLiteral("utf-16be") ||
        normalizedEncoding == QStringLiteral("utf32") ||
        normalizedEncoding == QStringLiteral("utf-32") ||
        normalizedEncoding == QStringLiteral("utf-32le") ||
        normalizedEncoding == QStringLiteral("utf-32be")) {
        encoderFlags |= QStringConverter::Flag::WriteBom;
    }

    const QByteArray nameBytes = encodingNameBytes(resolvedEncodingName);
    QStringEncoder encoder(nameBytes.constData(), encoderFlags);
    if (!encoder.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile",
                                                        "Unable to encode %1 using %2.")
                                .arg(QFileInfo(filePath).fileName(), encodingDisplayName(resolvedEncodingName));
        }
        return false;
    }

    const QByteArray encoded = encoder.encode(contents);
    if (encoder.hasError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile",
                                                        "Unable to encode %1 using %2.")
                                .arg(QFileInfo(filePath).fileName(), encodingDisplayName(resolvedEncodingName));
        }
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile",
                                                        "Unable to open %1 for writing.")
                                .arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    if (file.write(encoded) != encoded.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile", "Unable to write %1.")
                                .arg(QFileInfo(filePath).fileName());
        }
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::DocumentFile", "Unable to write %1.")
                                .arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    return true;
}

bool DocumentFile::readUtf8TextFile(const QString &filePath, QString *contents, QString *errorMessage)
{
    QString detectedEncoding = QStringLiteral("UTF-8");
    QString ignoredEncodingLabel;
    return readTextFile(filePath, contents, &detectedEncoding, &ignoredEncodingLabel, errorMessage);
}

bool DocumentFile::writeUtf8TextFile(const QString &filePath, const QString &contents, QString *errorMessage)
{
    return writeTextFile(filePath, contents, QStringLiteral("UTF-8"), errorMessage);
}
}
