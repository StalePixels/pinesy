-- license:BSD-3-Clause
-- copyright-holders:D. Rimron-Soutter

---------------------------------------------------------------------------
--
--   pinesy.lua
--
--   Rules for building MAME as a library for a native host
--
--   MAME's scripts/src/osd/pinesy.lua loads this file for OSD=pinesy.
--   MAME_DIR is the MAME tree, so ../engine and ../debuggers are
--   folders next to it.
--
--   The pinesy OSD is the Cocoa OSD (src/osd/mac) with its window
--   layer replaced: by osd/hostview.mm on macOS, which draws on a
--   view the host owns, and by osd/hostlinux.cpp on Linux, which
--   hands each frame to a GTK or Qt host. The result is
--   libmame_pinesy.dylib or libmame_pinesy.so, with the C interface
--   in include/pinesy.h. On Linux, USE_QTDEBUG=1 builds MAME's
--   Qt debugger in, and the library is libmame_pinesy_qt.so. With
--   PINESY_DEBUG_GTK=1 in the environment, the GTK debugger in
--   ../debuggers/gtk is built in instead, and the library is
--   libmame_pinesy_gtk.so.
--
---------------------------------------------------------------------------

dofile(MAME_DIR .. "scripts/src/osd/modules.lua")

-- the host owns the view, so there is no OpenGL window for the
-- OpenGL renderer to draw in; frames are software-rendered
_OPTIONS["NO_OPENGL"] = "1"

PINESY_MACOS = _OPTIONS["targetos"]=="macosx"

-- the makefile passes no option of ours to GENie, so the GTK debugger
-- build is chosen by an environment variable
PINESY_DEBUG_GTK = not PINESY_MACOS and os.getenv("PINESY_DEBUG_GTK")=="1"


function maintargetosdoptions(_target,_subtarget)
	osdmodulestargetconf()

	kind "SharedLib"

	-- on Linux the libraries with the Qt or GTK debugger have their own
	-- names, so they can sit next to the one without either
	local libname = "mame_pinesy"
	if PINESY_DEBUG_GTK then
		libname = "mame_pinesy_gtk"
	elseif not PINESY_MACOS and _OPTIONS["USE_QTDEBUG"]=="1" then
		libname = "mame_pinesy_qt"
	end
	targetname(libname)

	-- nothing inside MAME calls the C interface, so name each function
	-- to keep the linker from leaving pinesymain.o out of the archive
	local exports = { "run", "request_exit", "reset", "key", "char", "list_machines", "describe" }

	if PINESY_MACOS then
		targetextension ".dylib"
		linkoptions {
			"-install_name @rpath/libmame_pinesy.dylib",
		}
		for _, fn in ipairs(exports) do
			linkoptions {
				"-Wl,-u,_pinesy_" .. fn,
			}
		end
	else
		targetextension ".so"
		targetprefix "lib"
		table.insert(exports, "view_size")
		table.insert(exports, "focus")
		linkoptions {
			"-Wl,-soname,lib" .. libname .. ".so",
			"-Wl,-z,defs",
		}
		-- libfdt is in QEMU's deps archive, which the link reads before
		-- the osd library; only dtoverlay.c there uses fdt_empty_tree.o
		linkoptions {
			"-Wl,-u,fdt_create_empty_tree",
		}
		for _, fn in ipairs(exports) do
			linkoptions {
				"-Wl,-u,pinesy_" .. fn,
			}
		end
		links {
			"m",
			"pthread",
			"util",
			"dl",
		}
		if PINESY_DEBUG_GTK then
			linkoptions {
				backtick(pkgconfigcmd() .. " --libs gtk4"),
			}
		end
	end

	configuration { }
end

BASE_TARGETOS       = "unix"

if PINESY_MACOS then
	links {
		"Cocoa.framework",
		"IOKit.framework"
	}
	linkoptions {
		"-framework QuartzCore",
		"-framework GameController",
		"-weak_framework Metal",
	}
end

project ("qtdbg_" .. _OPTIONS["osd"])
	uuid (os.uuid("qtdbg_" .. _OPTIONS["osd"]))
	kind (LIBTYPE)

	dofile(MAME_DIR .. "../engine/pinesy_cfg.lua")
	includedirs {
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices", -- accessing imagedev from debugger
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "src/osd/modules/render",
		MAME_DIR .. "3rdparty",
	}

	qtdebuggerbuild()

