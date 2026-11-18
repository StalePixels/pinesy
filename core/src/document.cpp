// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  document.cpp - .pinesy files
//
//  A .pinesy file is a JSON object with four keys: format
//  (the file format version), machine (the driver that stands
//  for the machine in the machine list), arguments (the MAME
//  command line, as an array of strings) and windowSize (0 for
//  fit, else the magnification of the machine window). Other
//  keys are ignored when reading.
//
//============================================================

#include "pinesy_core.hpp"
#include "internal.hpp"

#include <cmath>

namespace pinesy::core {

namespace {

// A whole number that window::is_window_size() takes; anything else,
// or no value, is window::fit.
int read_window_size(const json::Value *value)
{
	try
	{
		if (!value) return window::fit;
		const double size = value->number();
		if (size != std::floor(size) || size < window::fit || size > window::max_magnification) return window::fit;
		return int(size);
	}
	catch (const json::Error &)
	{
		return window::fit;
	}
}

} // anonymous namespace

std::optional<Document> Document::read(std::string_view text)
{
	try
	{
		json::Value root = json::parse(text);
		const json::Value *format = root.find("format");
		const json::Value *machine = root.find("machine");
		const json::Value *arguments = root.find("arguments");
		if (!format || !machine || !arguments) return std::nullopt;
		double version = format->number();
		if (version != std::floor(version)) return std::nullopt;
		Document document;
		document.machine = machine->string();
		document.arguments = encode::to_strings(*arguments);
		if (version > format_version || document.arguments.empty()) return std::nullopt;
		document.window_size = read_window_size(root.find("windowSize"));
		return document;
	}
	catch (const json::Error &)
	{
		return std::nullopt;
	}
}

std::string Document::write() const
{
	return json::write_pretty_sorted(json::Object{
		{ "format", format_version },
		{ "machine", machine },
		{ "arguments", encode::strings(arguments) },
		{ "windowSize", window::is_window_size(window_size) ? window_size : window::fit } });
}

} // namespace pinesy::core
