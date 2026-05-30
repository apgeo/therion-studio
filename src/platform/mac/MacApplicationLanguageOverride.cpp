#include "../ApplicationLanguageOverride.h"

#include <CoreFoundation/CoreFoundation.h>

#include <QByteArray>
#include <QString>

#include <limits>

namespace
{
const CFStringRef kAppleLanguagesKey = CFSTR("AppleLanguages");

QString cfStringToQString(CFStringRef string)
{
    if (string == nullptr) {
        return QString();
    }

    const CFIndex length = CFStringGetLength(string);
    const CFIndex maximumSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    if (maximumSize <= 0 || maximumSize > std::numeric_limits<int>::max()) {
        return QString();
    }

    QByteArray buffer(static_cast<int>(maximumSize), '\0');
    if (!CFStringGetCString(string, buffer.data(), buffer.size(), kCFStringEncodingUTF8)) {
        return QString();
    }

    return QString::fromUtf8(buffer.constData());
}

CFStringRef createCFString(const QString &string)
{
    const QByteArray utf8 = string.toUtf8();
    return CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8 *>(utf8.constData()),
                                   static_cast<CFIndex>(utf8.size()),
                                   kCFStringEncodingUTF8,
                                   false);
}

QString languageFromPreferencesValue(CFPropertyListRef value)
{
    if (value == nullptr || CFGetTypeID(value) != CFArrayGetTypeID()) {
        return QStringLiteral("system");
    }

    const auto languages = static_cast<CFArrayRef>(value);
    if (CFArrayGetCount(languages) <= 0) {
        return QStringLiteral("system");
    }

    const void *firstValue = CFArrayGetValueAtIndex(languages, 0);
    if (firstValue == nullptr || CFGetTypeID(firstValue) != CFStringGetTypeID()) {
        return QStringLiteral("system");
    }

    return TherionStudio::Platform::normalizeApplicationLanguageSetting(
        cfStringToQString(static_cast<CFStringRef>(firstValue)));
}
} // namespace

namespace TherionStudio::Platform
{

QString applicationLanguageOverride()
{
    CFPropertyListRef value =
        CFPreferencesCopyAppValue(kAppleLanguagesKey, kCFPreferencesCurrentApplication);
    const QString language = languageFromPreferencesValue(value);
    if (value != nullptr) {
        CFRelease(value);
    }
    return language;
}

void setApplicationLanguageOverride(const QString &language)
{
    const QString normalizedLanguage = normalizeApplicationLanguageSetting(language);
    if (normalizedLanguage == QStringLiteral("system")) {
        CFPreferencesSetAppValue(kAppleLanguagesKey, nullptr, kCFPreferencesCurrentApplication);
        CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
        return;
    }

    CFStringRef languageValue = createCFString(normalizedLanguage);
    if (languageValue == nullptr) {
        return;
    }

    const void *values[] = {languageValue};
    CFArrayRef languages =
        CFArrayCreate(kCFAllocatorDefault, values, 1, &kCFTypeArrayCallBacks);
    CFRelease(languageValue);

    if (languages == nullptr) {
        return;
    }

    CFPreferencesSetAppValue(kAppleLanguagesKey, languages, kCFPreferencesCurrentApplication);
    CFRelease(languages);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

} // namespace TherionStudio::Platform