project ("osd_" .. _OPTIONS["osd"])
	uuid (os.uuid("osd_" .. _OPTIONS["osd"]))
	kind (LIBTYPE)

	dofile(MAME_DIR .. "../engine/pinesy_cfg.lua")
	osdmodulesbuild()

	includedirs {
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices", -- accessing imagedev from debugger
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "src/osd/modules/file",
		MAME_DIR .. "src/osd/modules/render",
		MAME_DIR .. "3rdparty",
		MAME_DIR .. "src/osd/mac",
		MAME_DIR .. "../engine/include",
	}

	if PINESY_MACOS then
	files {
		MAME_DIR .. "src/osd/modules/debugger/debugosx.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/breakpointsview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/breakpointsview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/consoleview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/consoleview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugcommandhistory.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugcommandhistory.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugconsole.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugconsole.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugwindowhandler.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugwindowhandler.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/deviceinfoviewer.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/deviceinfoviewer.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/devicesviewer.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/devicesviewer.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/disassemblyview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/disassemblyviewer.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/disassemblyviewer.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/errorlogview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/errorlogview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/disassemblyview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/errorlogviewer.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/errorlogviewer.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/exceptionpointsview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/exceptionpointsview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/memoryview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/memoryview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/memoryviewer.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/memoryviewer.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/pointsviewer.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/pointsviewer.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/registerpointsview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/registerpointsview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/registersview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/registersview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/watchpointsview.mm",
		MAME_DIR .. "src/osd/modules/debugger/osx/watchpointsview.h",
		MAME_DIR .. "src/osd/modules/debugger/osx/debugosx.h",
	}
	end

	-- the Cocoa OSD, without its own windows and application shell
	files {
		MAME_DIR .. "src/osd/mac/video.cpp",
		MAME_DIR .. "src/osd/mac/window.cpp",
		MAME_DIR .. "src/osd/mac/window.h",
		MAME_DIR .. "src/osd/modules/render/drawmacsoft.cpp",
		MAME_DIR .. "src/osd/modules/osdwindow.cpp",
		MAME_DIR .. "src/osd/modules/osdwindow.h",
	}

	files {
		MAME_DIR .. "../engine/include/pinesy.h",
		MAME_DIR .. "../engine/osd/pinesymain.cpp",
		MAME_DIR .. "../engine/osd/pinesyinfo.cpp",
		MAME_DIR .. "../engine/osd/hostmonitor.cpp",
		MAME_DIR .. "../engine/osd/hostmonitor.h",
	}

	-- Raspberry Pi's dtoverlay library, for the nextpi card's config.txt
	-- overlays and parameters; libfdt comes with QEMU
	includedirs {
		MAME_DIR .. "../qemu/subprojects/dtc/libfdt",
	}
	files {
		MAME_DIR .. "../vendor/rpi-utils/dtmerge/dtoverlay.c",
		MAME_DIR .. "../vendor/rpi-utils/dtmerge/dtoverlay.h",
	}
	-- MAME's warnings are errors; these two fire in dtoverlay.c and libfdt.h
	buildoptions_c {
		"-Wno-bad-function-cast",
		"-Wno-pointer-arith",
	}

	-- hostmonitor.cpp defines MONITOR_MAC
	removefiles {
		MAME_DIR .. "src/osd/modules/monitor/monitor_mac.cpp",
	}

	if PINESY_MACOS then
		files {
			MAME_DIR .. "src/osd/mac/macmain.cpp",
			MAME_DIR .. "src/osd/modules/input/input_macgame.mm",
			MAME_DIR .. "src/osd/modules/input/input_macjoy.cpp",
			MAME_DIR .. "../engine/osd/hostview.mm",
		}
	else
		-- Linux leaves for the Cocoa OSD's platform calls and modules
		files {
			MAME_DIR .. "../engine/osd/hostlinux.cpp",
			MAME_DIR .. "../engine/osd/input_evdev.cpp",
			MAME_DIR .. "../engine/osd/linuxmain.cpp",
			MAME_DIR .. "../engine/osd/linuxmodules.cpp",
		}
	end

	if PINESY_DEBUG_GTK then
		defines {
			"PINESY_DEBUG_GTK",
		}
		buildoptions {
			backtick(pkgconfigcmd() .. " --cflags gtk4"),
		}
		files {
			MAME_DIR .. "../debuggers/gtk/breakpointswindow.cpp",
			MAME_DIR .. "../debuggers/gtk/breakpointswindow.h",
			MAME_DIR .. "../debuggers/gtk/dasmwindow.cpp",
			MAME_DIR .. "../debuggers/gtk/dasmwindow.h",
			MAME_DIR .. "../debuggers/gtk/debuggerview.cpp",
			MAME_DIR .. "../debuggers/gtk/debuggerview.h",
			MAME_DIR .. "../debuggers/gtk/debuggtk.cpp",
			MAME_DIR .. "../debuggers/gtk/deviceinformationwindow.cpp",
			MAME_DIR .. "../debuggers/gtk/deviceinformationwindow.h",
			MAME_DIR .. "../debuggers/gtk/deviceswindow.cpp",
			MAME_DIR .. "../debuggers/gtk/deviceswindow.h",
			MAME_DIR .. "../debuggers/gtk/logwindow.cpp",
			MAME_DIR .. "../debuggers/gtk/logwindow.h",
			MAME_DIR .. "../debuggers/gtk/mainwindow.cpp",
			MAME_DIR .. "../debuggers/gtk/mainwindow.h",
			MAME_DIR .. "../debuggers/gtk/memoryviewaccess.cpp",
			MAME_DIR .. "../debuggers/gtk/memoryviewaccess.h",
			MAME_DIR .. "../debuggers/gtk/memorywindow.cpp",
			MAME_DIR .. "../debuggers/gtk/memorywindow.h",
			MAME_DIR .. "../debuggers/gtk/windowgtk.cpp",
			MAME_DIR .. "../debuggers/gtk/windowgtk.h",
		}
	end

	-- pinesyinfo.cpp reads the ini files as the frontend does
	includedirs {
		MAME_DIR .. "src/frontend/mame",
	}


