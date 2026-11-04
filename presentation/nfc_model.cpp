#include "presentation/nfc_model.hpp"
#include "application/nfc/ndef.hpp"
#include "presentation/nfc_records.hpp"
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>

namespace manager::presentation {
using application::nfc::Status;
NfcModel::NfcModel(std::shared_ptr<application::nfc::ClientPort> client, QObject *parent,
                   std::shared_ptr<application::nfc::FilePort> files)
    : QObject(parent), client_(std::move(client)), files_(std::move(files)) {
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
    const bool draft_invalidated = draft_.open() && (state_.content != state.content ||
                                                     state_.max_content_size != state.max_content_size);
    state_ = state;
    emit changed();
    if (draft_invalidated)
        emit draftChanged();
}
bool NfcModel::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        emit changed();
        emit draftChanged();
    }
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
    case Status::DriverMissing:
        return tr("The NXP NFC driver is not installed. Install the laptop manufacturer's NFC driver.");
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
    return *state_.field ? tr("NFC device nearby") : tr("Bring a phone near the NFC area");
}
QVariantList NfcModel::records() const {
    return nfcRecords(state_.content);
}
void NfcModel::refresh() {
    if (client_)
        client_->refresh();
}
void NfcModel::write(application::nfc::WriteRequest request) {
    if (busy_ || !client_)
        return;
    busy_ = true;
    emit changed();
    const auto accepted =
        client_->write(std::move(request), [guard = QPointer<NfcModel>(this)](Status status) {
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
    else {
        endDraft();
        emit writeFinished();
    }
}
void NfcModel::exportTag(const QUrl &url) {
    if (!ready()) {
        emit errorOccurred(tr("Could not save the NDEF file."));
        return;
    }
    saveFile(url, state_.content);
}
} // namespace manager::presentation
