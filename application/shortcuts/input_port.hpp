#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace manager::application::shortcuts {
enum class BindingStatus : std::uint8_t { Active, NotImplemented, Invalid, Conflict, Failed };
struct KeyChord {
    char latin_key{};
    bool control{};
    bool alt{};
    bool shift{};
};
struct BindingResult {
    BindingStatus status{BindingStatus::NotImplemented};
    std::string detail;
};
class InputPort {
  public:
    virtual ~InputPort() = default;
    // GUI thread only. Callback runs on that same thread; no callback after unbind/destruction.
    virtual BindingResult bind(KeyChord chord, std::function<void()> activated) = 0;
    virtual void unbind() = 0;
};
} // namespace manager::application::shortcuts
