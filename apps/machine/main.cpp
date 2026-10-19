#include "adapters/platform/runtime.hpp"

int main(int argc, char *argv[]) {
    return manager::adapters::run_machine_host(argc, argv);
}
