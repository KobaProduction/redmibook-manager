#include "adapters/platform/power_controls.hpp"
#include <iostream>
#include <string_view>

namespace {
using manager::application::display::Brightness;
using manager::application::display::BrightnessPort;
using manager::application::display::BrightnessStatus;

int round_trip(BrightnessPort &port, const Brightness &before) {
    if (!before.percent)
        return 1;
    const auto requested = *before.percent == 0 ? 1U : *before.percent - 1U;
    const auto changed = port.set(requested);
    const auto restored = port.set(*before.percent);
    std::cout << "before=" << *before.percent << " requested=" << requested
              << " changed-status=" << static_cast<int>(changed.status)
              << " restored-status=" << static_cast<int>(restored.status) << '\n';
    return changed.status == BrightnessStatus::Ready && changed.percent == requested &&
                   restored.status == BrightnessStatus::Ready && restored.percent == before.percent
               ? 0
               : 1;
}

int saver_round_trip() {
    using manager::application::power::SaverStatus;
    const auto port = manager::adapters::make_energy_saver();
    const auto before = port->read();
    if (!before.controllable || !before.enabled.has_value())
        return 1;
    const bool target = !*before.enabled;
    const auto changed = port->set(target);
    const auto restored = port->set(*before.enabled);
    std::cout << "saver-before=" << *before.enabled << " requested=" << target
              << " changed-status=" << static_cast<int>(changed.status)
              << " changed-state=" << changed.enabled.value_or(!target)
              << " restored-status=" << static_cast<int>(restored.status)
              << " restored-state=" << restored.enabled.value_or(target) << '\n';
    return changed.status == SaverStatus::Ready && changed.enabled == target &&
                   restored.status == SaverStatus::Ready && restored.enabled == before.enabled
               ? 0
               : 1;
}

void print_state(const Brightness &brightness) {
    std::cout << "brightness-status=" << static_cast<int>(brightness.status) << " percent=";
    if (brightness.percent)
        std::cout << *brightness.percent;
    else
        std::cout << "unknown";
    const auto saver = manager::adapters::make_energy_saver()->reconcile();
    std::cout << " energy-saver=";
    if (saver.enabled)
        std::cout << (*saver.enabled ? "on" : "off");
    else
        std::cout << "unknown";
    std::cout << " controllable=" << saver.controllable << '\n';
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--saver-round-trip")
        return saver_round_trip();
    const auto port = manager::adapters::make_brightness();
    const auto before = port->read();
    if (argc == 2 && std::string_view{argv[1]} == "--round-trip")
        return round_trip(*port, before);
    print_state(before);
    return before.status == BrightnessStatus::Ready ? 0 : 1;
}
