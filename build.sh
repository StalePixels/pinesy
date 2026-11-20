#!/bin/bash
#
# Builds Pinesy: the engine, gdb-pinesy and objcopy-pinesy (build-gdb.sh),
# and the app for this platform.
#
#   macOS:  ./build.sh         the app is app/build/Pinesy.app
#   Linux:  ./build.sh gtk     the GNOME app is build/gtk/pinesy
#           ./build.sh kde     the KDE app is build/kde/bin/pinesy-kde
#
# On macOS, gdb-pinesy and objcopy-pinesy go in the app, so they build
# before it: the engine, then GDB, then the app.
#
# On Linux they stay in build/gdb/install/bin. Before anything builds,
# build-gdb.sh --check names the packages GDB needs that are missing. Then
# the engine builds, then the app, and GDB last: the app and the engine
# must come from the same source, and a GDB build that fails after the
# engine would leave the old app with the new engine.
#
# What each build needs is in docs/build-macos.md and docs/build-linux.md.

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
cd "$here"

usage() {
	echo "usage: build.sh            (macOS)" >&2
	echo "       build.sh gtk | kde  (Linux)" >&2
	exit 1
}

case "$(uname -s)" in
	Darwin)
		case "${1:-}" in
			"") ;;
			gtk)
				echo "The GNOME app (gtk) builds on Linux only. On macOS, run ./build.sh to build the macOS app." >&2
				exit 1
				;;
			kde)
				echo "The KDE app (kde) builds on Linux only. On macOS, run ./build.sh to build the macOS app." >&2
				exit 1
				;;
			*)
				usage
				;;
		esac
		./build-mame.sh
		./build-gdb.sh
		app/build.sh
		;;
	*)
		[ $# -eq 1 ] || usage
		case "$1" in
			gtk)
				./build-gdb.sh --check
				./build-mame.sh --gtk
				cmake -S gtk -B build/gtk
				cmake --build build/gtk --parallel "$(nproc)"
				./build-gdb.sh
				;;
			kde)
				./build-gdb.sh --check
				./build-mame.sh --qt
				cmake -S kde -B build/kde
				cmake --build build/kde --parallel "$(nproc)"
				./build-gdb.sh
				;;
			*)
				usage
				;;
		esac
		;;
esac
