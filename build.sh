#!/bin/bash
#
# Builds Pinesy: the engine, then the app for this platform.
#
#   macOS:  ./build.sh         the app is app/build/Pinesy.app
#   Linux:  ./build.sh gtk     the GNOME app is build/gtk/pinesy
#           ./build.sh kde     the KDE app is build/kde/bin/pinesy-kde
#
# Each build also builds gdb-pinesy and objcopy-pinesy (build-gdb.sh). On
# macOS they go in the app. On Linux they stay in build/gdb/install/bin.
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
				./build-mame.sh --gtk
				./build-gdb.sh
				cmake -S gtk -B build/gtk
				cmake --build build/gtk --parallel "$(nproc)"
				;;
			kde)
				./build-mame.sh --qt
				./build-gdb.sh
				cmake -S kde -B build/kde
				cmake --build build/kde --parallel "$(nproc)"
				;;
			*)
				usage
				;;
		esac
		;;
esac
