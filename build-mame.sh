#!/bin/bash
#
# Build libmame_pinesy (.dylib on macOS, .so on Linux) with the machines
# listed in build-mame.conf.
#
# Usage: build-mame.sh [--qt | --gtk] [extra make arguments]
#
# --qt (Linux only) builds libmame_pinesy_qt.so instead: the same engine
# with MAME's Qt debugger, for the KDE front end. It has its own build
# folder, mame/build-qt.
#
# --gtk (Linux only) builds libmame_pinesy_gtk.so instead: the same
# engine with the GTK debugger from debuggers/gtk, for the GNOME front
# end. It has its own build folder, mame/build-gtk. pinesy.lua chooses
# this build from PINESY_DEBUG_GTK=1 in the environment.
#
# The libraries never share objects, because GENie does not recompile
# when only a define changes. All of them link the one QEMU build in
# mame/build/qemu.
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
gtk=0
if [ "$1" = "--qt" ]; then
	qt=1
	shift
elif [ "$1" = "--gtk" ]; then
	gtk=1
	shift
fi
unset PINESY_DEBUG_GTK

# every object goes into a shared library, so on Linux all code is PIC
extra=()
case "$(uname -s)" in
	Darwin)
		if [ $qt = 1 ] || [ $gtk = 1 ]; then
			echo "build-mame: --qt and --gtk are for Linux only" >&2
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
		elif [ $gtk = 1 ]; then
			extra=(USE_QTDEBUG=0 BUILDDIR=build-gtk)
			export PINESY_DEBUG_GTK=1
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
