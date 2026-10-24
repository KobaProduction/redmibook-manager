find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
find_program(CLANG_FORMAT NAMES clang-format-19 clang-format REQUIRED)
find_program(CLANG_TIDY NAMES clang-tidy-19 clang-tidy REQUIRED)
foreach(tool IN ITEMS CLANG_FORMAT CLANG_TIDY)
    execute_process(COMMAND "${${tool}}" --version OUTPUT_VARIABLE tool_version
        COMMAND_ERROR_IS_FATAL ANY)
    if(NOT tool_version MATCHES "version 19\\.")
        message(FATAL_ERROR "${tool} must be LLVM 19, got: ${tool_version}")
    endif()
endforeach()

file(GLOB_RECURSE MANAGER_CPP CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/contracts/*.hpp" "${PROJECT_SOURCE_DIR}/contracts/*.cpp"
    "${PROJECT_SOURCE_DIR}/domain/*.hpp"
    "${PROJECT_SOURCE_DIR}/application/*.hpp" "${PROJECT_SOURCE_DIR}/application/*.cpp"
    "${PROJECT_SOURCE_DIR}/adapters/*.hpp" "${PROJECT_SOURCE_DIR}/adapters/*.cpp"
    "${PROJECT_SOURCE_DIR}/presentation/*.hpp" "${PROJECT_SOURCE_DIR}/presentation/*.cpp"
    "${PROJECT_SOURCE_DIR}/apps/*.hpp" "${PROJECT_SOURCE_DIR}/apps/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.cpp")
set(MANAGER_TIDY_SOURCES ${MANAGER_CPP})
list(FILTER MANAGER_TIDY_SOURCES INCLUDE REGEX "\\.cpp$")
if(WIN32)
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/adapters/linux/")
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/apps/device/firmware_linux.cpp$")
else()
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/adapters/windows/")
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/apps/device/firmware_windows.cpp$")
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/apps/device/qualification.cpp$")
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/tests/payload_tests.cpp$")
    # This WMI contract target is assembled only on Windows, like payload-tests.
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/tests/firmware_safety_tests.cpp$")
endif()
add_custom_target(format-check
    COMMAND "${CMAKE_COMMAND}" -DMODE=format-check -P "${CMAKE_BINARY_DIR}/RunQuality.cmake")
add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -DMODE=format -P "${CMAKE_BINARY_DIR}/RunQuality.cmake")
set(lint_dependencies redmibook-manager)
if(BUILD_TESTING)
    list(APPEND lint_dependencies overview-tests platform-tests)
else()
    list(FILTER MANAGER_TIDY_SOURCES EXCLUDE REGEX "/tests/")
endif()
configure_file("${PROJECT_SOURCE_DIR}/cmake/RunQuality.cmake.in"
    "${CMAKE_BINARY_DIR}/RunQuality.cmake" @ONLY)
add_custom_target(lint
    COMMAND "${CMAKE_COMMAND}" -DMODE=lint -P "${CMAKE_BINARY_DIR}/RunQuality.cmake"
    DEPENDS ${lint_dependencies})
add_custom_target(boundaries
    COMMAND "${Python3_EXECUTABLE}" tools/vendor/chromium_checkdeps/checkdeps.py --root .
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
# Reuse Qt's generated import/resource arguments; fail on every QML warning.
add_custom_target(qml-strict
    COMMAND "$<TARGET_FILE:Qt6::qmllint>" --max-warnings 0
        "@${CMAKE_BINARY_DIR}/.rcc/qmllint/manager_presentation.rsp"
    DEPENDS redmibook-manager VERBATIM)
add_custom_target(quality DEPENDS format-check lint boundaries qml-strict)
if(BUILD_TESTING)
    add_test(NAME architecture-boundaries
        COMMAND "${Python3_EXECUTABLE}" tools/vendor/chromium_checkdeps/checkdeps.py --root .)
    set_tests_properties(architecture-boundaries PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    add_test(NAME boundary-regressions COMMAND "${Python3_EXECUTABLE}" tests/boundary_tests.py)
    set_tests_properties(boundary-regressions PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()
