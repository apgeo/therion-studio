#include "DocumentFile.h"

#include <QFile>
#include <QFileInfo>
#include <QByteArrayView>

namespace TherionStudio
{
namespace
{
QString encodingDisplayName(QStringConverter::Encoding encoding)
{
    const char *name = QStringConverter::nameForEncoding(encoding);
    if (name == nullptr || *name == '\0') {
        return QStringLiteral("UTF-8");
    }

    return QString::fromLatin1(name).toUpper();
}

bool decodeBytes(QByteArrayView data,
                 QStringConverter::Encoding encoding,
                 QString *contents,
                 QString *encodingLabel)
{
    QStringDecoder decoder(encoding, QStringConverter::Flag::ConvertInvalidToNull);
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
    if (encodingLabel != nullptr) {
        *encodingLabel = encodingDisplayName(encoding);
    }

    return true;
}
}

bool DocumentFile::readTextFile(const QString &filePath,
                                QString *contents,
                                QStringConverter::Encoding *encoding,
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

    auto tryDecode = [&](QStringConverter::Encoding candidateEncoding) {
        QString decoded;
        QString label;
        if (!decodeBytes(QByteArrayView(data), candidateEncoding, &decoded, &label)) {
            return false;
        }

        if (contents != nullptr) {
            *contents = decoded;
        }
        if (encoding != nullptr) {
            *encoding = candidateEncoding;
        }
        if (encodingLabel != nullptr) {
            *encodingLabel = label;
        }
        return true;
    };

    const auto detectedEncoding = QStringConverter::encodingForData(QByteArrayView(data));
    if (detectedEncoding.has_value() && tryDecode(*detectedEncoding)) {
        return true;
    }

    if (tryDecode(QStringConverter::Utf8)) {
        return true;
    }

    if (tryDecode(QStringConverter::System)) {
        return true;
    }

    if (tryDecode(QStringConverter::Latin1)) {
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
                                 QStringConverter::Encoding encoding,
                                 QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to open %1 for writing.").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    QStringEncoder encoder(encoding, QStringConverter::Flag::Default);
    const QByteArray encoded = encoder.encode(contents);
    if (encoder.hasError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to encode %1 using %2.")
                                .arg(QFileInfo(filePath).fileName(), encodingDisplayName(encoding));
        }
        return false;
    }

    if (file.write(encoded) == -1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to write %1.").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    return true;
}

bool DocumentFile::readUtf8TextFile(const QString &filePath, QString *contents, QString *errorMessage)
{
    QStringConverter::Encoding detectedEncoding = QStringConverter::Utf8;
    QString ignoredEncodingLabel;
    return readTextFile(filePath, contents, &detectedEncoding, &ignoredEncodingLabel, errorMessage);
}

bool DocumentFile::writeUtf8TextFile(const QString &filePath, const QString &contents, QString *errorMessage)
{
    return writeTextFile(filePath, contents, QStringConverter::Utf8, errorMessage);
}
}
