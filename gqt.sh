#!/bin/bash
export CMAKE_PREFIX_PATH="/Users/asultop/Qt/6.7.3/macos/lib/cmake"
QT_PATH="/Users/AsulTop/Qt/6.7.3/macos"
BUILD_TYPE="Release"
GENERATOR="Ninja"

mkdir -p build && cd build

cmake \
  -DCMAKE_PREFIX_PATH=${QT_PATH} \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -G "${GENERATOR}" \
  ..

if [ $? -eq 0 ]; then
  echo "CMake配置成功，开始构建..."
  ninja
else
  echo "CMake配置失败！"
  exit 1
fi