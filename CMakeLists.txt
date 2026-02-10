cmake_minimum_required(VERSION 3.16)

project(AMCS LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AMCS_BUILD_TESTS "Build AMCS tests" OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Concurrent)

set(EVERYTHING_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Everything)
set(QUAZIP_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDK/QuaZip)
set(ZLIB_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDK/ZLib)
if (WIN32)
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(EVERYTHING_DLL ${EVERYTHING_SDK_DIR}/dll/x64/Everything64.dll)
    set(EVERYTHING_LIB ${EVERYTHING_SDK_DIR}/lib/x64/Everything64.lib)
  else()
    set(EVERYTHING_DLL ${EVERYTHING_SDK_DIR}/dll/x86/Everything32.dll)
    set(EVERYTHING_LIB ${EVERYTHING_SDK_DIR}/lib/x86/Everything32.lib)
  endif()
  # Use local ZLib on Windows
  set(ZLIB_ROOT ${ZLIB_SDK_DIR})
  add_subdirectory(${ZLIB_SDK_DIR} ${CMAKE_BINARY_DIR}/ZLib EXCLUDE_FROM_ALL)
  set(ZLIB_LIBRARY zlibstatic)
  set(ZLIB_INCLUDE_DIR ${ZLIB_SDK_DIR})
endif()

add_library(amcs_core STATIC
  Core/AMCSCore.h
  Core/CoreSettings.h
  Core/CoreSettings.cpp
  Core/Auth/McAccount.h
  Core/Auth/McAccount.cpp
  Core/Auth/McAccountManager.h
  Core/Auth/McAccountManager.cpp
  Core/Api/McApi.h
  Core/Api/McApi.cpp
  Core/Manager/AccountManager.h
  Core/Manager/AccountManager.cpp
  Core/Manager/JavaManager.h
  Core/Manager/JavaManager.cpp
  Core/Manager/VersionManager.h
  Core/Manager/VersionManager.cpp
  Core/Searcher/JavaSearcher.h
  Core/Searcher/JavaSearcher.cpp
  Core/Launcher/LauncherCore.h
  Core/Launcher/LauncherCore.cpp
  Core/Launcher/LaunchOptions.h
  Core/Launcher/LoaderInterfaces.h
  Core/Download/AsulMultiDownloader.h
  Core/Download/AsulMultiDownloader.cpp
)

target_link_libraries(amcs_core
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Concurrent
)

if (WIN32)
  target_include_directories(amcs_core PRIVATE ${EVERYTHING_SDK_DIR}/include)
  if (EXISTS ${EVERYTHING_LIB})
    target_link_libraries(amcs_core ${EVERYTHING_LIB})
  endif()
endif()

if (TARGET Qt${QT_VERSION_MAJOR}::CorePrivate)
  target_link_libraries(amcs_core Qt${QT_VERSION_MAJOR}::CorePrivate)
endif()

add_subdirectory(${QUAZIP_SDK_DIR} EXCLUDE_FROM_ALL)
target_link_libraries(amcs_core QuaZip)


if (AMCS_BUILD_TESTS)
  add_subdirectory(Test)
  
  # Create a custom target to build all tests
  add_custom_target(tests
    DEPENDS
      amcs_test_core
      amcs_test_core_init
      amcs_test_launcher
      amcs_test_login_device
      amcs_test_install_latest
      amcs_test_launch_latest_offline
      amcs_test_launch_latest_microsoft
      amcs_test_manifest_fetch
      amcs_test_java_searcher
      amcs_test_manager_version
      amcs_test_manager_account
      amcs_test_quazip_pack_unpack
    COMMENT "Building all AMCS tests"
  )
endif()
