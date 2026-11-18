#!/bin/bash
#
# Build libmame_pinesy (.dylib on macOS, .so on Linux) with the machines
# listed in build-mame.conf.
#
# Usage: build-mame.sh [--qt | --gtk] [NAME=value ...]
#
# Each NAME=value goes to make (for example QT_HOME=/usr). Anything
# else is refused.
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

# The rest goes to make, so only NAME=value is allowed: make would take
# a mistyped option such as -qt as its own -q -t, which touches the
# targets instead of building them and leaves the build folder damaged.
for arg in "$@"; do
	case "$arg" in
		[A-Za-z_]*=*) ;;
		*)
			echo "build-mame: unknown argument: $arg" >&2
			echo "usage: build-mame.sh [--qt | --gtk] [NAME=value ...]" >&2
			exit 1
			;;
	esac
done

# On Linux, check what the build needs before it starts. A missing
# compiler otherwise shows up deep in MAME's build, as an error about the
# GCC version. The check names the package of this OS for each missing
# part (cmake/PinesyEngineCheck.cmake).
if [ "$(uname -s)" != Darwin ]; then
	if ! command -v cmake >/dev/null 2>&1; then
		echo "build-mame: cmake is not installed. Install the cmake package of your distribution." >&2
		exit 1
	fi
	flavour=plain
	[ $qt = 1 ] && flavour=qt
	[ $gtk = 1 ] && flavour=gtk
	qt_home=""
	for arg in "$@"; do
		case "$arg" in QT_HOME=*) qt_home=${arg#QT_HOME=} ;; esac
	done
	report=$(cmake -DFLAVOUR=$flavour -DQT_HOME="$qt_home" -P "$here/cmake/PinesyEngineCheck.cmake" 2>&1)
	if [ -n "$report" ]; then
		printf '%s\n' "$report" >&2
		exit 1
	fi
fi

# mame, qemu and vendor/rpi-utils build at the commits this repo pins.
# A pull moves the pins, not the checkouts, so put each submodule on its
# pin first. Not --recursive: qemu's own submodules are firmware sources
# that this build does not use.
if [ -e "$here/.git" ]; then
	git -C "$here" submodule update --init
fi

if [ ! -f "$mame/makefile" ]; then
	echo "build-mame: $mame is empty; run: git submodule update --init" >&2
	exit 1
fi

. "$conf"
if [ -z "$SOURCES" ]; then
	echo "build-mame: SOURCES is empty in $conf" >&2
	exit 1
fi
sources=$SOURCES

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
