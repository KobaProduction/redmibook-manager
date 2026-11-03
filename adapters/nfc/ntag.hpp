#pragma once
#include "adapters/nfc/transport.hpp"
#include <mutex>

namespace manager::adapters::nfc {
class Ntag final : public application::nfc::TagPort {
  public:
    explicit Ntag(std::shared_ptr<Transport> transport);
    application::nfc::Snapshot read() override;
    application::nfc::Result write(const application::nfc::WriteRequest &request) override;
    std::unique_ptr<application::nfc::Subscription> subscribe(std::function<void()> changed) override;

  private:
    struct ImageChange {
        const Bytes &before;
        const Bytes &after;
    };
    std::shared_ptr<Transport> transport_;
    std::mutex mutex_;
    application::nfc::Snapshot state_;
    application::nfc::Snapshot read_unlocked();
    Status read_content();
    application::nfc::Result perform_write(const Bytes &before,
                                           const application::nfc::WriteRequest &request);
    Status read_image(Bytes &image);
    Status write_blocks(ImageChange change);
    Status commit_image(ImageChange change);
    Status finish_programming();
    void update_field();
};
} // namespace manager::adapters::nfc
