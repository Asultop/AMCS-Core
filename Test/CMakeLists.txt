cmake_minimum_required(VERSION 3.16)

project(AMCSTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(amcs_test_core
  test_core_sanity.cpp
)

target_link_libraries(amcs_test_core amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_core_init
  test_core_init.cpp
)

target_link_libraries(amcs_test_core_init amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_launcher
  test_launcher_sanity.cpp
)

target_link_libraries(amcs_test_launcher amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_login_device
  test_login_device.cpp
)

target_link_libraries(amcs_test_login_device amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_install_latest
  test_install_latest.cpp
)

target_link_libraries(amcs_test_install_latest amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_launch_latest_offline
  test_launch_latest_offline.cpp
)

target_link_libraries(amcs_test_launch_latest_offline amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_launch_latest_microsoft
  test_launch_latest_microsoft.cpp
)

target_link_libraries(amcs_test_launch_latest_microsoft amcs_core Qt${QT_VERSION_MAJOR}::Core)

add_executable(amcs_test_manifest_fetch
  test_manifest_fetch.cpp
)

target_link_libraries(amcs_test_manifest_fetch amcs_core Qt${QT_VERSION_MAJOR}::Core)
