cmake_minimum_required(VERSION 3.30)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" MANAGER_VERSION LIMIT_COUNT 1)
if(NOT MANAGER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION.txt must contain major.minor.patch; changes require owner approval")
endif()
project(RedmibookManager VERSION "${MANAGER_VERSION}" LANGUAGES CXX)
set(MANAGER_BUILD_ID "local" CACHE STRING "Payload identity; independent of product version")
if(NOT MANAGER_BUILD_ID MATCHES "^[A-Za-z0-9._-]+$")
    message(FATAL_ERROR "MANAGER_BUILD_ID must be a simple stable payload identifier")
endif()
set(MANAGER_RELEASE_CHANNEL "alpha" CACHE STRING "Public update channel: alpha or stable")
set(MANAGER_RELEASE_SEQUENCE "0" CACHE STRING "Alpha sequence; zero for local builds")
if(NOT MANAGER_RELEASE_CHANNEL MATCHES "^(alpha|stable)$" OR NOT MANAGER_RELEASE_SEQUENCE MATCHES "^[0-9]+$")
    message(FATAL_ERROR "Invalid release channel or sequence")
endif()
add_compile_definitions(MANAGER_RELEASE_CHANNEL="${MANAGER_RELEASE_CHANNEL}" MANAGER_RELEASE_SEQUENCE=${MANAGER_RELEASE_SEQUENCE})
add_compile_definitions(MANAGER_APP_VERSION="${PROJECT_VERSION}" MANAGER_BUILD_ID="${MANAGER_BUILD_ID}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_LIBDIR lib)
include(CTest)
option(MANAGER_BUILD_DESKTOP "Build the Qt desktop application and its tooling" ON)

add_library(project_options INTERFACE)
if(MSVC)
    target_compile_options(project_options INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
endif()

add_library(manager_domain INTERFACE)
target_include_directories(manager_domain INTERFACE "${PROJECT_SOURCE_DIR}")
add_library(manager_application STATIC application/overview/read_overview.cpp)
target_sources(manager_application PRIVATE application/updates/release_policy.cpp application/updates/release_identity.cpp)
target_link_libraries(manager_application PUBLIC manager_domain PRIVATE project_options)
set_target_properties(manager_application PROPERTIES AUTOMOC OFF)

if(WIN32)
    set(MANAGER_PLATFORM windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MANAGER_PLATFORM linux)
else()
    message(FATAL_ERROR "Supported platforms: Windows and Linux")
endif()
add_library(manager_platform STATIC "adapters/${MANAGER_PLATFORM}/host_info.cpp")
target_link_libraries(manager_platform PUBLIC manager_application PRIVATE project_options)
set_target_properties(manager_platform PROPERTIES AUTOMOC OFF)

add_library(manager_device_profile STATIC adapters/device_profile/smbios.cpp
    "adapters/${MANAGER_PLATFORM}/device_profile/native_profile.cpp")
target_link_libraries(manager_device_profile PUBLIC manager_application PRIVATE project_options)
set_target_properties(manager_device_profile PROPERTIES AUTOMOC OFF)
add_library(manager_vendor_decoder STATIC adapters/windows/shortcuts/vendor_decoder.cpp)
target_link_libraries(manager_vendor_decoder PUBLIC manager_application PRIVATE project_options)
set_target_properties(manager_vendor_decoder PROPERTIES AUTOMOC OFF)
add_library(manager_vendor_input STATIC "adapters/${MANAGER_PLATFORM}/shortcuts/vendor_input.cpp")
find_package(Threads REQUIRED)
target_link_libraries(manager_vendor_input PUBLIC manager_vendor_decoder PRIVATE project_options Threads::Threads)
set_target_properties(manager_vendor_input PROPERTIES AUTOMOC OFF)
if(WIN32)
    target_compile_definitions(manager_vendor_input PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(manager_vendor_input PRIVATE ole32 oleaut32 wbemuuid)
endif()
add_library(manager_device_policy STATIC application/battery/charge_control.cpp application/performance/performance_control.cpp)
target_link_libraries(manager_device_policy PUBLIC manager_application PRIVATE project_options)
set_target_properties(manager_device_policy PROPERTIES AUTOMOC OFF)
add_library(manager_vendor_protocol STATIC adapters/windows/firmware/mifs_protocol.cpp
    adapters/windows/battery/charge_control.cpp adapters/windows/performance/performance_control.cpp)
target_link_libraries(manager_vendor_protocol PUBLIC manager_device_policy PRIVATE project_options)
set_target_properties(manager_vendor_protocol PROPERTIES AUTOMOC OFF)
if(WIN32)
    target_sources(manager_vendor_protocol PRIVATE adapters/windows/firmware/native_wmi_transport.cpp)
    target_link_libraries(manager_vendor_protocol PRIVATE ole32 oleaut32 wbemuuid)
endif()
if(WIN32)
    add_library(manager_session_controls STATIC adapters/windows/microphone.cpp adapters/windows/display_refresh.cpp)
else()
    add_library(manager_session_controls STATIC adapters/linux/session_controls.cpp)
endif()
target_sources(manager_session_controls PRIVATE "adapters/${MANAGER_PLATFORM}/microphone_watch.cpp")
target_link_libraries(manager_session_controls PUBLIC manager_application PRIVATE project_options Threads::Threads)
set_target_properties(manager_session_controls PROPERTIES AUTOMOC OFF)
if(WIN32)
    target_compile_definitions(manager_session_controls PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(manager_device_profile PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(manager_session_controls PRIVATE ole32 uuid user32)
endif()

add_library(manager_runtime STATIC "adapters/${MANAGER_PLATFORM}/runtime_client.cpp" "adapters/${MANAGER_PLATFORM}/window_appearance.cpp")
target_sources(manager_runtime PRIVATE contracts/local_api/firmware_protocol.cpp)
if(WIN32)
    target_sources(manager_runtime PRIVATE adapters/windows/hosting/firmware_client.cpp adapters/windows/hosting/firmware_server.cpp)
    target_link_libraries(manager_runtime PRIVATE manager_vendor_protocol manager_device_profile)
else()
    target_sources(manager_runtime PRIVATE adapters/linux/firmware_controls.cpp)
endif()
target_link_libraries(manager_runtime PUBLIC manager_application PRIVATE project_options)
set_target_properties(manager_runtime PROPERTIES AUTOMOC OFF)
if(WIN32)
    target_compile_definitions(manager_runtime PRIVATE _WIN32_WINNT=0x0602 WIN32_LEAN_AND_MEAN NOMINMAX)
    target_sources(manager_runtime PRIVATE
        adapters/windows/hosting/client_pipe.cpp
        adapters/windows/hosting/payload_manifest.cpp
        adapters/windows/hosting/pipe_server.cpp adapters/windows/hosting/service_host.cpp
        adapters/windows/hosting/service_admin.cpp adapters/windows/hosting/maintenance.cpp)
    target_link_libraries(manager_runtime PRIVATE advapi32 shell32 rstrtmgr user32 dwmapi)
endif()
add_executable(redmibook-service apps/machine/main.cpp)
target_link_libraries(redmibook-service PRIVATE manager_runtime project_options)
if(MINGW)
    # The installer copies this native maintenance host to its private temporary
    # directory; it must not depend on the payload's replaceable MinGW DLLs.
    target_link_options(redmibook-service PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(BUILD_TESTING)
    add_executable(core-firmware-protocol-tests tests/core_firmware_protocol_tests.cpp)
    target_link_libraries(core-firmware-protocol-tests PRIVATE manager_runtime project_options)
    add_test(NAME core-firmware-protocol-contract COMMAND core-firmware-protocol-tests)
    add_executable(vendor-decoder-tests tests/vendor_decoder_tests.cpp)
    target_link_libraries(vendor-decoder-tests PRIVATE manager_vendor_decoder project_options)
    add_test(NAME vendor-key-decoder-contract COMMAND vendor-decoder-tests)
    add_executable(display-cycle-tests tests/display_cycle_tests.cpp)
    target_link_libraries(display-cycle-tests PRIVATE manager_application project_options)
    add_test(NAME internal-panel-cycle-contract COMMAND display-cycle-tests)
    add_executable(device-profile-tests tests/device_profile_tests.cpp)
    target_link_libraries(device-profile-tests PRIVATE manager_device_profile project_options)
    add_test(NAME device-profile-contract COMMAND device-profile-tests)
    add_executable(microphone-tests tests/microphone_tests.cpp)
    target_link_libraries(microphone-tests PRIVATE manager_application project_options)
    add_test(NAME microphone-toggle-contract COMMAND microphone-tests)
    add_executable(battery-tests tests/battery_tests.cpp)
    target_link_libraries(battery-tests PRIVATE manager_vendor_protocol project_options)
    add_test(NAME vendor-policy-contract COMMAND battery-tests)
    add_executable(overview-tests tests/overview_tests.cpp)
    if(WIN32)
        add_executable(payload-tests tests/payload_tests.cpp)
        target_link_libraries(payload-tests PRIVATE manager_runtime project_options)
        add_test(NAME payload-update-contract COMMAND payload-tests)
    endif()
    target_link_libraries(overview-tests PRIVATE manager_application project_options)
    add_test(NAME overview-contract COMMAND overview-tests)
    add_executable(platform-tests tests/platform_tests.cpp)
    target_link_libraries(platform-tests PRIVATE manager_platform project_options)
    add_test(NAME native-platform-contract COMMAND platform-tests "${MANAGER_PLATFORM}")
endif()

if(NOT MANAGER_BUILD_DESKTOP)
    return()
endif()
find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Core Gui Widgets Svg Network Concurrent Qml Quick QuickControls2 LinguistTools)
qt_standard_project_setup(REQUIRES 6.10 I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES ru)
add_executable(redmibook-device apps/device/main.cpp apps/device/vendor_events.cpp "apps/device/firmware_${MANAGER_PLATFORM}.cpp")
if(WIN32)
    target_sources(redmibook-device PRIVATE apps/device/qualification.cpp)
    target_sources(redmibook-device PRIVATE adapters/windows/shortcuts/vendor_events.cpp)
endif()
target_link_libraries(redmibook-device PRIVATE manager_device_profile manager_session_controls manager_vendor_protocol manager_runtime Qt6::Core project_options)

qt_add_library(manager_input STATIC "adapters/${MANAGER_PLATFORM}/shortcuts/input_binding.cpp")
target_link_libraries(manager_input PUBLIC manager_application Qt6::Core PRIVATE project_options)
if(WIN32)
    target_compile_definitions(manager_input PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(manager_input PRIVATE user32)
endif()

qt_add_library(manager_preferences STATIC
    adapters/storage/ui_preferences/store.cpp presentation/common/preferences.cpp
    presentation/common/preferences.hpp presentation/common/appearance/theme.cpp
    presentation/common/appearance/theme.hpp presentation/common/appearance/widget_controls.cpp
    presentation/common/appearance/widget_controls.hpp presentation/common/runtime_monitor.cpp
    presentation/common/runtime_monitor.hpp presentation/common/displays.cpp presentation/common/displays.hpp presentation/common/rounded_frame.cpp presentation/common/rounded_frame.hpp)
target_link_libraries(manager_preferences PUBLIC manager_application Qt6::Widgets Qt6::Qml PRIVATE project_options)
qt_add_library(manager_updates STATIC adapters/updates/release_feed.cpp adapters/updates/github_client.cpp adapters/updates/history.cpp adapters/updates/target.cpp)
target_link_libraries(manager_updates PUBLIC manager_application Qt6::Network PRIVATE project_options)
target_compile_definitions(manager_updates PRIVATE MANAGER_TARGET_OS="${MANAGER_PLATFORM}")

qt_add_library(manager_session_api STATIC
    adapters/local_api/device_state_codec.cpp adapters/local_api/device_state_codec.hpp
    adapters/local_api/device_state_stream.cpp adapters/local_api/device_state_stream.hpp
    adapters/local_api/session_client.cpp adapters/local_api/session_client.hpp
    adapters/local_api/session_server.cpp adapters/local_api/session_server.hpp
    adapters/local_api/session_codec.cpp adapters/local_api/session_codec.hpp)
target_link_libraries(manager_session_api PUBLIC manager_runtime Qt6::Network Qt6::Concurrent Qt6::Gui PRIVATE project_options)

qt_add_library(manager_session_ui STATIC presentation/osd/diagnostic_osd.cpp presentation/osd/diagnostic_osd.hpp
    presentation/osd/vendor_osd.cpp presentation/osd/vendor_osd.hpp
    presentation/osd/microphone_osd.cpp presentation/osd/microphone_osd.hpp
    presentation/updates/update_notice.cpp presentation/updates/update_notice.hpp
    presentation/osd/placement.cpp
    presentation/tray/dashboard.cpp presentation/tray/dashboard.hpp
    presentation/tray/tray_client.cpp presentation/tray/tray_client.hpp)
target_link_libraries(manager_session_ui PUBLIC manager_preferences PRIVATE project_options Qt6::Svg)

qt_add_executable(redmibook-session apps/session/main.cpp apps/session/update_scheduler.cpp apps/session/input_actions.cpp apps/session/vendor_actions.cpp apps/session/device_state_publisher.cpp)
target_link_libraries(redmibook-session PRIVATE manager_session_api manager_session_ui manager_updates manager_input manager_session_controls manager_device_profile manager_vendor_input project_options)
qt_add_executable(redmibook-tray apps/tray/main.cpp apps/tray/tray_operations.cpp apps/tray/device_controls.cpp)
target_link_libraries(redmibook-tray PRIVATE manager_session_api manager_session_ui manager_session_controls manager_device_policy Qt6::Concurrent project_options)
set_target_properties(redmibook-session redmibook-tray PROPERTIES WIN32_EXECUTABLE TRUE)

qt_add_library(manager_presentation STATIC)
qt_add_qml_module(manager_presentation
    URI Redmibook.Manager
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/Redmibook/Manager"
    RESOURCE_PREFIX /qt/qml
    QML_FILES presentation/qml/Main.qml presentation/qml/pages/OverviewPage.qml
        presentation/qml/MonitorPicker.qml presentation/qml/Surface.qml presentation/qml/AppPalette.qml presentation/qml/LicenseDialog.qml presentation/qml/pages/AboutPage.qml
        presentation/qml/pages/SettingsPage.qml presentation/qml/pages/UnsupportedPage.qml presentation/qml/pages/HardwarePage.qml
        presentation/qml/pages/BatteryPage.qml presentation/qml/pages/MiKeyPage.qml
    SOURCES presentation/about_model.cpp presentation/about_model.hpp presentation/overview_model.hpp presentation/overview_model.cpp
        presentation/manager_model.hpp presentation/manager_model.cpp
        presentation/updates/update_model.hpp presentation/updates/update_model.cpp
        presentation/device_controls_model.hpp presentation/device_controls_model.cpp
        presentation/device_controls_firmware.cpp
)
target_include_directories(manager_presentation PUBLIC "${PROJECT_SOURCE_DIR}")
target_include_directories(manager_presentation PRIVATE "${PROJECT_SOURCE_DIR}/presentation" "${PROJECT_SOURCE_DIR}/presentation/updates")
target_link_libraries(manager_presentation PUBLIC manager_application manager_preferences Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
    PRIVATE project_options manager_device_policy)

set_property(TARGET manager_presentation APPEND PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION
    "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/packaging/*")
qt_add_resources(manager_presentation "license_texts" PREFIX "/licenses" BASE "${PROJECT_SOURCE_DIR}"
    FILES LICENSE packaging/licenses/Qt/GPL-3.0-only.txt packaging/licenses/Qt/LGPL-3.0-only.txt packaging/THIRD_PARTY_NOTICES.md)

qt_add_executable(redmibook-manager apps/desktop/main.cpp apps/desktop/smoke_test.cpp
    apps/desktop/style_setup.cpp apps/desktop/appearance_setup.cpp apps/desktop/popup_smoke.cpp
    apps/desktop/manager_host.cpp apps/desktop/manager_window.cpp apps/desktop/manager_window.hpp
    apps/desktop/ui_contract_smoke.cpp)
target_link_libraries(redmibook-manager PRIVATE manager_platform manager_device_profile manager_session_controls manager_session_api manager_presentation manager_presentationplugin manager_updates project_options)
target_compile_definitions(redmibook-manager PRIVATE MANAGER_APP_VERSION="${PROJECT_VERSION}")
set_target_properties(redmibook-manager PROPERTIES WIN32_EXECUTABLE TRUE)
foreach(gui_host IN ITEMS redmibook-manager redmibook-session redmibook-tray)
    qt_add_resources(${gui_host} "${gui_host}_icon"
        PREFIX "/icons" BASE "${PROJECT_SOURCE_DIR}/presentation/assets"
        FILES presentation/assets/app-icon.svg presentation/assets/app-icon.ico
            presentation/assets/microphone-active.svg presentation/assets/microphone-muted.svg)
    set_property(TARGET ${gui_host} APPEND PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION
        "${PROJECT_SOURCE_DIR}/presentation/assets/*")
endforeach()
if(WIN32)
    enable_language(RC)
    configure_file(packaging/windows/application-icon.rc.in
        "${CMAKE_BINARY_DIR}/application-icon.rc" @ONLY)
    foreach(gui_host IN ITEMS redmibook-manager redmibook-session redmibook-tray)
        target_sources(${gui_host} PRIVATE "${CMAKE_BINARY_DIR}/application-icon.rc")
    endforeach()
endif()

qt_import_qml_plugins(redmibook-manager)

if(BUILD_TESTING)
    qt_add_executable(preferences-tests tests/preferences_tests.cpp tests/preferences_sync.cpp)
    qt_add_executable(device-controls-async-tests tests/device_controls_async_tests.cpp)
    target_link_libraries(device-controls-async-tests PRIVATE manager_presentation project_options)
    add_test(NAME device-controls-async-contract COMMAND device-controls-async-tests)
    set_tests_properties(device-controls-async-contract PROPERTIES TIMEOUT 5 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(tray-tests tests/tray_tests.cpp)
    qt_add_executable(ui-details-tests tests/ui_details_tests.cpp)
    qt_add_executable(system-theme-probe tests/system_theme_probe.cpp)
    target_link_libraries(system-theme-probe PRIVATE manager_presentation manager_presentationplugin project_options)
    target_link_libraries(ui-details-tests PRIVATE manager_presentation manager_session_ui project_options)
    add_test(NAME ui-details-contract COMMAND ui-details-tests)
    set_tests_properties(ui-details-contract PROPERTIES TIMEOUT 10 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(updates-tests tests/updates_tests.cpp)
    target_link_libraries(updates-tests PRIVATE manager_updates project_options)
    add_test(NAME update-policy-contract COMMAND updates-tests)
    qt_add_executable(displays-tests tests/displays_tests.cpp)
    target_link_libraries(displays-tests PRIVATE manager_session_ui project_options)
    add_test(NAME display-placement-contract COMMAND displays-tests)
    set_tests_properties(display-placement-contract PROPERTIES TIMEOUT 10 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(runtime-monitor-tests tests/runtime_monitor_tests.cpp)
    qt_add_executable(session-codec-tests tests/session_codec_tests.cpp)
    target_link_libraries(session-codec-tests PRIVATE manager_session_api project_options)
    add_test(NAME session-codec-contract COMMAND session-codec-tests)
    qt_add_executable(session-concurrency-tests tests/session_concurrency_tests.cpp)
    target_link_libraries(session-concurrency-tests PRIVATE manager_session_api project_options)
    add_test(NAME session-concurrency-contract COMMAND session-concurrency-tests)
    set_tests_properties(session-concurrency-contract PROPERTIES TIMEOUT 5)
    target_link_libraries(runtime-monitor-tests PRIVATE manager_preferences project_options)
    add_test(NAME runtime-health-contract COMMAND runtime-monitor-tests)
    set_tests_properties(runtime-health-contract PROPERTIES TIMEOUT 12)
    target_link_libraries(tray-tests PRIVATE manager_session_ui project_options)
    add_test(NAME tray-osd-contract COMMAND tray-tests)
    target_link_libraries(preferences-tests PRIVATE manager_preferences manager_session_ui project_options)
    set(translation_test_target preferences-tests ui-details-tests)
    add_test(NAME ui-preferences-contract COMMAND preferences-tests)
    add_test(NAME ui-live-contract COMMAND redmibook-manager --ui-contract)
    set_tests_properties(ui-preferences-contract ui-live-contract tray-osd-contract PROPERTIES TIMEOUT 15
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    if(WIN32)
        set_property(TEST ui-preferences-contract ui-live-contract tray-osd-contract ui-details-contract
            APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{SystemRoot}/Fonts")
    endif()
endif()

qt_add_translations(TARGETS redmibook-manager redmibook-session redmibook-tray ${translation_test_target}
    SOURCE_TARGETS manager_presentation manager_preferences manager_session_ui redmibook-tray redmibook-manager
    TS_FILES presentation/i18n/manager_ru.ts
    PLURALS_TS_FILE presentation/i18n/manager_en.ts
    RESOURCE_PREFIX "/translations" MERGE_QT_TRANSLATIONS)

if(BUILD_TESTING)
    add_test(NAME desktop-smoke COMMAND redmibook-manager --smoke-test)
    set_tests_properties(desktop-smoke PROPERTIES TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

include(cmake/Quality.cmake)
include(cmake/Packaging.cmake)
