cmake_minimum_required(VERSION 3.16)

project(AMCSTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

if (WIN32 AND DEFINED EVERYTHING_DLL)
  function(copy_everything_dll target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${EVERYTHING_DLL}
        $<TARGET_FILE_DIR:${target_name}>
    )
  endfunction()
endif()

add_executable(amcs_test_core
  test_core_sanity.cpp
)

target_link_libraries(amcs_test_core amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_core)
endif()

add_executable(amcs_test_core_init
  test_core_init.cpp
)

target_link_libraries(amcs_test_core_init amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_core_init)
endif()

add_executable(amcs_test_launcher
  test_launcher_sanity.cpp
)

target_link_libraries(amcs_test_launcher amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_launcher)
endif()

add_executable(amcs_test_login_device
  test_login_device.cpp
)

target_link_libraries(amcs_test_login_device amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_login_device)
endif()

add_executable(amcs_test_install_latest
  test_install_latest.cpp
)

target_link_libraries(amcs_test_install_latest amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_install_latest)
endif()

add_executable(amcs_test_launch_latest_offline
  test_launch_latest_offline.cpp
)

target_link_libraries(amcs_test_launch_latest_offline amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_launch_latest_offline)
endif()

add_executable(amcs_test_launch_latest_microsoft
  test_launch_latest_microsoft.cpp
)

target_link_libraries(amcs_test_launch_latest_microsoft amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_launch_latest_microsoft)
endif()

add_executable(amcs_test_manifest_fetch
  test_manifest_fetch.cpp
)

target_link_libraries(amcs_test_manifest_fetch amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_manifest_fetch)
endif()

add_executable(amcs_test_java_searcher
  test_java_searcher.cpp
)

target_link_libraries(amcs_test_java_searcher amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_java_searcher)
endif()

add_executable(amcs_test_manager_version
  test_manager_version.cpp
)

target_link_libraries(amcs_test_manager_version amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_manager_version)
endif()

add_executable(amcs_test_manager_account
  test_manager_account.cpp
)

target_link_libraries(amcs_test_manager_account amcs_core Qt${QT_VERSION_MAJOR}::Core)
if (WIN32 AND DEFINED EVERYTHING_DLL)
  copy_everything_dll(amcs_test_manager_account)
endif()
