#include "DocumentFile.h"

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
            *errorMessage = QStringLiteral("Unable to open %1 for reading.").arg(QFileInfo(filePath).fileName());
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

    if (tryDecodeEnum(QStringConverter::System)) {
        return true;
    }

    if (tryDecodeEnum(QStringConverter::Latin1)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Unable to decode %1 using supported text encodings.")
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

    const QByteArray nameBytes = encodingNameBytes(resolvedEncodingName);
    QStringEncoder encoder(nameBytes.constData(), QStringConverter::Flag::Default);
    if (!encoder.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to encode %1 using %2.")
                                .arg(QFileInfo(filePath).fileName(), encodingDisplayName(resolvedEncodingName));
        }
        return false;
    }

    const QByteArray encoded = encoder.encode(contents);
    if (encoder.hasError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to encode %1 using %2.")
                                .arg(QFileInfo(filePath).fileName(), encodingDisplayName(resolvedEncodingName));
        }
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to open %1 for writing.").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    if (file.write(encoded) != encoded.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to write %1.").arg(QFileInfo(filePath).fileName());
        }
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to write %1.").arg(QFileInfo(filePath).fileName());
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