project ("ocore_" .. _OPTIONS["osd"])
	uuid (os.uuid("ocore_" .. _OPTIONS["osd"]))
	kind (LIBTYPE)

	removeflags {
		"SingleOutputDir",
	}

	dofile(MAME_DIR .. "../engine/pinesy_cfg.lua")

	includedirs {
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "src/osd/mac",
		ext_includedir("asio"),
	}

	files {
		MAME_DIR .. "src/osd/asio.cpp",
		MAME_DIR .. "src/osd/asio.h",
		MAME_DIR .. "src/osd/osdcore.cpp",
		MAME_DIR .. "src/osd/osdcore.h",
		MAME_DIR .. "src/osd/osdfile.h",
		MAME_DIR .. "src/osd/strconv.cpp",
		MAME_DIR .. "src/osd/strconv.h",
		MAME_DIR .. "src/osd/osdsync.cpp",
		MAME_DIR .. "src/osd/osdsync.h",
		MAME_DIR .. "src/osd/modules/osdmodule.cpp",
		MAME_DIR .. "src/osd/modules/osdmodule.h",
		MAME_DIR .. "src/osd/modules/lib/osdlib.h",
		MAME_DIR .. "src/osd/modules/file/posixdir.cpp",
		MAME_DIR .. "src/osd/modules/file/posixfile.cpp",
		MAME_DIR .. "src/osd/modules/file/posixfile.h",
		MAME_DIR .. "src/osd/modules/file/posixptty.cpp",
		MAME_DIR .. "src/osd/modules/file/posixsocket.cpp",
	}

	if PINESY_MACOS then
		files {
			MAME_DIR .. "src/osd/modules/lib/osdlib_macosx.cpp",
		}
	else
		-- osdlib_unix.cpp takes SDL's clipboard calls from a stand-in
		files {
			MAME_DIR .. "src/osd/modules/lib/osdlib_unix.cpp",
		}
		includedirs {
			MAME_DIR .. "../engine/osd/linux",
		}
	end
