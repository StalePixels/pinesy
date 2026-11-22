#!/bin/bash
# Builds Pinesy.app.
#
#   app/build.sh [output-dir]      (default: app/build)
#
# MAME_LIB overrides the library to bundle
# (default: mame/libmame_pinesy.dylib in this repo). It must be named
# libmame_pinesy.dylib: the app links -lmame_pinesy from its folder.
#
# GDB_BIN overrides gdb-pinesy, the GDB for the Next's Z80 and the Pi's
# ARM (default: build/gdb/install/bin/gdb-pinesy in this repo, which
# ../build-gdb.sh builds). It goes in Contents/MacOS. OBJCOPY_BIN does the
# same for objcopy-pinesy, the objcopy that makes Z80 ELF symbol files for
# gdb-pinesy (default: build/gdb/install/bin/objcopy-pinesy).
#
# The core, libpinesy.dylib, is compiled from ../core/src into
# <output-dir>/core, linked against the engine library.
#
# The bundle carries the app icon (Pinesy.icns), the ROM sets in ../roms in Contents/Resources/roms,
# MAME's plugins, hash, bgfx, artwork and language folders in
# Contents/Resources, and libmame_pinesy.dylib and every library it needs outside /usr/lib
# and /System in Contents/Frameworks, with libpinesy.dylib. Their install
# names are rewritten to @rpath, which the executable points at
# Contents/Frameworks. Sparkle.framework, the updater, goes there too;
# sparkle.sh gets it (SPARKLE_DIR overrides the folder it is in).
#
# A CI build that is published sets PINESY_BUILD_TRACK (ci, nightly or
# release) and PINESY_BUILD_VERSION. They go into the core (with the kind
# "macos") and into Info.plist as CFBundleVersion and
# CFBundleShortVersionString, which Sparkle compares. A local build sets
# neither, and never looks for updates.
#
# The bundle is signed ad hoc. With CODESIGN_IDENTITY, it is signed with
# that identity instead (from CODESIGN_KEYCHAIN, when set), with the
# hardened runtime, a secure timestamp and Pinesy.entitlements on the
# executable, as notarisation needs.

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
out=${1:-$here/build}
mame_lib=${MAME_LIB:-$here/../mame/libmame_pinesy.dylib}
gdb_bin=${GDB_BIN:-$here/../build/gdb/install/bin/gdb-pinesy}
objcopy_bin=${OBJCOPY_BIN:-$here/../build/gdb/install/bin/objcopy-pinesy}

sparkle=${SPARKLE_DIR:-$("$here/sparkle.sh")}
track=${PINESY_BUILD_TRACK:-}
version=${PINESY_BUILD_VERSION:-}

app=$out/Pinesy.app
contents=$app/Contents
frameworks=$contents/Frameworks

rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Resources" "$frameworks"
cp "$here/Info.plist" "$contents/Info.plist"
identity=()
if [ -n "$track" ] && [ -n "$version" ]; then
	plutil -replace CFBundleVersion -string "$version" "$contents/Info.plist"
	plutil -replace CFBundleShortVersionString -string "$version" "$contents/Info.plist"
	identity=(-DPINESY_BUILD_TRACK="\"$track\"" -DPINESY_BUILD_VERSION="\"$version\"" -DPINESY_BUILD_KIND='"macos"')
fi
cp "$here/Pinesy.icns" "$here/PinesyDocument.icns" "$contents/Resources/"

