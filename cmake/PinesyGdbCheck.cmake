# Checks, before anything builds, that this Linux system has what
# build-gdb.sh needs for gdb-pinesy and objcopy-pinesy. Run as a script:
#
#   cmake -DSOURCE=<vendor/gdb> [-DGMP_PREFIX=<dir>] [-DMPFR_PREFIX=<dir>]
#         -P PinesyGdbCheck.cmake
#
# It prints nothing when everything is there. Otherwise it prints what to
# install, what is too old on this OS and what this OS does not have.
#
# SOURCE is GDB's source folder. bison and flex are needed only when it
# has no generated parsers and lexer: a git checkout (or a folder not
# checked out yet), not a release tarball. GMP_PREFIX and MPFR_PREFIX are
# the folders build-gdb.sh takes GMP and MPFR from; without them, the
# system's are checked.

cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/PinesyPackages.cmake")

set(_missing "")

# A header in the system's include folders, or in <prefix>/include when
# a prefix is given. /app is where a Flatpak build installs the libraries
# its manifest builds.
function(_pinesy_header out header prefix)
	if(prefix)
		set(_globs "${prefix}/include/${header}")
	else()
		set(_globs /usr/include/${header} /usr/include/*/${header}
			/usr/local/include/${header} /app/include/${header})
	endif()
	file(GLOB _files ${_globs})
	set(${out} "${_files}" PARENT_SCOPE)
endfunction()

# GDB's configure (autoconf 2.69) looks for gcc or cc, and g++ or c++.
# It does not try clang.
foreach(_compiler IN ITEMS "C compiler|gcc|cc" "C++17 compiler|g++|c++")
	string(REPLACE "|" ";" _compiler "${_compiler}")
	list(GET _compiler 0 _what)
	list(GET _compiler 1 _name)
	list(GET _compiler 2 _other)
	find_program(_found_${_name} NAMES ${_name} ${_other})
	if(NOT _found_${_name})
		list(APPEND _missing "${_what}|/usr/bin/${_name}|0")
	endif()
endforeach()

find_program(_make make)
if(NOT _make)
	list(APPEND _missing "make|/usr/bin/make|0")
endif()

# GDB's parsers and lexer
if(NOT EXISTS "${SOURCE}/gdb/c-exp.c")
	find_program(_bison bison)
	set(_bison_version "")
	if(_bison)
		execute_process(COMMAND ${_bison} --version
			OUTPUT_VARIABLE _output ERROR_QUIET)
		string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" _bison_version "${_output}")
	endif()
	if(NOT _bison OR _bison_version VERSION_LESS 3.0)
		list(APPEND _missing "bison 3.0 or later|/usr/bin/bison|3.0")
	endif()
endif()
if(NOT EXISTS "${SOURCE}/gdb/ada-lex.c")
	find_program(_flex flex)
	if(NOT _flex)
		list(APPEND _missing "flex|/usr/bin/flex|0")
	endif()
endif()

# libraries: the header configure looks for, and the file the package
# search looks up: the pkg-config file of the development package, or the
# header's full path where a search for the pkg-config file finds a
# package for cross builds first
foreach(_library IN ITEMS
		"GMP|gmp.h|gmp.pc|${GMP_PREFIX}"
		"MPFR|mpfr.h|mpfr.pc|${MPFR_PREFIX}"
		"expat|expat.h|expat.pc|"
		"zlib|zlib.h|/usr/include/zlib.h|"
		"ncurses|ncurses.h|ncursesw.pc|")
	string(REPLACE "|" ";" _library "${_library}")
	list(GET _library 0 _what)
	list(GET _library 1 _header)
	list(GET _library 2 _lookup)
	list(LENGTH _library _fields)
	set(_prefix "")
	if(_fields GREATER 3)
		list(GET _library 3 _prefix)
	endif()
	_pinesy_header(_found "${_header}" "${_prefix}")
	if(NOT _found)
		list(APPEND _missing "${_what} development files|${_lookup}|0")
	endif()
endforeach()

if(_missing)
	pinesy_missing_message(_message "debugger, gdb-pinesy" ${_missing})
	message(NOTICE "${_message}")
endif()
