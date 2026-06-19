#!/usr/bin/env bash

set -e

BUILD_TYPE="Debug"

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d) BUILD_TYPE="Debug" ;;
        -r) BUILD_TYPE="Release" ;;
    esac
    shift
done

echo "==> Building $BUILD_TYPE ..."

cmake --build build/$BUILD_TYPE --config $BUILD_TYPE

echo "==> Build succeeded"