# The ROM sets ship in the bundle: one zip, 7z or folder of ROM dumps per
# set. Disk images (SD cards, hard disks, floppies) never ship, loose or
# inside a zip.
disk_image='\.(img|ima|chd|hdf|hdd|vhd|iso|cue|dmg|dsk|edsk|trd|scl|mgt|fdi|td0|udi|mfm|hfe|ipf|imd|opd|opu|d40|d80|sad|zip|7z)$'
roms=$here/../roms
if [ -d "$roms" ]; then
	bad=""
	for entry in "$roms"/*; do
		[ -e "$entry" ] || continue
		name=$(basename "$entry")
		case "$name" in
			*.zip|*.ZIP)
				# a zip of a ROM set holds ROM dumps only
				found=$(unzip -Z1 "$entry" | grep -iE "$disk_image" || true)
				[ -z "$found" ] || bad="$bad$entry: $found
"
				;;
			*.7z|*.7Z)
				# a 7z cannot be listed with the tools macOS has, so its
				# contents are not checked
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
		echo "build.sh: only ROM sets (a zip, 7z or folder of ROM dumps) go in the bundle:" >&2
		printf '%s' "$bad" >&2
		exit 1
	fi
	cp -R "$roms" "$contents/Resources/roms"
else
	mkdir -p "$contents/Resources/roms"
fi

# MAME's own support files, in the folders the core's fixed options point at
for folder in plugins hash bgfx artwork language; do
	cp -R "$here/../mame/$folder" "$contents/Resources/$folder"
done

# The core: the rules the app shares with the GNOME and KDE apps
core=$here/../core
core_lib=$out/core/libpinesy.dylib
mkdir -p "$out/core"
clang++ -std=c++20 -O2 -Wall -Wextra \
	-target arm64-apple-macos26.0 \
	-dynamiclib -install_name @rpath/libpinesy.dylib \
	-I "$core/include" -I "$core/src" -I "$here/../engine/include" \
	${identity[@]+"${identity[@]}"} \
	-L "$(dirname "$mame_lib")" -lmame_pinesy \
	-o "$core_lib" "$core"/src/*.cpp

swiftc -swift-version 6 -O \
	-target arm64-apple-macos26.0 \
	-I "$here/Sources/CPinesy" \
	-Xcc -I"$here/../engine/include" \
	-L "$out/core" -lpinesy \
	-L "$(dirname "$mame_lib")" -lmame_pinesy \
	-F "$sparkle" -framework Sparkle \
	-Xlinker -rpath -Xlinker @executable_path/../Frameworks \
	-o "$contents/MacOS/Pinesy" \
	"$here"/Sources/*.swift

if [ ! -x "$gdb_bin" ]; then
	echo "build.sh: $gdb_bin is missing; run ../build-gdb.sh" >&2
	exit 1
fi
cp "$gdb_bin" "$contents/MacOS/gdb-pinesy"
chmod u+w "$contents/MacOS/gdb-pinesy"
if [ ! -x "$objcopy_bin" ]; then
	echo "build.sh: $objcopy_bin is missing; run ../build-gdb.sh" >&2
	exit 1
fi
cp "$objcopy_bin" "$contents/MacOS/objcopy-pinesy"
chmod u+w "$contents/MacOS/objcopy-pinesy"

# the libraries a Mach-O file loads, without its own install name
dependencies() {
	local id
	id=$(otool -D "$1" | tail -n +2)
	otool -L "$1" | tail -n +2 | awk '{print $1}' | while read -r dep; do
		[ "$dep" = "$id" ] || echo "$dep"
	done
}

# libraries that come with macOS stay where they are
is_system() {
	case "$1" in
		/usr/lib/*|/System/*) return 0 ;;
		*) return 1 ;;
	esac
}

ditto "$sparkle/Sparkle.framework" "$frameworks/Sparkle.framework"
cp "$mame_lib" "$core_lib" "$frameworks/"
chmod u+w "$frameworks/$(basename "$mame_lib")" "$frameworks/libpinesy.dylib"

# Copy each library's own dependencies until none are left to copy.
pending="$frameworks/$(basename "$mame_lib") $frameworks/libpinesy.dylib"
while [ -n "$pending" ]; do
	next=""
	for lib in $pending; do
		name=$(basename "$lib")
		install_name_tool -id "@rpath/$name" "$lib" 2>/dev/null
		# drop search paths that point outside the bundle
		otool -l "$lib" | awk '/cmd LC_RPATH/ {getline; getline; print $2}' | while read -r rpath; do
			case "$rpath" in
				@*) ;;
				*) install_name_tool -delete_rpath "$rpath" "$lib" 2>/dev/null ;;
			esac
		done
		for dep in $(dependencies "$lib"); do
			is_system "$dep" && continue
			depname=$(basename "$dep")
			install_name_tool -change "$dep" "@rpath/$depname" "$lib" 2>/dev/null
			case "$dep" in @rpath/*) continue ;; esac
			if [ ! -e "$frameworks/$depname" ]; then
				cp -L "$dep" "$frameworks/$depname"
				chmod u+w "$frameworks/$depname"
				next="$next $frameworks/$depname"
			fi
		done
	done
	pending=$next
done

# a reference left outside the bundle would load from the build machine
for bin in "$contents/MacOS/Pinesy" "$contents/MacOS/gdb-pinesy" "$contents/MacOS/objcopy-pinesy" "$frameworks"/*.dylib; do
	for dep in $(dependencies "$bin"); do
		if ! is_system "$dep" && [ "${dep#@rpath/}" = "$dep" ]; then
			echo "build.sh: $bin still loads $dep" >&2
			exit 1
		fi
		if [ "${dep#@rpath/}" != "$dep" ] && [ ! -e "$frameworks/${dep#@rpath/}" ]; then
			echo "build.sh: $bin loads $dep, which is not in the bundle" >&2
			exit 1
		fi
	done
done

# Inside out: each library and helper first, the bundle last.
sign=(codesign --force --sign -)
if [ -n "${CODESIGN_IDENTITY:-}" ]; then
	sign=(codesign --force --sign "$CODESIGN_IDENTITY" --options runtime --timestamp)
	[ -z "${CODESIGN_KEYCHAIN:-}" ] || sign+=(--keychain "$CODESIGN_KEYCHAIN")
fi
for lib in "$frameworks"/*.dylib; do
	"${sign[@]}" "$lib"
done
"${sign[@]}" "$contents/MacOS/gdb-pinesy"
"${sign[@]}" "$contents/MacOS/objcopy-pinesy"
sparkle_b=$frameworks/Sparkle.framework/Versions/B
"${sign[@]}" "$sparkle_b/XPCServices/Installer.xpc"
"${sign[@]}" --preserve-metadata=entitlements "$sparkle_b/XPCServices/Downloader.xpc"
"${sign[@]}" "$sparkle_b/Autoupdate"
"${sign[@]}" "$sparkle_b/Updater.app"
"${sign[@]}" "$frameworks/Sparkle.framework"
"${sign[@]}" --entitlements "$here/Pinesy.entitlements" "$app"

echo "built $app"
