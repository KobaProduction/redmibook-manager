#include "adapters/platform/power_scheme.hpp"
#include <windows.h>

#include <algorithm>
#include <array>
#include <objbase.h>
#include <powrprof.h>
#include <vector>

namespace manager::adapters {
namespace {
using application::power::Status;
std::string utf8(const wchar_t *value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};
    std::string text(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, text.data(), size, nullptr, nullptr);
    text.pop_back();
    return text;
}
std::string guid_text(const GUID &id) {
    std::array<wchar_t, 40> buffer{};
    return StringFromGUID2(id, buffer.data(), static_cast<int>(buffer.size())) > 0 ? utf8(buffer.data())
                                                                                   : std::string{};
}
std::string friendly(const GUID &id) {
    DWORD bytes{};
    if (PowerReadFriendlyName(nullptr, &id, nullptr, nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
        bytes == 0 || bytes > 4096)
        return guid_text(id);
    std::vector<wchar_t> buffer(static_cast<std::size_t>(bytes) / sizeof(wchar_t) + 1, L'\0');
    if (PowerReadFriendlyName(nullptr, &id, nullptr, nullptr, reinterpret_cast<UCHAR *>(buffer.data()),
                              &bytes) != ERROR_SUCCESS)
        return guid_text(id);
    return utf8(buffer.data());
}
class Schemes final : public application::power::SchemePort {
    static DWORD enumerate_scheme(ULONG index, GUID &id) {
        DWORD bytes = sizeof(id);
        const auto error = PowerEnumerate(nullptr, nullptr, nullptr, ACCESS_SCHEME, index,
                                          reinterpret_cast<UCHAR *>(&id), &bytes);
        return error == ERROR_SUCCESS && bytes != sizeof(id) ? ERROR_INVALID_DATA : error;
    }
    static std::string active_scheme() {
        GUID *active{};
        if (PowerGetActiveScheme(nullptr, &active) != ERROR_SUCCESS)
            return {};
        const auto id = guid_text(*active);
        LocalFree(active);
        return id;
    }

  public:
    application::power::Snapshot read() const override {
        application::power::Snapshot result;
        result.active = active_scheme();
        if (result.active.empty())
            return result;
        for (ULONG index = 0; index < 128; ++index) {
            GUID id{};
            const auto error = enumerate_scheme(index, id);
            if (error == ERROR_NO_MORE_ITEMS) {
                result.status = Status::Ready;
                return result;
            }
            if (error != ERROR_SUCCESS)
                return result;
            result.schemes.push_back({guid_text(id), friendly(id)});
        }
        return result;
    }
    Status select(const std::string &id) override {
        const auto before = read();
        if (before.status != Status::Ready ||
            std::ranges::none_of(before.schemes, [&id](const auto &scheme) { return scheme.id == id; }))
            return Status::Unavailable;
        const std::wstring wide(id.begin(), id.end());
        GUID guid{};
        if (CLSIDFromString(wide.c_str(), &guid) != S_OK)
            return Status::Failed;
        const auto error = PowerSetActiveScheme(nullptr, &guid);
        if (error != ERROR_SUCCESS)
            return error == ERROR_ACCESS_DENIED ? Status::Denied : Status::Failed;
        const auto after = read();
        return after.status == Status::Ready && after.active == id ? Status::Ready : Status::Unconfirmed;
    }
};
} // namespace
std::shared_ptr<application::power::SchemePort> make_power_schemes() {
    return std::make_shared<Schemes>();
}
} // namespace manager::adapters
