#!/bin/bash
#
# Build gdb-pinesy and objcopy-pinesy: upstream GDB and GNU binutils'
# objcopy, unmodified, for the two debug stubs of Pinesy. MAME's GDB stub
# serves the Next's Z80, and QEMU's serves the Pi's ARM1176 (ARMv6), bare
# metal or running Linux. objcopy-pinesy makes the Z80 ELF symbol files
# that gdb-pinesy reads (docs/gdb.md).
#
# Usage: build-gdb.sh [--prefix DIR]
#
# The source is vendor/gdb, a shallow checkout of a GDB release tag. The
# build runs out of tree in build/gdb/obj and installs into DIR (default
# build/gdb/install), which gets bin/gdb-pinesy and bin/objcopy-pinesy.
#
# Only three targets are built in: z80-unknown-elf, arm-none-eabi (bare
# metal) and arm-linux-gnueabihf (GNU/Linux). Python is not built in.
#
# GMP and MPFR come from GMP_PREFIX and MPFR_PREFIX (a folder with
# include and lib). On macOS they default to Homebrew's and are linked
# statically, so the program loads only libraries that come with macOS.
# On other systems they default to the system's, as shared libraries.
# bash 3.2 and BSD tools.

set -e

here=$(cd "$(dirname "$0")" && pwd)
src=$here/vendor/gdb
build=$here/build/gdb
prefix=$build/install

while [ $# -gt 0 ]; do
	case "$1" in
		--prefix)
			[ $# -ge 2 ] || { echo "build-gdb: --prefix needs a folder" >&2; exit 1; }
			prefix=$2
			shift 2
			;;
		*)
			echo "build-gdb: unknown argument: $1" >&2
			echo "usage: build-gdb.sh [--prefix DIR]" >&2
			exit 1
			;;
	esac
done

# vendor/gdb is not fetched with the other submodules (.gitmodules sets
# update = none for it), so ask for it by name. It is a shallow checkout
# of the tag this repo pins.
if [ -e "$here/.git" ]; then
	git -C "$here" submodule update --init --checkout vendor/gdb
fi

if [ ! -f "$src/configure" ]; then
	echo "build-gdb: $src is empty; run: git submodule update --init --checkout vendor/gdb" >&2
	exit 1
fi

# A git checkout does not carry GDB's parsers and lexer, which are
# generated from .y files with bison 3.0 or later and from .l files with
# flex. macOS has bison 2.3 and flex. A release tarball carries them, so it
# needs neither.
if [ ! -f "$src/gdb/c-exp.c" ]; then
	bison_major=$(bison --version 2>/dev/null | sed -n '1s/.* //p')
	bison_major=${bison_major%%.*}
	if [ "${bison_major:-0}" -lt 3 ]; then
		if [ "$(uname -s)" = Darwin ] && command -v brew >/dev/null 2>&1 \
			&& [ -x "$(brew --prefix bison 2>/dev/null)/bin/bison" ]; then
			PATH=$(brew --prefix bison)/bin:$PATH
			export PATH
		else
			echo "build-gdb: bison 3.0 or later is needed. On macOS: brew install bison" >&2
			exit 1
		fi
	fi
fi
if [ ! -f "$src/gdb/ada-lex.c" ] && ! command -v flex >/dev/null 2>&1; then
	echo "build-gdb: flex is needed. Install the flex package of your distribution." >&2
	exit 1
fi

extra=()
case "$(uname -s)" in
	Darwin)
		jobs=$(sysctl -n hw.ncpu)
		# the deployment target of the app (app/build.sh)
		export MACOSX_DEPLOYMENT_TARGET=26.0
		if ! command -v brew >/dev/null 2>&1; then
			echo "build-gdb: Homebrew is needed for gmp and mpfr: brew install gmp mpfr" >&2
			exit 1
		fi
		# The linker takes a .dylib before a .a when both are in a
		# folder. A folder with only the .a files and the headers makes
		# GDB link GMP and MPFR statically.
		gmp=${GMP_PREFIX:-$(brew --prefix gmp)}
		mpfr=${MPFR_PREFIX:-$(brew --prefix mpfr)}
		static=$build/static-libs
		rm -rf "$static"
		mkdir -p "$static/include" "$static/lib"
		ln -s "$gmp/include/gmp.h" "$static/include/"
		ln -s "$mpfr/include/mpfr.h" "$mpfr/include/mpf2mpfr.h" "$static/include/"
		ln -s "$gmp/lib/libgmp.a" "$mpfr/lib/libmpfr.a" "$static/lib/"
		GMP_PREFIX=$static
		MPFR_PREFIX=$static
		# The SDK declares pipe2, which macOS has from version 27. A
		# program that calls it on an earlier macOS crashes at start-up,
		# so configure must not find it. Every configure script in the
		# tree reads this variable from the environment.
		export ac_cv_func_pipe2=no
		;;
	*)
		jobs=$(nproc)
		;;
esac
[ -z "$GMP_PREFIX" ] || extra=("${extra[@]}" --with-gmp="$GMP_PREFIX")
[ -z "$MPFR_PREFIX" ] || extra=("${extra[@]}" --with-mpfr="$MPFR_PREFIX")

mkdir -p "$build/obj"
cd "$build/obj"

# GDB, and objcopy from the binutils in the same source tree. The
# assembler, linker, the other binutils programs, simulators and
# gdbserver are not built. NLS, zstd, lzma, debuginfod and Python would
# each add a library that the bundle would have to carry. expat, which
# GDB needs to read the target description of a stub, and zlib come from
# the system.
#
# configure runs again when the arguments below change, so a build
# folder from an earlier version of this script gets objcopy too.
configure_args=(
	--prefix="$prefix"
	--target=z80-unknown-elf
	--enable-targets=arm-none-eabi,arm-linux-gnueabihf
	--program-suffix=-pinesy
	--disable-gas --disable-ld --disable-gold
	--disable-gprof --disable-gprofng --disable-sim --disable-gdbserver
	--disable-nls --disable-werror
	--with-system-zlib --with-expat
	--without-python --without-guile --without-babeltrace
	--without-debuginfod --without-lzma --without-zstd --without-xxhash
	--without-libunwind-ia64
	${extra[@]+"${extra[@]}"}
	CFLAGS=-O2 CXXFLAGS=-O2
)
if [ ! -f Makefile ] || [ "$(cat configure-args 2>/dev/null)" != "${configure_args[*]}" ]; then
	"$src/configure" "${configure_args[@]}"
	echo "${configure_args[*]}" > configure-args
fi

# MAKEINFO=true: the manual is not built, so texinfo is not needed
make -j"$jobs" MAKEINFO=true all-gdb
make MAKEINFO=true install-gdb

echo "build-gdb: installed $prefix/bin/gdb-pinesy"

# objcopy only. It links the bfd, libiberty and libsframe that GDB
# already built. A GDB release tarball has no binutils folder; the
# Flatpak manifests build objcopy-pinesy from the binutils release.
if [ -d "$src/binutils" ]; then
	make MAKEINFO=true configure-binutils
	make -C binutils -j"$jobs" MAKEINFO=true objcopy
	mkdir -p "$prefix/bin"
	install -m 755 binutils/objcopy "$prefix/bin/objcopy-pinesy"
	echo "build-gdb: installed $prefix/bin/objcopy-pinesy"
fi
