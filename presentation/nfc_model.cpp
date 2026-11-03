#include "presentation/nfc_model.hpp"
#include "application/nfc/ndef.hpp"
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>

namespace manager::presentation {
namespace {
using application::nfc::Bytes;
using application::nfc::Status;
QByteArray raw(const Bytes &bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}
QString record_type(const application::nfc::Record &record) {
    return QString::fromUtf8(reinterpret_cast<const char *>(record.type.data()),
                             static_cast<qsizetype>(record.type.size()));
}
} // namespace
NfcModel::NfcModel(std::shared_ptr<application::nfc::ClientPort> client, QObject *parent)
    : QObject(parent), client_(std::move(client)) {
    QCoreApplication::instance()->installEventFilter(this);
    if (client_)
        client_->observe([guard = QPointer<NfcModel>(this)](const auto &state) {
            if (guard)
                guard->acceptState(state);
        });
}
NfcModel::~NfcModel() {
    if (client_)
        client_->observe({});
}
void NfcModel::acceptState(const application::nfc::Snapshot &state) {
    if (state_ == state)
        return;
    state_ = state;
    emit changed();
}
bool NfcModel::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        emit changed();
    return QObject::eventFilter(watched, event);
}
QString NfcModel::statusMessage(Status status) {
    switch (status) {
    case Status::Ready:
        return tr("Ready");
    case Status::Absent:
        return tr("NFC module not detected");
    case Status::Denied:
        return tr("Access to the NFC module was denied.");
    case Status::Busy:
        return tr("The NFC module is in use. Try again after removing the phone.");
    case Status::ReadOnly:
        return tr("The tag is write-protected.");
    case Status::Conflict:
        return tr("The tag changed. Refresh its contents before writing.");
    case Status::InvalidData:
        return tr("The NFC data is invalid or exceeds the tag capacity.");
    case Status::TimedOut:
        return tr("The NFC operation timed out.");
    case Status::UnknownOutcome:
        return tr("The write could not be confirmed. Refresh and inspect the tag before another write.");
    case Status::Incompatible:
        return tr("The NFC components or driver are incompatible.");
    case Status::Unavailable:
        return tr("The NFC session is unavailable.");
    case Status::Failed:
        return tr("The NFC operation failed.");
    }
    return {};
}
QString NfcModel::statusText() const {
    return statusMessage(state_.status);
}
QString NfcModel::fieldText() const {
    if (!state_.field)
        return tr("NFC field state is unknown");
    return *state_.field ? tr("NFC device nearby") : tr("No NFC field");
}
QString NfcModel::contentHex() const {
    return QString::fromLatin1(raw(state_.content).toHex(' '));
}
QVariantList NfcModel::records() const {
    QVariantList result;
    const auto decoded = application::nfc::decode_ndef(state_.content);
    if (!decoded)
        return result;
    for (const auto &record : *decoded)
        result.append(
            QVariantMap{{"type", record_type(record)}, {"bytes", static_cast<int>(record.payload.size())}});
    return result;
}
void NfcModel::refresh() {
    if (client_)
        client_->refresh();
}
void NfcModel::write(Bytes content) {
    if (!writable() || !client_ || !application::nfc::decode_ndef(content)) {
        emit errorOccurred(statusMessage(Status::InvalidData));
        return;
    }
    busy_ = true;
    emit changed();
    const auto accepted = client_->write({state_.content, std::move(content)},
                                         [guard = QPointer<NfcModel>(this)](Status status) {
                                             if (guard)
                                                 guard->finish(status);
                                         });
    if (!accepted)
        finish(Status::Unavailable);
}
void NfcModel::finish(Status status) {
    busy_ = false;
    emit changed();
    if (status != Status::Ready)
        emit errorOccurred(statusMessage(status));
}
void NfcModel::writeText(const QString &text) {
    auto content = application::nfc::make_text_record(text.toUtf8().toStdString());
    if (content.empty())
        emit errorOccurred(statusMessage(Status::InvalidData));
    else
        write(std::move(content));
}
void NfcModel::writeUri(const QString &uri) {
    const auto url = QUrl(uri);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) {
        emit errorOccurred(tr("Enter a valid HTTP or HTTPS link."));
        return;
    }
    auto content = application::nfc::make_uri_record(url.toEncoded().toStdString());
    if (content.empty())
        emit errorOccurred(statusMessage(Status::InvalidData));
    else
        write(std::move(content));
}
void NfcModel::writeHex(const QString &hex) {
    auto compact = hex;
    compact.remove(QRegularExpression(QStringLiteral("\\s")));
    if (compact.size() > 4096 || compact.size() % 2 != 0 ||
        compact.contains(QRegularExpression(QStringLiteral("[^0-9a-fA-F]")))) {
        emit errorOccurred(statusMessage(Status::InvalidData));
        return;
    }
    const auto bytes = QByteArray::fromHex(compact.toLatin1());
    write(Bytes(bytes.begin(), bytes.end()));
}
void NfcModel::exportTag(const QUrl &url) {
    if (!url.isLocalFile() || state_.status != Status::Ready) {
        emit errorOccurred(tr("Could not save the NFC backup."));
        return;
    }
    const auto bytes = raw(state_.content);
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        emit errorOccurred(tr("Could not save the NFC backup."));
}
QString NfcModel::readBackup(const QUrl &url) {
    QFile file(url.toLocalFile());
    if (!url.isLocalFile() || !file.open(QIODevice::ReadOnly) || file.size() > 2048) {
        emit errorOccurred(tr("Could not read the NFC backup."));
        return {};
    }
    const auto bytes = file.readAll();
    const Bytes content(bytes.begin(), bytes.end());
    if (!application::nfc::decode_ndef(content)) {
        emit errorOccurred(statusMessage(Status::InvalidData));
        return {};
    }
    return QString::fromLatin1(bytes.toHex(' '));
}
} // namespace manager::presentation
