#!/bin/bash
# Builds Pinesy.app.
#
#   app/build.sh [output-dir]      (default: app/build)
#
# MAME_LIB overrides the library to bundle
# (default: mame/libmame_pinesy.dylib in this repo).
#
# The bundle carries the ROM sets in ../roms in Contents/Resources/roms,
# and libmame_pinesy.dylib and every library it needs outside /usr/lib
# and /System in Contents/Frameworks. Their install
# names are rewritten to @rpath, which the executable points at
# Contents/Frameworks. The bundle is signed ad hoc.

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
out=${1:-$here/build}
mame_lib=${MAME_LIB:-$here/../mame/libmame_pinesy.dylib}

app=$out/Pinesy.app
contents=$app/Contents
frameworks=$contents/Frameworks

rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Resources" "$frameworks"
cp "$here/Info.plist" "$contents/Info.plist"

# The ROM sets ship in the bundle: one zip or folder per set. Disk images
# (SD cards, hard disks) never ship.
roms=$here/../roms
if [ -d "$roms" ]; then
	images=$(find "$roms" -type f \( -iname '*.img' -o -iname '*.chd' -o -iname '*.hdf' -o -iname '*.vhd' -o -iname '*.iso' -o -iname '*.dmg' \))
	if [ -n "$images" ]; then
		echo "build.sh: disk images do not go in the bundle:" >&2
		echo "$images" >&2
		exit 1
	fi
	cp -R "$roms" "$contents/Resources/roms"
else
	mkdir -p "$contents/Resources/roms"
fi

swiftc -swift-version 6 -O \
	-target arm64-apple-macos27.2 \
	-I "$here/Sources/CPinesy" \
	-L "$(dirname "$mame_lib")" -lmame_pinesy \
	-Xlinker -rpath -Xlinker @executable_path/../Frameworks \
	-o "$contents/MacOS/Pinesy" \
	"$here"/Sources/*.swift

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

cp "$mame_lib" "$frameworks/"
chmod u+w "$frameworks/$(basename "$mame_lib")"

# Copy each library's own dependencies until none are left to copy.
pending="$frameworks/$(basename "$mame_lib")"
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
for bin in "$contents/MacOS/Pinesy" "$frameworks"/*.dylib; do
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

for lib in "$frameworks"/*.dylib; do
	codesign --force --sign - "$lib"
done
codesign --force --sign - "$app"

echo "built $app"
