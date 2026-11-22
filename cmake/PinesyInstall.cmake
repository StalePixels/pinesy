# The install rules the GNOME app and the KDE app share: the core, the
# engine and the resources folder, beside the app.
#
#   pinesy_install(<app target> BINDIR <dir> LIBDIR <dir> DATADIR <dir>)
#
# The three folders are relative to the install prefix. The app goes in
# BINDIR (the caller installs it), libpinesy and the engine library in
# LIBDIR, and the resources folder in DATADIR/<PINESY_INSTALL_NAME>/resources
# (by default DATADIR/pinesy/resources), filled by install-resources.sh.
#
# An installed copy loads its libraries from beside itself: the app's
# RUNPATH is $ORIGIN/<LIBDIR from BINDIR>, libpinesy's is $ORIGIN, and
# no build or source folder is in either. The app gets
# PINESY_INSTALLED_RESOURCES, the resources folder relative to BINDIR,
# for the core's resources_folder().

set(_pinesy_install_resources "${CMAKE_CURRENT_LIST_DIR}/install-resources.sh")
set(PINESY_INSTALL_NAME "pinesy" CACHE STRING "The folder in DATADIR that holds the installed resources folder")

function(pinesy_install app)
	cmake_parse_arguments(ARG "" "BINDIR;LIBDIR;DATADIR" "" ${ARGN})
	file(RELATIVE_PATH _lib "/${ARG_BINDIR}" "/${ARG_LIBDIR}")
	file(RELATIVE_PATH _resources "/${ARG_BINDIR}" "/${ARG_DATADIR}/${PINESY_INSTALL_NAME}/resources")

	set_target_properties(${app} PROPERTIES
		INSTALL_RPATH "$ORIGIN/${_lib}"
		INSTALL_RPATH_USE_LINK_PATH OFF)
	set_target_properties(pinesy PROPERTIES
		INSTALL_RPATH "$ORIGIN"
		INSTALL_RPATH_USE_LINK_PATH OFF)
	target_compile_definitions(${app} PRIVATE PINESY_INSTALLED_RESOURCES="${_resources}")

	install(TARGETS pinesy LIBRARY DESTINATION "${ARG_LIBDIR}")
	install(PROGRAMS "${PINESY_ENGINE_LIBRARY}" DESTINATION "${ARG_LIBDIR}")
	install(CODE "execute_process(
		COMMAND \"${_pinesy_install_resources}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${ARG_DATADIR}/${PINESY_INSTALL_NAME}/resources\"
		COMMAND_ERROR_IS_FATAL ANY)")
endfunction()
