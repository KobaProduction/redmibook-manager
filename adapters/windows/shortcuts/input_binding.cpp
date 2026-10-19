#include "adapters/platform/input_binding.hpp"
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <utility>
#include <windows.h>

namespace manager::adapters {
namespace {
class NativeInput final : public application::shortcuts::InputPort, public QAbstractNativeEventFilter {
  public:
    ~NativeInput() override {
        unbind();
    }
    application::shortcuts::BindingResult bind(application::shortcuts::KeyChord chord,
                                               std::function<void()> activated) override {
        using application::shortcuts::BindingStatus;
        if (chord.latin_key < 'A' || chord.latin_key > 'Z' || (!chord.control && !chord.alt) || !activated) {
            return {BindingStatus::Invalid, "letter-and-ctrl-or-alt-required"};
        }
        if (!QCoreApplication::instance()) {
            return {BindingStatus::Failed, "gui-event-loop-required"};
        }
        unbind();
        const UINT modifiers = MOD_NOREPEAT | (chord.control ? MOD_CONTROL : 0U) |
                               (chord.alt ? MOD_ALT : 0U) | (chord.shift ? MOD_SHIFT : 0U);
        if (!RegisterHotKey(nullptr, hotkey_id, modifiers, static_cast<UINT>(chord.latin_key))) {
            return {GetLastError() == ERROR_HOTKEY_ALREADY_REGISTERED ? BindingStatus::Conflict
                                                                      : BindingStatus::Failed,
                    "windows-hotkey-registration-failed"};
        }
        activated_ = std::move(activated);
        QCoreApplication::instance()->installNativeEventFilter(this);
        return {BindingStatus::Active, "windows-register-hotkey"};
    }
    void unbind() override {
        if (!activated_) {
            return;
        }
        if (QCoreApplication::instance()) {
            QCoreApplication::instance()->removeNativeEventFilter(this);
        }
        UnregisterHotKey(nullptr, hotkey_id);
        activated_ = {};
    }
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override {
        const auto *event = static_cast<MSG *>(message);
        if (event->message != WM_HOTKEY || event->wParam != hotkey_id || !activated_) {
            return false;
        }
        activated_();
        *result = 0;
        return true;
    }

  private:
    static constexpr int hotkey_id = 0x5242;
    std::function<void()> activated_;
};
} // namespace
std::unique_ptr<application::shortcuts::InputPort> make_native_input() {
    return std::make_unique<NativeInput>();
}
} // namespace manager::adapters
