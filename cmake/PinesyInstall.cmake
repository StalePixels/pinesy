# The install rules the GNOME app and the KDE app share: the core, the
# engine and the resources folder, beside the app.
#
#   pinesy_install(<app target> NAME <name> BINDIR <dir> LIBDIR <dir> DATADIR <dir>)
#
# The three folders are relative to the install prefix. The app goes in
# BINDIR (the caller installs it), libpinesy and the engine library in
# LIBDIR/<name>, and the resources folder in DATADIR/<name>/resources,
# filled by install-resources.sh. Each app has its own <name> (pinesy,
# pinesy-kde), so both can be installed in one prefix.
#
# An installed copy loads its libraries from beside itself: the app's
# RUNPATH is $ORIGIN/<LIBDIR/name from BINDIR>, libpinesy's is $ORIGIN,
# and no build or source folder is in either. The app gets
# PINESY_INSTALLED_RESOURCES, the resources folder relative to BINDIR,
# for the core's resources_folder().
#
# A CI build that is published sets PINESY_BUILD_TRACK,
# PINESY_BUILD_VERSION and PINESY_BUILD_KIND. The install then writes
# them into build-identity.json in the resources folder, where the core
# reads the build's identity (../core/README.md, "Updates"). A build
# without them has no such file, and never looks for updates.

set(_pinesy_install_resources "${CMAKE_CURRENT_LIST_DIR}/install-resources.sh")
set(PINESY_BUILD_TRACK "" CACHE STRING "The track a CI build is published to: ci, nightly or release")
set(PINESY_BUILD_VERSION "" CACHE STRING "The version of a CI build")
set(PINESY_BUILD_KIND "" CACHE STRING "How a CI build is delivered and updated: flatpak or deb")

function(pinesy_install app)
	cmake_parse_arguments(ARG "" "NAME;BINDIR;LIBDIR;DATADIR" "" ${ARGN})
	set(_libdir "${ARG_LIBDIR}/${ARG_NAME}")
	set(_resdir "${ARG_DATADIR}/${ARG_NAME}/resources")
	file(RELATIVE_PATH _lib "/${ARG_BINDIR}" "/${_libdir}")
	file(RELATIVE_PATH _resources "/${ARG_BINDIR}" "/${_resdir}")

	set_target_properties(${app} PROPERTIES
		INSTALL_RPATH "$ORIGIN/${_lib}"
		INSTALL_RPATH_USE_LINK_PATH OFF)
	set_target_properties(pinesy PROPERTIES
		INSTALL_RPATH "$ORIGIN"
		INSTALL_RPATH_USE_LINK_PATH OFF)
	target_compile_definitions(${app} PRIVATE PINESY_INSTALLED_RESOURCES="${_resources}")

	install(TARGETS pinesy LIBRARY DESTINATION "${_libdir}")
	install(PROGRAMS "${PINESY_ENGINE_LIBRARY}" DESTINATION "${_libdir}")
	install(CODE "execute_process(
		COMMAND \"${_pinesy_install_resources}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_resdir}\"
		COMMAND_ERROR_IS_FATAL ANY)")
	if(PINESY_BUILD_TRACK AND PINESY_BUILD_VERSION AND PINESY_BUILD_KIND)
		install(CODE "file(WRITE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_resdir}/build-identity.json\"
	\"{\\\"track\\\": \\\"${PINESY_BUILD_TRACK}\\\", \\\"version\\\": \\\"${PINESY_BUILD_VERSION}\\\", \\\"kind\\\": \\\"${PINESY_BUILD_KIND}\\\"}\\n\")")
	endif()
endfunction()
