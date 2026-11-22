# The Debian packages of the GNOME app, the KDE app and gdb-pinesy, made
# with CPack:
#
#   pinesy_deb(NAME <package> SUMMARY <text> [LIBDIR <dir>] [DEPENDS <packages>])
#
# After a build and install with CMAKE_INSTALL_PREFIX=/usr, `cpack -G DEB`
# in the build folder makes <package>_<version>_<architecture>.deb. The
# version is PINESY_BUILD_VERSION, which CI sets with the rest of the
# build's identity (../core/CMakeLists.txt); a build without it gets
# version 0. dpkg-shlibdeps works out the packages it depends on, with
# the private libraries in LIBDIR (relative to /), and DEPENDS adds others.
#
# The two packages can be installed side by side, so each keeps its own
# copy of libpinesy, the engine and the resources folder: configure the
# GNOME app with CMAKE_INSTALL_LIBDIR=lib/pinesy and the KDE app with
# KDE_INSTALL_LIBDIR=lib/pinesy-kde and PINESY_INSTALL_NAME=pinesy-kde
# (PinesyInstall.cmake). The GNOME app also needs
# PINESY_INSTALL_COMPILED_SCHEMAS off: the system compiles its schemas
# when the package is installed. gdb-pinesy and objcopy-pinesy are in a
# package of their own, pinesy-gdb (gdb-deb/), which both apps depend on.

function(pinesy_deb)
	cmake_parse_arguments(ARG "" "NAME;SUMMARY;LIBDIR;DEPENDS" "" ${ARGN})
	set(CPACK_GENERATOR DEB PARENT_SCOPE)
	set(CPACK_PACKAGE_NAME "${ARG_NAME}" PARENT_SCOPE)
	if(PINESY_BUILD_VERSION)
		set(CPACK_PACKAGE_VERSION "${PINESY_BUILD_VERSION}" PARENT_SCOPE)
	else()
		set(CPACK_PACKAGE_VERSION 0 PARENT_SCOPE)
	endif()
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${ARG_SUMMARY}" PARENT_SCOPE)
	set(CPACK_PACKAGE_CONTACT "StalePixels <https://github.com/StalePixels/pinesy>" PARENT_SCOPE)
	set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_SECTION otherosfs PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_HOMEPAGE https://github.com/StalePixels/pinesy PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${ARG_LIBDIR}" PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${ARG_DEPENDS}" PARENT_SCOPE)
	set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" PARENT_SCOPE)
endfunction()
