#!/bin/sh
# Builds bootpd, runs the tests and installs into ./workspace, see README.md
set -e
conan install . --build=missing -s compiler.cppstd=20
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
cmake --install build/Release --prefix workspace
