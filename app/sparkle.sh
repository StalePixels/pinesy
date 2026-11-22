#!/bin/bash
# Gets Sparkle, the macOS app's updater, and prints the folder it is in.
#
#   app/sparkle.sh
#
# The release named below is downloaded once from Sparkle's GitHub
# releases into ../build/sparkle/<version>, and checked against its
# SHA-256. The folder holds Sparkle.framework, which app/build.sh puts in
# Contents/Frameworks, and bin/, with sign_update for CI.

set -euo pipefail

version=2.10.1
sha256=1a856d2a9edb8e628de88121d7be04cf0e2167f8641650b78fca00b14244cf27
url=https://github.com/sparkle-project/Sparkle/releases/download/$version/Sparkle-$version.tar.xz

here=$(cd "$(dirname "$0")" && pwd)
dir=$here/../build/sparkle/$version

if [ ! -d "$dir/Sparkle.framework" ]; then
	rm -rf "$dir.part"
	mkdir -p "$dir.part"
	curl -fsSL -o "$dir.part/Sparkle.tar.xz" "$url"
	got=$(shasum -a 256 "$dir.part/Sparkle.tar.xz" | cut -d' ' -f1)
	if [ "$got" != "$sha256" ]; then
		echo "sparkle.sh: Sparkle-$version.tar.xz has SHA-256 $got, not $sha256" >&2
		exit 1
	fi
	tar -xf "$dir.part/Sparkle.tar.xz" -C "$dir.part"
	rm "$dir.part/Sparkle.tar.xz"
	rm -rf "$dir"
	mv "$dir.part" "$dir"
fi
cd "$dir" && pwd
