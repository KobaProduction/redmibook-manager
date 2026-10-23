#pragma once
#include "application/power/scheme_port.hpp"
namespace manager::adapters {
std::shared_ptr<application::power::SchemePort> make_power_schemes();
}
