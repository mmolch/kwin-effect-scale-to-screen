#!/usr/bin/env bash

readonly PROJECT_DIR=$(cd "$(dirname "${0}")/.." && pwd)
readonly SONAME=ScaleToScreen.so
readonly INSTALL_DIR='/usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/'

sudo ln -sf "${PROJECT_DIR}/build/${SONAME}" "${INSTALL_DIR}"

echo "Link installed in ${INSTALL_DIR}"
ls -l "${INSTALL_DIR}/${SONAME}"
