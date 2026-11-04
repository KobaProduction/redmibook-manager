#pragma once
#include "application/nfc/port.hpp"

namespace manager::application::nfc {
struct FileResult {
    Status status{Status::Failed};
    Bytes content;
};
// UTF-8 local paths; one pending operation. Callbacks use the owning event loop.
// A timeout reports uncertainty, not cancellation. Destruction drops callbacks;
// implementations retain dependencies until the native file operation completes.
class FilePort {
  public:
    using Completion = std::function<void(FileResult)>;
    virtual ~FilePort() = default;
    virtual bool load(std::string path, Completion completion) = 0;
    virtual bool save(std::string path, Bytes content, Completion completion) = 0;
};
} // namespace manager::application::nfc
