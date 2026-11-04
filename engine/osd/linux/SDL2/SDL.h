// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  SDL.h - the four SDL clipboard calls, without SDL
//
//  src/osd/modules/lib/osdlib_unix.cpp is the Linux platform
//  library for every OSD, and it uses SDL only for the
//  clipboard. The pinesy OSD has no SDL, so on Linux it builds
//  that file with this header on its include path. The
//  clipboard is MAME's own, inside the process.
//
//============================================================

#ifndef MAME_OSD_PINESY_LINUX_SDL2_SDL_H
#define MAME_OSD_PINESY_LINUX_SDL2_SDL_H

#include <cstdlib>
#include <cstring>
#include <string>

inline std::string &pinesy_clipboard()
{
	static std::string s_text;
	return s_text;
}

inline bool SDL_HasClipboardText()
{
	return !pinesy_clipboard().empty();
}

inline char *SDL_GetClipboardText()
{
	return strdup(pinesy_clipboard().c_str());
}

inline void SDL_free(void *mem)
{
	free(mem);
}

inline int SDL_SetClipboardText(const char *text)
{
	pinesy_clipboard().assign(text);
	return 0;
}

#endif // MAME_OSD_PINESY_LINUX_SDL2_SDL_H
