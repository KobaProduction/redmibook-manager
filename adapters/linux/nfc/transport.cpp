#include "adapters/nfc/ntag.hpp"
#include "adapters/platform/nfc.hpp"
#include <array>
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <string_view>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace manager::adapters::nfc {
namespace {
namespace fs = std::filesystem;
Status native_error() {
    if (errno == EACCES || errno == EPERM)
        return Status::Denied;
    if (errno == EBUSY || errno == EWOULDBLOCK)
        return Status::Busy;
    if (errno == ETIMEDOUT)
        return Status::TimedOut;
    if (errno == ENOENT || errno == ENODEV)
        return Status::Unavailable;
    return Status::Failed;
}
std::string line(const fs::path &path) {
    std::ifstream stream(path);
    std::string value;
    std::getline(stream, value);
    return value;
}
bool identified(const fs::path &path) {
    const auto name = line(path / "name");
    return name == "NTAG5332:00" || name == "NTAG5332" || line(path / "firmware_node" / "hid") == "NTAG5332";
}
std::optional<fs::path> find_tag() {
    std::error_code error;
    for (fs::directory_iterator it("/sys/bus/i2c/devices", error), end; !error && it != end;
         it.increment(error))
        if (identified(it->path()))
            return it->path();
    return {};
}
std::string bus_device(const fs::path &path) {
    std::error_code error;
    auto parent = fs::canonical(path, error).parent_path();
    while (!error && !parent.empty() && parent != parent.root_path()) {
        const auto name = parent.filename().string();
        if (name.starts_with("i2c-") && name.size() > 4 &&
            name.find_first_not_of("0123456789", 4) == std::string::npos)
            return "/dev/" + name;
        parent = parent.parent_path();
    }
    return {};
}
std::optional<std::uint16_t> device_address(const fs::path &path) {
    const auto name = path.filename().string();
    const auto dash = name.find('-');
    unsigned int address{};
    if (dash != std::string::npos && name.find_first_not_of("0123456789", 0) == dash) {
        const auto first = name.data() + dash + 1;
        const auto parsed = std::from_chars(first, name.data() + name.size(), address, 16);
        if (parsed.ec == std::errc{} && parsed.ptr == name.data() + name.size() && address >= 8 &&
            address <= 0x77)
            return static_cast<std::uint16_t>(address);
    }
    // Qualified from the installed TM2309 DSDT: NFC._CRS, I2C0, address 0x54.
    // An ACPI name alone does not reveal an arbitrary board's bus address.
    if (line("/sys/class/dmi/id/board_name") == "TM2309" &&
        line("/sys/class/dmi/id/bios_version") == "RMAMT6B0P0A0A")
        return 0x54;
    return {};
}
class LinuxTransport final : public Transport {
  public:
    ~LinuxTransport() override {
        if (file_ >= 0)
            close(file_);
    }
    Discovery discover() override;
    Status read(std::uint16_t block, std::span<std::uint8_t> result) override;
    Status write(std::uint16_t block, std::span<const std::uint8_t> bytes) override;
    Status read_register(std::uint16_t block, std::uint8_t index, std::uint8_t &value) override;
    bool has_events() const override {
        return false;
    }
    Status wait_event(std::stop_token) override {
        return Status::Unavailable;
    }

  private:
    int file_{-1};
    std::uint16_t address_{0x54};
    Status exchange(Bytes command, std::span<std::uint8_t> result);
    Status open_device(const fs::path &device);
    Status claim_bus(const std::string &bus);
};
Status LinuxTransport::claim_bus(const std::string &bus) {
    const int descriptor = open(bus.c_str(), O_RDWR | O_CLOEXEC);
    if (descriptor < 0)
        return native_error();
    if (flock(descriptor, LOCK_EX | LOCK_NB) < 0 || ioctl(descriptor, I2C_SLAVE, address_) < 0) {
        const auto result = native_error();
        close(descriptor);
        return result;
    }
    file_ = descriptor;
    return Status::Ready;
}
Status LinuxTransport::open_device(const fs::path &device) {
    std::error_code error;
    if (fs::exists(device / "driver", error))
        return Status::Busy;
    const auto address = device_address(device);
    if (!address)
        return Status::Incompatible;
    address_ = *address;
    const auto bus = bus_device(device);
    return bus.empty() ? Status::Unavailable : claim_bus(bus);
}
Discovery LinuxTransport::discover() {
    if (file_ >= 0)
        return {Status::Ready, true};
    const auto device = find_tag();
    if (!device)
        return {};
    return {open_device(*device), true};
}
Status LinuxTransport::exchange(Bytes command, std::span<std::uint8_t> result) {
    std::array<i2c_msg, 2> messages{{{address_, 0, static_cast<__u16>(command.size()), command.data()},
                                     {address_, I2C_M_RD, static_cast<__u16>(result.size()), result.data()}}};
    i2c_rdwr_ioctl_data request{messages.data(), static_cast<__u32>(messages.size())};
    const int count = ioctl(file_, I2C_RDWR, &request);
    return count == 2 ? Status::Ready : native_error();
}
Status LinuxTransport::read(std::uint16_t block, std::span<std::uint8_t> result) {
    return exchange({static_cast<std::uint8_t>(block >> 8U), static_cast<std::uint8_t>(block & 0xffU)},
                    result);
}
Status LinuxTransport::read_register(std::uint16_t block, std::uint8_t index, std::uint8_t &value) {
    return exchange({static_cast<std::uint8_t>(block >> 8U), static_cast<std::uint8_t>(block & 0xffU), index},
                    std::span(&value, 1));
}
Status LinuxTransport::write(std::uint16_t block, std::span<const std::uint8_t> bytes) {
    if (block >= 511 || bytes.size() != 4)
        return Status::InvalidData;
    std::array<std::uint8_t, 6> command{static_cast<std::uint8_t>(block >> 8U),
                                        static_cast<std::uint8_t>(block & 0xffU),
                                        bytes[0],
                                        bytes[1],
                                        bytes[2],
                                        bytes[3]};
    const auto count = ::write(file_, command.data(), command.size());
    return count == static_cast<ssize_t>(command.size()) ? Status::Ready : native_error();
}
} // namespace
} // namespace manager::adapters::nfc
namespace manager::adapters {
std::shared_ptr<application::nfc::TagPort> make_nfc_tag() {
    return std::make_shared<nfc::Ntag>(std::make_shared<nfc::LinuxTransport>());
}
} // namespace manager::adapters
