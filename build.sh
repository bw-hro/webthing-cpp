#!/usr/bin/env bash

script_path=$(realpath "$0")
base_dir="$( dirname "$script_path" )"
build_dir="$base_dir/build"
toolchain_file="$base_dir/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"

if [[ "${@#clean}" = "$@" ]]
then
    echo "use cache for project build"
else
    echo "clean project - remove $build_dir"
    rm -rf $build_dir
fi

if [[ "${@#release}" = "$@" ]]
then
    build_type="Debug"
    code_coverage="ON"
else
    build_type="Release"
    code_coverage="OFF"
    rm -rf $build_dir
fi
echo "project build type: $build_type"
echo "project code coverage: $code_coverage"

if [[ "${@#with_ssl}" = "$@" ]]
then
    ssl_support="OFF"
    vcpkg_file=vcpkg-no-ssl.json
else
    ssl_support="ON"
    vcpkg_file=vcpkg-with-ssl.json
fi
echo "project SSL support: $ssl_support"
cp $vcpkg_file vcpkg.json


cmake -B build -S . -D"WT_WITH_SSL=$ssl_support" -D"CMAKE_BUILD_TYPE=$build_type" -D"WT_ENABLE_COVERAGE=$code_coverage" -D"CMAKE_TOOLCHAIN_FILE=$toolchain_file" -D"CMAKE_MAKE_PROGRAM:PATH=make" -D"CMAKE_CXX_COMPILER=g++"
cmake --build build --parallel $(nproc)

ctest --test-dir build/test/