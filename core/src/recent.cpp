// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  recent.cpp - recent configurations
//
//  Which recent .pinesy files a front end shows, the list it
//  keeps, and what a launch opens. The list itself is in the
//  platform's own store.
//
//============================================================

#include "pinesy_core.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace pinesy::core::recent {

std::vector<Entry> entries(const std::vector<std::string> &paths)
{
	std::vector<Entry> result;
	std::vector<std::string> seen;
	for (const std::string &path : paths)
	{
		if (result.size() >= std::size_t(limit)) break;
		if (path.empty() || std::find(seen.begin(), seen.end(), path) != seen.end()) continue;
		seen.push_back(path);
		std::error_code error;
		const std::filesystem::path file(path);
		if (!std::filesystem::is_regular_file(file, error)) continue;
		result.push_back(Entry{ path, file.stem().string(), file.parent_path().string() });
	}
	return result;
}

std::vector<std::string> noted(const std::vector<std::string> &paths, const std::string &path)
{
	std::vector<std::string> result;
	if (!path.empty()) result.push_back(path);
	for (const std::string &p : paths)
	{
		if (result.size() >= std::size_t(limit)) break;
		if (!p.empty() && std::find(result.begin(), result.end(), p) == result.end())
			result.push_back(p);
	}
	return result;
}

Launch launch(bool files_to_open, std::size_t recent_count)
{
	if (files_to_open) return Launch::files;
	return recent_count > 0 ? Launch::recent : Launch::new_configuration;
}

bool restorable(const std::vector<std::string> &arguments)
{
	return !arguments.empty();
}

} // namespace pinesy::core::recent
