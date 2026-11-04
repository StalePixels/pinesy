#!/bin/bash
#
# Build libmame_pinesy (.dylib on macOS, .so on Linux) with the machines
# listed in build-mame.conf.
#
# Usage: build-mame.sh [--qt] [extra make arguments]
#
# --qt (Linux only) builds libmame_pinesy_qt.so instead: the same engine
# with MAME's Qt debugger, for the KDE front end. It has its own build
# folder, mame/build-qt, so the two libraries never share objects. Both
# link the one QEMU build in mame/build/qemu.
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

qt=0
if [ "$1" = "--qt" ]; then
	qt=1
	shift
fi

# every object goes into a shared library, so on Linux all code is PIC
extra=()
case "$(uname -s)" in
	Darwin)
		if [ $qt = 1 ]; then
			echo "build-mame: --qt is for Linux only" >&2
			exit 1
		fi
		jobs=$(sysctl -n hw.ncpu)
		archopts=-march=native
		;;
	*)
		jobs=$(nproc)
		archopts="-march=native -fPIC"
		if [ $qt = 1 ]; then
			extra=(USE_QTDEBUG=1 BUILDDIR=build-qt)
		else
			# no Qt in the engine: the GTK front end must not load it
			extra=(USE_QTDEBUG=0)
		fi
		;;
esac

echo "build-mame: SOURCES=$sources"
cd "$mame"
exec make REGENIE=1 PRECOMPILE=0 -j"$jobs" PYTHON_EXECUTABLE=python3 TOOLS=0 OSD=pinesy \
	"OVERRIDE_CC=ccache clang" "OVERRIDE_CXX=ccache clang++" "ARCHOPTS=$archopts" \
	SOURCES="$sources" "${extra[@]}" "$@"
