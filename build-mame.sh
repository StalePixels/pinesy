#!/bin/bash
#
# Build libmame_pinesy.dylib with the machines listed in build-mame.conf.
#
# Usage: build-mame.sh [extra make arguments]
# bash 3.2 and BSD tools.

set -e

here=$(cd "$(dirname "$0")" && pwd)
mame=$here/mame
conf=$here/build-mame.conf

if [ ! -f "$mame/makefile" ]; then
	echo "build-mame: $mame is empty; run: git submodule update --init mame" >&2
	exit 1
fi

. "$conf"
if [ -z "$SOURCES" ]; then
	echo "build-mame: SOURCES is empty in $conf" >&2
	exit 1
fi
sources=$SOURCES

jobs=$(sysctl -n hw.ncpu)

echo "build-mame: SOURCES=$sources"
cd "$mame"
exec make REGENIE=1 PRECOMPILE=0 -j"$jobs" PYTHON_EXECUTABLE=python3 TOOLS=0 OSD=pinesy \
	"OVERRIDE_CC=ccache clang" "OVERRIDE_CXX=ccache clang++" ARCHOPTS=-march=native \
	SOURCES="$sources" "$@"
