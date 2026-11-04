#include "presentation/nfc_records.hpp"
#include "application/nfc/ndef.hpp"
#include <QCoreApplication>
#include <QStringDecoder>
#include <array>

namespace manager::presentation {
namespace {
namespace app = application::nfc;
QByteArray raw(const app::Bytes &content) {
    return {reinterpret_cast<const char *>(content.data()), static_cast<qsizetype>(content.size())};
}
QString uri(const QByteArray &payload) {
    static constexpr std::array prefixes{"",
                                         "http://www.",
                                         "https://www.",
                                         "http://",
                                         "https://",
                                         "tel:",
                                         "mailto:",
                                         "ftp://anonymous:anonymous@",
                                         "ftp://ftp.",
                                         "ftps://",
                                         "sftp://",
                                         "smb://",
                                         "nfs://",
                                         "ftp://",
                                         "dav://",
                                         "news:",
                                         "telnet://",
                                         "imap:",
                                         "rtsp://",
                                         "urn:",
                                         "pop:",
                                         "sip:",
                                         "sips:",
                                         "tftp:",
                                         "btspp://",
                                         "btl2cap://",
                                         "btgoep://",
                                         "tcpobex://",
                                         "irdaobex://",
                                         "file://",
                                         "urn:epc:id:",
                                         "urn:epc:tag:",
                                         "urn:epc:pat:",
                                         "urn:epc:raw:",
                                         "urn:epc:",
                                         "urn:nfc:"};
    if (payload.isEmpty() || static_cast<unsigned char>(payload[0]) >= prefixes.size())
        return {};
    return QString::fromLatin1(prefixes[static_cast<unsigned char>(payload[0])]) +
           QString::fromUtf8(payload.mid(1));
}
std::optional<QString> text(const QByteArray &payload) {
    if (payload.isEmpty())
        return {};
    const auto status = static_cast<unsigned char>(payload[0]);
    const auto offset = 1 + (status & 0x3f);
    if (offset > payload.size() || (status & 0x40) != 0)
        return {};
    const auto value = payload.mid(offset);
    if ((status & 0x80) == 0)
        return QString::fromUtf8(value);
    const auto encoding =
        value.startsWith(QByteArray::fromHex("fffe")) ? QStringDecoder::Utf16LE : QStringDecoder::Utf16BE;
    return QString(QStringDecoder(encoding)(value));
}
QVariantMap describe(const app::Record &record) {
    const auto payload = raw(record.payload);
    const auto type = raw(record.type);
    if (record.tnf == 1 && type == "U" && !uri(payload).isEmpty())
        return {{"kind", "uri"},
                {"title", uri(payload).startsWith("tel:")
                              ? QCoreApplication::translate("NfcRecords", "Phone number")
                          : uri(payload).startsWith("mailto:")
                              ? QCoreApplication::translate("NfcRecords", "Email address")
                              : QCoreApplication::translate("NfcRecords", "Link / URI")},
                {"value", uri(payload)}};
    const auto decoded_text = text(payload);
    if (record.tnf == 1 && type == "T" && decoded_text)
        return {{"kind", "text"},
                {"title", QCoreApplication::translate("NfcRecords", "Text")},
                {"value", *decoded_text}};
    if (record.tnf == 4 && type == "com.xiaomi.mi_connect_service:externaltype")
        return {{"kind", "binary"},
                {"title", QCoreApplication::translate("NfcRecords", "Xiaomi connection record")},
                {"value", QString::fromUtf8(type)}};
    return {{"kind", "binary"},
            {"title", QCoreApplication::translate("NfcRecords", "NFC record")},
            {"value", QString::fromUtf8(type.isEmpty() ? QByteArray("NDEF") : type)}};
}
} // namespace
QVariantList nfcRecords(const app::Bytes &content) {
    QVariantList result;
    if (const auto decoded = app::decode_ndef(content))
        for (const auto &record : *decoded)
            result.append(describe(record));
    return result;
}
} // namespace manager::presentation
