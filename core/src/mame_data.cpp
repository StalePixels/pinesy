// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  mame_data.cpp - the JSON documents pinesy_list_machines()
//  and pinesy_describe() return
//
//  A field the type declares as required must be there and
//  have its type, or the whole document does not decode. An
//  optional field may be missing or null.
//
//============================================================

#include "pinesy_core.hpp"
#include "internal.hpp"

#include <algorithm>

namespace pinesy::core {

using json::Value;

namespace {

const Value &required(const Value &object, std::string_view key)
{
	const Value *v = object.find(key);
	if (!v || v->is_null()) throw json::Error("missing " + std::string(key));
	return *v;
}

std::string str(const Value &o, std::string_view key) { return required(o, key).string(); }
bool flag(const Value &o, std::string_view key) { return required(o, key).boolean(); }

std::optional<std::string> optional_str(const Value &o, std::string_view key)
{
	const Value *v = o.find(key);
	if (!v || v->is_null()) return std::nullopt;
	return v->string();
}

BiosSet bios_set(const Value &v)
{
	return BiosSet{ str(v, "name"), str(v, "description"), flag(v, "default") };
}

std::vector<BiosSet> bios_sets(const Value &v)
{
	std::vector<BiosSet> result;
	for (const Value &item : v.array()) result.push_back(bios_set(item));
	return result;
}

std::optional<std::vector<BiosSet>> optional_bios_sets(const Value &o, std::string_view key)
{
	const Value *v = o.find(key);
	if (!v || v->is_null()) return std::nullopt;
	return bios_sets(*v);
}

Slot slot(const Value &v)
{
	Slot s;
	s.tag = str(v, "tag");
	s.parent = str(v, "parent");
	s.kind = str(v, "kind");
	s.type = str(v, "type");
	s.typedescription = str(v, "typedescription");
	s.fixed = flag(v, "fixed");
	s.default_card = str(v, "default");
	s.current = str(v, "current");
	s.currentdescription = optional_str(v, "currentdescription");
	for (const Value &o : required(v, "options").array())
		s.options.push_back(SlotOption{ str(o, "name"), str(o, "description") });
	s.biossets = optional_bios_sets(v, "biossets");
	s.bios = optional_str(v, "bios");
	return s;
}

std::vector<Slot> slots(const Value &v)
{
	std::vector<Slot> result;
	for (const Value &item : v.array()) result.push_back(slot(item));
	return result;
}

Media media(const Value &v)
{
	Media m;
	m.name = str(v, "name");
	m.instance = str(v, "instance");
	m.brief = str(v, "brief");
	m.type_name = str(v, "typename");
	const Value &owner = required(v, "owner");
	m.owner = MediaOwner{ str(owner, "tag"), str(owner, "description"), str(owner, "slot"), str(owner, "partof") };
	m.extensions = str(v, "extensions");
	m.interface = str(v, "interface");
	m.mandatory = flag(v, "mandatory");
	m.current = str(v, "current");
	return m;
}

} // anonymous namespace

const BiosSet *default_set(const std::vector<BiosSet> &sets)
{
	for (const BiosSet &set : sets)
		if (set.is_default) return &set;
	return sets.empty() ? nullptr : &sets.front();
}

std::vector<std::string> Media::extension_list() const
{
	std::vector<std::string> result;
	size_t start = 0;
	while (start <= extensions.size())
	{
		size_t comma = extensions.find(',', start);
		if (comma == std::string::npos) comma = extensions.size();
		if (comma > start) result.push_back(extensions.substr(start, comma - start));
		start = comma + 1;
	}
	// raw hard disk images, which MAME's hard disk devices also open
	if (type_name == "harddisk")
		for (const char *raw : { "img", "raw" })
			if (std::find(result.begin(), result.end(), raw) == result.end()) result.push_back(raw);
	return result;
}

std::vector<DriverInfo> decode_driver_list(std::string_view text)
{
	try
	{
		Value root = json::parse(text);
		std::vector<DriverInfo> list;
		for (const Value &v : root.array())
		{
			DriverInfo d;
			d.name = str(v, "name");
			d.description = str(v, "description");
			d.manufacturer = str(v, "manufacturer");
			d.year = str(v, "year");
			d.cloneof = optional_str(v, "cloneof");
			d.sourcefile = str(v, "sourcefile");
			d.unofficial = flag(v, "unofficial");
			d.status = str(v, "status");
			d.biossets = bios_sets(required(v, "biossets"));
			for (const Value &dev : required(v, "devices").array()) d.devices.push_back(dev.string());
			d.slots = slots(required(v, "slots"));
			list.push_back(std::move(d));
		}
		return list;
	}
	catch (const json::Error &)
	{
		return {};
	}
}

MachineDescription decode_description(std::string_view text)
{
	try
	{
		Value v = json::parse(text);
		MachineDescription d;
		d.errors = str(v, "errors");
		d.driver = optional_str(v, "driver");
		d.bios = optional_str(v, "bios");
		d.biossets = optional_bios_sets(v, "biossets");
		if (const Value *s = v.find("slots"); s && !s->is_null()) d.slots = slots(*s);
		if (const Value *m = v.find("media"); m && !m->is_null())
		{
			std::vector<Media> list;
			for (const Value &item : m->array()) list.push_back(media(item));
			d.media = std::move(list);
		}
		d.warnings = optional_str(v, "warnings");
		if (const Value *mouse = v.find("mouse"); mouse && !mouse->is_null()) d.mouse = mouse->boolean();
		return d;
	}
	catch (const json::Error &)
	{
		MachineDescription d;
		d.errors = "The emulator gave no description of this configuration.";
		return d;
	}
}

//------------------------------------------------------------
//  Encoding, for the C interface
//------------------------------------------------------------

namespace encode {

static Value opt(const std::optional<std::string> &s) { return s ? Value(*s) : Value(); }

Value bios_sets(const std::vector<BiosSet> &sets)
{
	json::Array a;
	for (const BiosSet &b : sets)
		a.push_back(json::Object{ { "name", b.name }, { "description", b.description }, { "default", b.is_default } });
	return a;
}

Value slot(const Slot &s)
{
	json::Array options;
	for (const SlotOption &o : s.options)
		options.push_back(json::Object{ { "name", o.name }, { "description", o.description } });
	return json::Object{
		{ "tag", s.tag }, { "parent", s.parent }, { "kind", s.kind }, { "type", s.type },
		{ "typedescription", s.typedescription }, { "fixed", s.fixed }, { "default", s.default_card },
		{ "current", s.current }, { "currentdescription", opt(s.currentdescription) },
		{ "options", std::move(options) },
		{ "biossets", s.biossets ? bios_sets(*s.biossets) : Value() },
		{ "bios", opt(s.bios) } };
}

Value media(const Media &m)
{
	return json::Object{
		{ "name", m.name }, { "instance", m.instance }, { "brief", m.brief }, { "typename", m.type_name },
		{ "owner", json::Object{ { "tag", m.owner.tag }, { "description", m.owner.description },
				{ "slot", m.owner.slot }, { "partof", m.owner.partof } } },
		{ "extensions", m.extensions }, { "extensionList", strings(m.extension_list()) },
		{ "interface", m.interface }, { "mandatory", m.mandatory },
		{ "current", m.current } };
}

Value strings(const std::vector<std::string> &list)
{
	json::Array a;
	for (const std::string &s : list) a.push_back(s);
	return a;
}

std::vector<std::string> to_strings(const Value &v)
{
	std::vector<std::string> result;
	for (const Value &item : v.array()) result.push_back(item.string());
	return result;
}

Value model(const Catalogue::Model &m)
{
	return json::Object{ { "driver", m.driver }, { "label", m.label }, { "year", m.year } };
}

Value models(const std::vector<Catalogue::Model> &list)
{
	json::Array a;
	for (const auto &m : list) a.push_back(model(m));
	return a;
}

Value machine(const Catalogue::Machine &m)
{
	json::Array variants;
	for (const auto &v : m.variants)
		variants.push_back(json::Object{ { "driver", v.driver }, { "name", v.name }, { "biossets", bios_sets(v.biossets) } });
	return json::Object{
		{ "id", m.id }, { "name", m.name }, { "manufacturer", m.manufacturer }, { "year", m.year },
		{ "preliminary", m.preliminary }, { "defaultDriver", m.default_driver() },
		{ "models", models(m.models) }, { "otherModels", models(m.other_models) },
		{ "variants", std::move(variants) } };
}

Value machines(const std::vector<Catalogue::Machine> &list)
{
	json::Array a;
	for (const auto &m : list) a.push_back(machine(m));
	return a;
}

static Value choices(const std::vector<Summary::Choice> &list)
{
	json::Array a;
	for (const auto &c : list) a.push_back(json::Object{ { "value", c.value }, { "label", c.label } });
	return a;
}

static Value rom_choice(const Summary::ROMChoice &r)
{
	return json::Object{ { "driver", r.driver }, { "bios", opt(r.bios) }, { "label", r.label } };
}

static Value rom_choices(const std::vector<Summary::ROMChoice> &list)
{
	json::Array a;
	for (const auto &r : list) a.push_back(rom_choice(r));
	return a;
}

Value summary(const Summary &s)
{
	json::Array hardware;
	for (const auto &row : s.hardware)
		hardware.push_back(json::Object{
			{ "slot", slot(row.slot) }, { "label", row.label }, { "depth", row.depth },
			{ "choices", choices(row.choices) }, { "fixedText", opt(row.fixed_text) },
			{ "meantFor", opt(row.meant_for) }, { "roms", choices(row.roms) },
			{ "currentROM", row.current_rom } });
	json::Array media_rows;
	for (const auto &row : s.media)
		media_rows.push_back(json::Object{
			{ "media", media(row.media) }, { "label", row.label } });
	json::Array missing_files;
	for (const auto &name : s.missing_files) missing_files.push_back(name);
	json::Object sd_card_problems;
	for (const auto &[name, problem] : s.sd_card_problems) sd_card_problems.emplace_back(name, problem);
	return json::Object{
		{ "machine", s.machine ? machine(*s.machine) : Value() },
		{ "driver", s.driver }, { "year", s.year }, { "preliminary", s.preliminary },
		{ "models", models(s.models) }, { "roms", rom_choices(s.roms) },
		{ "variantROMs", rom_choices(s.variant_roms) },
		{ "currentROM", s.current_rom ? rom_choice(*s.current_rom) : Value() },
		{ "hardware", std::move(hardware) }, { "media", std::move(media_rows) },
		{ "mouse", s.mouse },
		{ "errors", s.errors }, { "warnings", s.warnings },
		{ "missingMedia", strings(s.missing_media) }, { "missingFiles", std::move(missing_files) },
		{ "sdCardProblems", std::move(sd_card_problems) },
		{ "canRun", s.can_run() } };
}

} // namespace encode

} // namespace pinesy::core
