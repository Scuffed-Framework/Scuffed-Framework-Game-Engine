#!/usr/bin/env bash

set -e

# Parse args
BUILD_TYPE="Debug"

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d) BUILD_TYPE="Debug" ;;
        -r) BUILD_TYPE="Release" ;;
    esac
    shift
done

echo "==> Build type: $BUILD_TYPE"

echo "==> Running Conan..."
conan install . -s build_type=$BUILD_TYPE --build=missing -of=build/$BUILD_TYPE

echo "==> Running CMake configure..."
cmake -B build/$BUILD_TYPE -DCMAKE_BUILD_TYPE=$BUILD_TYPE -S .

echo "==> Configure complete"