-- license:BSD-3-Clause
-- copyright-holders:D. Rimron-Soutter

-- pinesy builds the Cocoa OSD's code, so it takes the Cocoa OSD's defines.
-- On Linux it takes OSD_MAC only, for the Cocoa OSD's portable parts, and
-- SDLMAME_LINUX for osdsync.cpp; SDLMAME_UNIX would build the SDL font
-- module.
if _OPTIONS["targetos"]=="macosx" then
	dofile(MAME_DIR .. "scripts/src/osd/mac_cfg.lua")
else
	dofile(MAME_DIR .. "scripts/src/osd/modules.lua")

	if _OPTIONS["USE_TAPTUN"]=="1" or _OPTIONS["USE_PCAP"]=="1" then
		defines {
			"USE_NETWORK",
		}
		if _OPTIONS["USE_TAPTUN"]=="1" then
			defines {
				"OSD_NET_USE_TAPTUN",
			}
		end
		if _OPTIONS["USE_PCAP"]=="1" then
			defines {
				"OSD_NET_USE_PCAP",
			}
		end
	end

	defines {
		"OSD_MAC",
		"SDLMAME_LINUX",
	}
end
