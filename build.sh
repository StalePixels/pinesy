#!/bin/bash
#
# Builds Pinesy: the engine, then the app for this platform.
#
#   macOS:  ./build.sh         the app is app/build/Pinesy.app
#   Linux:  ./build.sh gtk     the GNOME app is build/gtk/pinesy
#           ./build.sh kde     the KDE app is build/kde/bin/pinesy-kde
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
		if [ $# -ne 0 ]; then
			echo "build.sh: on macOS this builds the macOS app; gtk and kde build on Linux only" >&2
			usage
		fi
		./build-mame.sh
		app/build.sh
		;;
	*)
		[ $# -eq 1 ] || usage
		case "$1" in
			gtk)
				./build-mame.sh --gtk
				cmake -S gtk -B build/gtk
				cmake --build build/gtk --parallel "$(nproc)"
				;;
			kde)
				./build-mame.sh --qt
				cmake -S kde -B build/kde
				cmake --build build/kde --parallel "$(nproc)"
				;;
			*)
				usage
				;;
		esac
		;;
esac
