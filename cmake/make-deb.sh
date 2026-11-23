#!/bin/bash
# Makes a Debian package from a build that ./build.sh made, without
# compiling anything.
#
#   cmake/make-deb.sh <gtk|kde|gdb> <out-dir> [<version> [<track>]]
#
# The packages:
#
#   gtk  pinesy      the GNOME app, from build/gtk
#   kde  pinesy-kde  the KDE app, from build/kde
#   gdb  pinesy-gdb  gdb-pinesy and objcopy-pinesy, from build/gdb/install/bin
#
# Both apps depend on pinesy-gdb, and each keeps its own libpinesy,
# engine and resources folder (lib/<name>, share/<name>), so both can be
# installed at once. The app is installed into a staging folder with
# cmake --install and the prefix /usr. The D-Bus service file then names
# /usr/bin, and the compiled schemas file is left out: the system
# compiles its schemas when the package is installed.
#
# The version is 0 unless given. With a track (ci, nightly or release),
# the app gets the build's identity, build-identity.json in its
# resources folder, with the kind "deb" (../core/README.md, "Updates").
# dpkg-shlibdeps (dpkg-dev) works out the packages it depends on.
# The package is <out-dir>/<package>_<version>_<architecture>.deb.

set -euo pipefail

here=$(cd "$(dirname "$0")/.." && pwd)
what=${1:?usage: make-deb.sh <gtk|kde|gdb> <out-dir> [<version> [<track>]]}
out=${2:?usage: make-deb.sh <gtk|kde|gdb> <out-dir> [<version> [<track>]]}
version=${3:-0}
track=${4:-}
arch=$(dpkg --print-architecture)
homepage=https://github.com/StalePixels/pinesy

case $what in
	gtk)
		package=pinesy name=pinesy build=$here/build/gtk
		summary="Pi/Ne-Sy, a Pi/Next-System emulator: the GNOME app"
		depends="pinesy-gdb"
		;;
	kde)
		package=pinesy-kde name=pinesy-kde build=$here/build/kde
		summary="Pi/Ne-Sy, a Pi/Next-System emulator: the KDE Plasma app"
		# the QML modules ../kde/qml imports, which dpkg-shlibdeps cannot see
		depends="pinesy-gdb, qml6-module-org-kde-kirigami, qml6-module-qtcore, qml6-module-qtqml-models, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-window"
		;;
	gdb)
		package=pinesy-gdb name= build=$here/build/gdb/install/bin
		summary="Pi/Ne-Sy, a Pi/Next-System emulator: gdb-pinesy and objcopy-pinesy"
		depends=""
		;;
	*)
		echo "make-deb.sh: the package is gtk, kde or gdb, not $what" >&2
		exit 1
		;;
esac
[ -d "$build" ] || { echo "make-deb.sh: no $build; build it first (./build.sh)" >&2; exit 1; }

work=$(mktemp -d "${TMPDIR:-/tmp}/make-deb.XXXXXX")
trap 'rm -rf "$work"' EXIT
stage=$work/stage
mkdir -p "$stage/DEBIAN" "$out"

if [ "$what" = gdb ]; then
	mkdir -p "$stage/usr/bin"
	cp "$build/gdb-pinesy" "$build/objcopy-pinesy" "$stage/usr/bin/"
	binaries=("$stage/usr/bin/gdb-pinesy" "$stage/usr/bin/objcopy-pinesy")
	private=()
else
	DESTDIR=$stage cmake --install "$build" --prefix /usr >/dev/null
	rm -f "$stage/usr/share/glib-2.0/schemas/gschemas.compiled"
	for service in "$stage"/usr/share/dbus-1/services/*.service; do
		sed -i "s|^Exec=.*/bin/|Exec=/usr/bin/|" "$service"
	done
	resources=$stage/usr/share/$name/resources
	[ -d "$resources" ] || { echo "make-deb.sh: no $resources after the install" >&2; exit 1; }
	if [ -n "$track" ]; then
		printf '{"track": "%s", "version": "%s", "kind": "deb"}\n' "$track" "$version" >"$resources/build-identity.json"
	fi
	binaries=("$stage/usr/bin/$package")
	libdir=$(dirname "$(find "$stage/usr/lib" -name libpinesy.so | head -1)")
	binaries+=("$libdir"/*.so)
	private=(-l"$libdir")
fi

# dpkg-shlibdeps reads debian/control in the folder it runs in. Two of
# its warnings are expected here and are left out: the private libraries
# have no versioned SONAME ("cannot extract name and version"), and
# libc6 diverts the dynamic loader for the merged /usr ("diversions
# involved"). Every other line it prints is shown.
mkdir -p "$work/debian"
printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$package" "$package" >"$work/debian/control"
shlibs=$(cd "$work" && dpkg-shlibdeps -O "${private[@]}" "${binaries[@]}" \
	2> >(grep -v -E 'cannot extract name and version from library name|diversions involved|^ diversion by libc6 ' >&2) |
	sed -n 's/^shlibs:Depends=//p')
[ -z "$shlibs" ] || depends=${depends:+$depends, }$shlibs

cat >"$stage/DEBIAN/control" <<EOF
Package: $package
Version: $version
Architecture: $arch
Maintainer: StalePixels <$homepage>
Section: otherosfs
Priority: optional
Homepage: $homepage
Installed-Size: $(du -sk --exclude=DEBIAN "$stage" | cut -f1)
Depends: $depends
Description: $summary
EOF

deb=$out/${package}_${version}_$arch.deb
dpkg-deb --root-owner-group -Zxz --build "$stage" "$deb" >/dev/null
echo "$deb"
