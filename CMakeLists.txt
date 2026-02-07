cmake_minimum_required(VERSION 3.16)

project(AMCS LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AMCS_BUILD_TESTS "Build AMCS tests" ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Concurrent)

add_library(amcs_core
  Core/AMCSCore.h
  Core/CoreSettings.h
  Core/CoreSettings.cpp
  Core/Auth/McAccount.h
  Core/Auth/McAccount.cpp
  Core/Auth/McAccountManager.h
  Core/Auth/McAccountManager.cpp
  Core/Api/McApi.h
  Core/Api/McApi.cpp
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

if (TARGET Qt${QT_VERSION_MAJOR}::CorePrivate)
  target_link_libraries(amcs_core Qt${QT_VERSION_MAJOR}::CorePrivate)
endif()

add_executable(installer
  Installer/main.cpp
)

target_link_libraries(installer amcs_core)

if (AMCS_BUILD_TESTS)
  add_subdirectory(Test)
endif()
