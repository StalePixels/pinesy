// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  document.cpp - .pinesy files
//
//  A .pinesy file is a JSON object with three keys: format
//  (the file format version), machine (the driver that stands
//  for the machine in the machine list) and arguments (the
//  MAME command line, as an array of strings). Other keys are
//  ignored when reading.
//
//============================================================

#include "pinesy_core.hpp"
#include "internal.hpp"

#include <cmath>

namespace pinesy::core {

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
		{ "arguments", encode::strings(arguments) } });
}

} // namespace pinesy::core
