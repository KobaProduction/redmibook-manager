#pragma once
#include "application/nfc/draft.hpp"
#include "application/nfc/files.hpp"
#include "application/nfc/port.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace manager::presentation {
class NfcModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(QVariantList draftRecords READ draftRecords NOTIFY draftChanged)
    Q_PROPERTY(int draftSize READ draftSize NOTIFY draftChanged)
    Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY draftChanged)
    Q_PROPERTY(bool draftConflict READ draftConflict NOTIFY draftChanged)
    Q_PROPERTY(bool draftFits READ draftFits NOTIFY draftChanged)
    Q_PROPERTY(int contentLimit READ contentLimit NOTIFY changed)
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool writable READ writable NOTIFY changed)
    Q_PROPERTY(bool eventDriven READ eventDriven NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString fieldText READ fieldText NOTIFY changed)
    Q_PROPERTY(int capacity READ capacity NOTIFY changed)
    Q_PROPERTY(int used READ used NOTIFY changed)
    Q_PROPERTY(QVariantList records READ records NOTIFY changed)
  public:
    explicit NfcModel(std::shared_ptr<application::nfc::ClientPort> client, QObject *parent = nullptr,
                      std::shared_ptr<application::nfc::FilePort> files = {});
    ~NfcModel() override;
    bool present() const {
        return state_.present;
    }
    bool ready() const {
        return state_.status == application::nfc::Status::Ready;
    }
    bool writable() const {
        return state_.writable && ready() && !busy_;
    }
    bool busy() const {
        return busy_;
    }
    bool eventDriven() const {
        return state_.event_driven;
    }
    QString name() const {
        return QString::fromStdString(state_.name);
    }
    QString statusText() const;
    QString fieldText() const;
    int capacity() const {
        return static_cast<int>(state_.capacity);
    }
    int used() const {
        return static_cast<int>(state_.content.size());
    }
    QVariantList records() const;
    QVariantList draftRecords() const;
    int draftSize() const;
    bool draftDirty() const;
    bool draftConflict() const;
    bool draftFits() const;
    int contentLimit() const {
        return static_cast<int>(state_.max_content_size);
    }
    Q_INVOKABLE bool beginDraft();
    Q_INVOKABLE void endDraft();
    Q_INVOKABLE bool putDraftRecord(int index, const QString &kind, const QString &value);
    Q_INVOKABLE void removeDraftRecord(int index);
    Q_INVOKABLE void duplicateDraftRecord(int index);
    Q_INVOKABLE void moveDraftRecord(int from, int to);
    Q_INVOKABLE void clearDraft();
    QString draftHex() const;
    Q_INVOKABLE void saveDraft();
    Q_INVOKABLE void importDraft(const QUrl &url);
    Q_INVOKABLE void exportDraft(const QUrl &url);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void exportTag(const QUrl &url);
    void acceptState(const application::nfc::Snapshot &state);
  signals:
    void changed();
    void draftChanged();
    void writeFinished();
    void errorOccurred(const QString &message);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    std::shared_ptr<application::nfc::ClientPort> client_;
    application::nfc::Snapshot state_;
    std::shared_ptr<application::nfc::FilePort> files_;
    application::nfc::Draft draft_;
    quint64 draft_epoch_{};
    bool replaceDraft(std::vector<application::nfc::Record> records);
    bool busy_{};
    void write(application::nfc::WriteRequest request);
    void saveFile(const QUrl &url, application::nfc::Bytes content);
    void finishFile(application::nfc::Status status, bool loading);
    void finishImport(const application::nfc::FileResult &result, quint64 epoch);
    void finish(application::nfc::Status status);
    static QString statusMessage(application::nfc::Status status);
};
} // namespace manager::presentation
