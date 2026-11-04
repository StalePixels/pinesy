// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  internal.hpp - JSON encoding of the core's types, for the
//  C interface
//
//  Field names are MAME's for MAME's data, and the Swift
//  app's for the core's own types.
//
//============================================================

#ifndef PINESY_CORE_INTERNAL_HPP
#define PINESY_CORE_INTERNAL_HPP

#include "pinesy_core.hpp"
#include "json.hpp"

namespace pinesy::core::encode {

json::Value bios_sets(const std::vector<BiosSet> &sets);
json::Value slot(const Slot &slot);
json::Value media(const Media &media);
json::Value strings(const std::vector<std::string> &list);
json::Value model(const Catalogue::Model &model);
json::Value models(const std::vector<Catalogue::Model> &list);
json::Value machine(const Catalogue::Machine &machine);
json::Value machines(const std::vector<Catalogue::Machine> &list);
json::Value summary(const Summary &summary);

// A JSON array of strings. Throws json::Error for anything else.
std::vector<std::string> to_strings(const json::Value &value);

} // namespace pinesy::core::encode

#endif // PINESY_CORE_INTERNAL_HPP
