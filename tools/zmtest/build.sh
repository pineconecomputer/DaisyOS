#!/bin/sh
# Builds the native Z-machine test harness. Run from the project root.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${ROOT}/tools/zmtest/zmtest"
c++ -std=c++11 -g -O1 -DZM_HOST_BUILD \
    -I"${ROOT}/include" \
    -Wall -Wno-unused-parameter -Wno-write-strings -Wno-unused-but-set-variable \
    "${ROOT}"/src/zmachine/*.cpp "${ROOT}"/tools/zmtest/harness.cpp \
    -o "${OUT}"
echo "built ${OUT}"
