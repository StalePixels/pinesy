# The Debian package pinesy-gdb: gdb-pinesy and objcopy-pinesy, which
# ../../build-gdb.sh builds, in /usr/bin. The GNOME app's package and the
# KDE app's package both depend on it, so they can be installed side by
# side. With CMAKE_INSTALL_PREFIX=/usr, `cpack -G DEB` in the build folder
# makes pinesy-gdb_<version>_<architecture>.deb (../PinesyDeb.cmake).

cmake_minimum_required(VERSION 3.20)
project(pinesy_gdb NONE)

set(PINESY_GDB_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../../build/gdb/install/bin" CACHE PATH "The folder with gdb-pinesy and objcopy-pinesy")
set(PINESY_BUILD_VERSION "" CACHE STRING "The version of a CI build")
foreach(_program gdb-pinesy objcopy-pinesy)
	if(NOT EXISTS "${PINESY_GDB_BIN}/${_program}")
		message(FATAL_ERROR "${PINESY_GDB_BIN}/${_program} is missing; run build-gdb.sh")
	endif()
	install(PROGRAMS "${PINESY_GDB_BIN}/${_program}" DESTINATION bin)
endforeach()

include("${CMAKE_CURRENT_SOURCE_DIR}/../PinesyDeb.cmake")
pinesy_deb(NAME pinesy-gdb SUMMARY "Pi/Ne-Sy, a Pi/Next-System emulator: gdb-pinesy and objcopy-pinesy")
include(CPack)
