#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: build.sh <git|release>"
    exit 1
fi

build_git() {
    GIT_HASH=$(git rev-parse --short HEAD)
    cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel -DPROJECT_VERSION_SUFFIX=${GIT_HASH} && cmake --build build
}

build_release() {
    cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build build
}
