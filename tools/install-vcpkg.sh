#!/usr/bin/env bash

tools_dir="$( dirname -- "$0"; )"
vcpkg_dir=$tools_dir"/vcpkg"

if [[ ! -d "$vcpkg_dir" ]]; then
    echo "clone vcpkg from GitHub"
    git clone https://github.com/Microsoft/vcpkg.git $vcpkg_dir
fi

echo "install vcpkg"
$vcpkg_dir/bootstrap-vcpkg.sh -disableMetrics