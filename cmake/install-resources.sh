#!/bin/bash
# Installs the resources folder of the Linux apps: the content app/build.sh
# puts in Contents/Resources of the macOS bundle. `cmake --install` of the
# GNOME app and of the KDE app runs it (PinesyInstall.cmake).
#
#   cmake/install-resources.sh <resources-dir>
#
# <resources-dir>/roms gets the ROM sets in ../roms, with the same rule as
# app/build.sh: one zip, 7z or folder of ROM dumps per set, and no disk
# image (SD card, hard disk or floppy), loose or inside a zip. A 7z is not
# checked. <resources-dir> gets MAME's plugins, hash, bgfx, artwork and
# language folders from ../mame.

set -euo pipefail

here=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?usage: install-resources.sh <resources-dir>}

disk_image='\.(img|ima|chd|hdf|hdd|vhd|iso|cue|dmg|dsk|edsk|trd|scl|mgt|fdi|td0|udi|mfm|hfe|ipf|imd|opd|opu|d40|d80|sad|zip|7z)$'
roms=$here/roms
mkdir -p "$out"
if [ -d "$roms" ]; then
	bad=""
	for entry in "$roms"/*; do
		[ -e "$entry" ] || continue
		name=$(basename "$entry")
		case "$name" in
			*.zip|*.ZIP)
				found=$(unzip -Z1 "$entry" | grep -iE "$disk_image" || true)
				[ -z "$found" ] || bad="$bad$entry: $found
"
				;;
			*.7z|*.7Z)
				;;
			*)
				if [ -d "$entry" ]; then
					found=$(find "$entry" -type f | grep -iE "$disk_image" || true)
					[ -z "$found" ] || bad="$bad$found
"
				else
					bad="$bad$entry
"
				fi
				;;
		esac
	done
	if [ -n "$bad" ]; then
		echo "install-resources.sh: only ROM sets (a zip, 7z or folder of ROM dumps) go in the resources folder:" >&2
		printf '%s' "$bad" >&2
		exit 1
	fi
	rm -rf "$out/roms"
	cp -R "$roms" "$out/roms"
else
	mkdir -p "$out/roms"
fi

for folder in plugins hash bgfx artwork language; do
	rm -rf "${out:?}/$folder"
	cp -R "$here/mame/$folder" "$out/$folder"
done
