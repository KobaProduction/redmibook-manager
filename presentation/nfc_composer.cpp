#include "presentation/nfc_model.hpp"
#include "presentation/nfc_records.hpp"
#include <QPointer>
#include <QRegularExpression>

namespace manager::presentation {
namespace {
namespace app = application::nfc;
enum class RecordKind : std::uint8_t { Text, Uri, Phone, Email, Invalid };
RecordKind record_kind(const QString &kind) {
    if (kind == "text")
        return RecordKind::Text;
    if (kind == "uri")
        return RecordKind::Uri;
    if (kind == "phone")
        return RecordKind::Phone;
    if (kind == "email")
        return RecordKind::Email;
    return RecordKind::Invalid;
}
QString phone_uri(QString value) {
    value.remove(QRegularExpression(QStringLiteral("[() .-]")));
    if (!QRegularExpression(QStringLiteral("^\\+?[0-9]{3,20}$")).match(value).hasMatch())
        return {};
    return value.prepend(QStringLiteral("tel:"));
}
QString email_uri(QString value) {
    const auto at = value.indexOf('@');
    if (at <= 0 || at + 1 >= value.size() || value.lastIndexOf('@') != at ||
        value.contains(QRegularExpression(QStringLiteral("\\s"))))
        return {};
    return value.prepend(QStringLiteral("mailto:"));
}
std::optional<QString> uri_value(RecordKind kind, QString value) {
    value = value.trimmed();
    if (kind == RecordKind::Phone)
        value = phone_uri(value);
    else if (kind == RecordKind::Email)
        value = email_uri(value);
    else if (kind != RecordKind::Uri)
        return {};
    const QUrl uri(value.trimmed());
    if (!uri.isValid() || uri.scheme().isEmpty())
        return {};
    return QString::fromLatin1(uri.toEncoded());
}
std::optional<app::Record> make_record(RecordKind kind, const QString &value) {
    app::Bytes bytes;
    if (kind == RecordKind::Text)
        bytes = app::make_text_record(value.toUtf8().toStdString());
    else if (const auto uri = uri_value(kind, value))
        bytes = app::make_uri_record(uri->toStdString());
    const auto records = app::decode_ndef(bytes);
    return records && records->size() == 1 ? std::optional{records->front()} : std::nullopt;
}
} // namespace
bool NfcModel::beginDraft() {
    if (busy_)
        return false;
    if (draft_.open())
        return true;
    const auto status = draft_.begin(state_);
    if (status != app::Status::Ready) {
        emit errorOccurred(statusMessage(status));
        return false;
    }
    ++draft_epoch_;
    emit draftChanged();
    return true;
}
void NfcModel::endDraft() {
    draft_.end();
    ++draft_epoch_;
    emit draftChanged();
}
QVariantList NfcModel::draftRecords() const {
    const auto bytes = draft_.bytes();
    return bytes ? nfcRecords(*bytes) : QVariantList{};
}
int NfcModel::draftSize() const {
    const auto bytes = draft_.bytes();
    return bytes ? static_cast<int>(bytes->size()) : 2049;
}
bool NfcModel::draftDirty() const {
    return draft_.dirty();
}
bool NfcModel::draftConflict() const {
    return draft_.conflict(state_);
}
bool NfcModel::draftFits() const {
    return draftSize() <= contentLimit();
}
bool NfcModel::putDraftRecord(int index, const QString &kind, const QString &value) {
    auto record = make_record(record_kind(kind), value);
    if (!record) {
        emit errorOccurred(tr("Enter a valid record value."));
        return false;
    }
    if (busy_)
        return false;
    const auto status = draft_.put(index, std::move(*record));
    if (status != app::Status::Ready) {
        emit errorOccurred(statusMessage(status));
        return false;
    }
    emit draftChanged();
    return true;
}
bool NfcModel::replaceDraft(std::vector<app::Record> records) {
    if (busy_)
        return false;
    const auto status = draft_.replace(std::move(records));
    if (status != app::Status::Ready) {
        emit errorOccurred(statusMessage(status));
        return false;
    }
    emit draftChanged();
    return true;
}
void NfcModel::removeDraftRecord(int index) {
    if (!busy_ && draft_.remove(index))
        emit draftChanged();
}
void NfcModel::moveDraftRecord(int from, int to) {
    if (!busy_ && draft_.move(from, to))
        emit draftChanged();
}
void NfcModel::duplicateDraftRecord(int index) {
    if (!busy_ && draft_.duplicate(index) == app::Status::Ready)
        emit draftChanged();
}
void NfcModel::clearDraft() {
    replaceDraft({});
}
QString NfcModel::draftHex() const {
    const auto bytes = draft_.bytes();
    if (!bytes)
        return {};
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(bytes->data()), static_cast<qsizetype>(bytes->size()))
            .toHex(' '));
}
void NfcModel::saveDraft() {
    app::WriteRequest request;
    const auto status = draft_.prepare(state_, request);
    if (status != app::Status::Ready) {
        emit errorOccurred(statusMessage(status));
        return;
    }
    write(std::move(request));
}
void NfcModel::importDraft(const QUrl &url) {
    if (busy_ || !draft_.open())
        return;
    if (!files_ || !url.isLocalFile()) {
        emit errorOccurred(tr("Could not read the NDEF file."));
        return;
    }
    busy_ = true;
    emit changed();
    const auto accepted =
        files_->load(url.toLocalFile().toStdString(),
                     [guard = QPointer<NfcModel>(this), epoch = draft_epoch_](const app::FileResult &result) {
                         if (guard)
                             guard->finishImport(result, epoch);
                     });
    if (!accepted)
        finishFile(app::Status::Busy, true);
}
void NfcModel::finishImport(const app::FileResult &result, quint64 epoch) {
    const QPointer<NfcModel> guard(this);
    finishFile(result.status, true);
    if (!guard || result.status != app::Status::Ready || epoch != guard->draft_epoch_)
        return;
    const auto records = app::decode_ndef(result.content);
    if (records)
        guard->replaceDraft(*records);
    else
        emit guard->errorOccurred(statusMessage(app::Status::InvalidData));
}
void NfcModel::exportDraft(const QUrl &url) {
    const auto bytes = draft_.bytes();
    if (draft_.open() && bytes)
        saveFile(url, *bytes);
    else
        emit errorOccurred(tr("Could not save the NDEF file."));
}
void NfcModel::saveFile(const QUrl &url, app::Bytes content) {
    if (busy_)
        return;
    if (!files_ || !url.isLocalFile()) {
        emit errorOccurred(tr("Could not save the NDEF file."));
        return;
    }
    busy_ = true;
    emit changed();
    const auto accepted = files_->save(url.toLocalFile().toStdString(), std::move(content),
                                       [guard = QPointer<NfcModel>(this)](const app::FileResult &result) {
                                           if (guard)
                                               guard->finishFile(result.status, false);
                                       });
    if (!accepted)
        finishFile(app::Status::Busy, false);
}
void NfcModel::finishFile(app::Status status, bool loading) {
    busy_ = false;
    emit changed();
    if (status == app::Status::TimedOut || status == app::Status::UnknownOutcome)
        emit errorOccurred(tr("The file operation timed out. Its completion could not be confirmed."));
    else if (status != app::Status::Ready)
        emit errorOccurred(loading ? tr("Could not read the NDEF file.")
                                   : tr("Could not save the NDEF file."));
}
} // namespace manager::presentation
