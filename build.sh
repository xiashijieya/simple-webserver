#!/usr/bin/env bash
set -e

if [ $# -ne 1 ]; then
    echo "usage: ./build.sh debug | release"
    exit 1
fi

case "$1" in
    debug)
        build_type=Debug
        build_dir=build_debug
        ;;
    release)
        build_type=RelWithDebInfo
        build_dir=build_release
        ;;
    *)
        echo "usage: ./build.sh debug | release"
        exit 1
        ;;
esac

cmake -S . -B "$build_dir" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE="$build_type"

echo ""
echo "configure done, next steps:"
echo "  cd $build_dir && make"
