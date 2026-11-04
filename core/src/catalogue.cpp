// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  catalogue.cpp - the machines the user can choose
//
//  Built from MAME's driver list by rules that use only MAME's
//  data. No driver is named here.
//
//  - Variants. An unofficial driver (MAME: "an after-market or
//    end-user modification") is a ROM choice of its machine,
//    not a machine. Its machine is its parent; without a
//    parent, the official driver in the same source file with
//    the same devices.
//  - Models. A driver and its clones that share one name
//    before a colon ("ZX Spectrum Next: KS1") are one machine
//    with a choice of model. The parent is MAME's reference
//    for the clones, not a product, so it is not offered as a
//    model; a configuration that names it still opens.
//  - Sections. Machines are grouped by their first maker, and
//    a clone joins its parent's section when they share a
//    maker. Makers with one product (a machine with models
//    counts each model) go into "Other makers". Sections and
//    machines are in year order.
//  - Starting configuration. A port where MAME fits the same
//    card on every model of a machine family is MAME's
//    convenience, not part of any product, so a new
//    configuration leaves that port empty. Where MAME fits
//    different cards on different models, the card records
//    what each model has, and it stays. Memory, sound chips
//    and drives are parts of the machine and keep MAME's
//    setting.
//
//============================================================

#include "pinesy_core.hpp"

#include <algorithm>
#include <tuple>

namespace pinesy::core {

namespace {

// "ZX Spectrum Next" for "ZX Spectrum Next: KS1".
std::optional<std::string> family_name(const std::string &description)
{
	size_t at = description.find(": ");
	if (at == std::string::npos) return std::nullopt;
	return description.substr(0, at);
}

// "KS1" for "ZX Spectrum Next: KS1".
std::string model_name(const std::string &description)
{
	size_t at = description.find(": ");
	if (at == std::string::npos) return description;
	return description.substr(at + 2);
}

bool starts_with(std::string_view text, std::string_view prefix)
{
	return text.substr(0, prefix.size()) == prefix;
}

std::string trim_spaces(std::string_view text)
{
	size_t start = text.find_first_not_of(" \t");
	if (start == std::string_view::npos) return {};
	size_t end = text.find_last_not_of(" \t");
	return std::string(text.substr(start, end - start + 1));
}

std::string lower_ascii(std::string_view text)
{
	std::string out(text);
	for (char &c : out)
		if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
	return out;
}

// The machine an unofficial driver is a ROM choice of.
std::optional<std::string> variant_parent(const DriverInfo &driver, const std::vector<DriverInfo> &list,
		const std::map<std::string, DriverInfo, std::less<>> &by_name)
{
	if (driver.cloneof && by_name.count(*driver.cloneof)) return *driver.cloneof;
	const DriverInfo *match = nullptr;
	int count = 0;
	for (const DriverInfo &d : list)
		if (!d.unofficial && d.sourcefile == driver.sourcefile && d.devices == driver.devices)
		{
			match = &d;
			count++;
		}
	if (count == 1) return match->name;
	return std::nullopt;
}

} // anonymous namespace

//------------------------------------------------------------
//  Machine
//------------------------------------------------------------

const std::string &Catalogue::Machine::default_driver() const
{
	return models.empty() ? id : models.back().driver;
}

std::vector<std::string> Catalogue::Machine::drivers() const
{
	std::vector<std::string> result{ id };
	for (const Model &m : models) result.push_back(m.driver);
	for (const Model &m : other_models) result.push_back(m.driver);
	for (const Variant &v : variants) result.push_back(v.driver);
	return result;
}

const Catalogue::Model *Catalogue::Machine::model(std::string_view driver) const
{
	for (const Model &m : models)
		if (m.driver == driver) return &m;
	for (const Model &m : other_models)
		if (m.driver == driver) return &m;
	return nullptr;
}

//------------------------------------------------------------
//  Building the catalogue
//------------------------------------------------------------

int Catalogue::year_value(std::string_view year)
{
	// A sortable year; unknown digits ("198?") sort after the known ones.
	std::string text(year);
	std::replace(text.begin(), text.end(), '?', '9');
	size_t i = 0;
	bool negative = false;
	if (i < text.size() && (text[i] == '+' || text[i] == '-')) negative = text[i++] == '-';
	if (i == text.size()) return 9999;
	long long value = 0;
	for (; i < text.size(); i++)
	{
		if (text[i] < '0' || text[i] > '9') return 9999;
		value = value * 10 + (text[i] - '0');
		if (value > 0x7fffffffLL) return 9999;
	}
	return int(negative ? -value : value);
}

std::vector<std::string> Catalogue::makers(std::string_view manufacturer)
{
	// The makers named in a manufacturer field, keeping company
	// suffixes with their names: "Scorpion, Ltd." is one maker.
	static const std::set<std::string> suffixes{ "ltd", "ltd.", "inc", "inc.", "co", "co.", "plc", "gmbh", "s.a.", "corp", "corp." };
	std::vector<std::string> result;
	size_t start = 0;
	for (;;)
	{
		size_t at = manufacturer.find(", ", start);
		std::string_view part = manufacturer.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start);
		std::string trimmed = trim_spaces(part);
		if (!result.empty() && suffixes.count(lower_ascii(trimmed)))
			result.back() += ", " + trimmed;
		else if (!trimmed.empty())
			result.push_back(trimmed);
		if (at == std::string_view::npos) break;
		start = at + 2;
	}
	return result;
}

Catalogue::Catalogue(const std::vector<DriverInfo> &list)
{
	for (const DriverInfo &d : list)
		m_drivers.emplace(d.name, d);   // the first of a name wins

	// variants: unofficial drivers become ROM choices of their machine
	std::map<std::string, std::vector<const DriverInfo *>> variants_of;
	std::set<std::string> variant_drivers;
	for (const DriverInfo &d : list)
	{
		if (!d.unofficial) continue;
		if (auto parent = variant_parent(d, list, m_drivers))
		{
			variants_of[*parent].push_back(&d);
			variant_drivers.insert(d.name);
		}
	}

	std::vector<const DriverInfo *> candidates;
	for (const DriverInfo &d : list)
		if (!variant_drivers.count(d.name)) candidates.push_back(&d);

	auto by_year = [](const DriverInfo *a, const DriverInfo *b) { return year_value(a->year) < year_value(b->year); };

	// models: a parent and the clones that share its name before a colon
	std::map<std::string, std::vector<const DriverInfo *>> models_of;
	std::set<std::string> model_drivers;
	for (const DriverInfo *parent : candidates)
	{
		auto family = family_name(parent->description);
		if (!family) continue;
		std::vector<const DriverInfo *> clones;
		for (const DriverInfo *c : candidates)
			if (c->cloneof == parent->name && family_name(c->description) == family) clones.push_back(c);
		if (!clones.empty())
		{
			std::stable_sort(clones.begin(), clones.end(), by_year);
			for (const DriverInfo *c : clones) model_drivers.insert(c->name);
			models_of[parent->name] = std::move(clones);
		}
	}

	for (const DriverInfo *driver : candidates)
	{
		if (model_drivers.count(driver->name)) continue;
		static const std::vector<const DriverInfo *> none;
		auto found = models_of.find(driver->name);
		const auto &clones = found == models_of.end() ? none : found->second;

		Machine m;
		m.id = driver->name;
		m.name = clones.empty() ? driver->description : family_name(driver->description).value_or(driver->description);
		m.manufacturer = driver->manufacturer;
		m.year = clones.empty() ? driver->year : clones.front()->year;
		for (const DriverInfo *c : clones) m.models.push_back(Model{ c->name, model_name(c->description), c->year });
		if (!clones.empty()) m.other_models.push_back(Model{ driver->name, model_name(driver->description), driver->year });
		const DriverInfo *shown = clones.empty() ? driver : clones.back();
		m.preliminary = shown->status == "preliminary";

		std::vector<const DriverInfo *> variants;
		if (auto v = variants_of.find(driver->name); v != variants_of.end()) variants = v->second;
		std::stable_sort(variants.begin(), variants.end(), by_year);
		for (const DriverInfo *v : variants) m.variants.push_back(Variant{ v->name, v->description, v->biossets });

		// a maker MAME does not know ("<unknown>") is not shown, so it is not searched
		std::vector<std::string> words{ m.name };
		if (!starts_with(driver->manufacturer, "<")) words.push_back(driver->manufacturer);
		for (const Model &model : m.models) words.push_back(model.label);
		for (const Variant &v : m.variants) words.push_back(v.name);
		for (size_t i = 0; i < words.size(); i++)
			m.search_text += (i ? " " : "") + words[i];

		m_machines.push_back(std::move(m));
	}

	// sections
	std::map<std::string, std::string> first_maker;
	for (const Machine &m : m_machines)
	{
		auto list = makers(m.manufacturer);
		first_maker[m.id] = list.empty() ? m.manufacturer : list.front();
	}

	// a clone joins its parent's section when the two share a maker
	std::map<std::string, std::string> section_of;
	for (const Machine &m : m_machines)
	{
		std::string section = first_maker[m.id];
		const DriverInfo *info = driver(m.id);
		if (info && info->cloneof)
		{
			auto parent = std::find_if(m_machines.begin(), m_machines.end(), [&](const Machine &p) { return p.id == *info->cloneof; });
			if (parent != m_machines.end())
			{
				auto own = makers(m.manufacturer);
				auto theirs = makers(parent->manufacturer);
				bool shared = std::any_of(own.begin(), own.end(), [&](const std::string &a) {
					return std::find(theirs.begin(), theirs.end(), a) != theirs.end();
				});
				if (shared) section = first_maker[parent->id];
			}
		}
		section_of[m.id] = section;
	}

	// a machine with models counts once for each model
	std::map<std::string, int> products;
	for (const Machine &m : m_machines)
		products[section_of[m.id]] += std::max<int>(1, int(m.models.size()));

	std::map<std::string, std::vector<Machine>> grouped;
	for (const Machine &m : m_machines)
	{
		const std::string &section = section_of[m.id];
		bool other = products[section] < 2 || starts_with(section, "<");
		grouped[other ? other_makers : section].push_back(m);
	}

	for (auto &[name, machines] : grouped)
	{
		std::stable_sort(machines.begin(), machines.end(), [](const Machine &a, const Machine &b) {
			return std::make_tuple(year_value(a.year), std::cref(a.name)) < std::make_tuple(year_value(b.year), std::cref(b.name));
		});
		m_sections.push_back(Section{ name, std::move(machines), name == other_makers });
	}

	auto earliest = [](const Section &s) {
		int year = 9999;
		for (const Machine &m : s.machines) year = std::min(year, year_value(m.year));
		return year;
	};
	std::stable_sort(m_sections.begin(), m_sections.end(), [&](const Section &a, const Section &b) {
		if (a.shows_maker != b.shows_maker) return !a.shows_maker;
		return std::make_tuple(earliest(a), std::cref(a.name)) < std::make_tuple(earliest(b), std::cref(b.name));
	});

	// m_machines is in section order from here on, as machines() gives them
	m_machines.clear();
	for (const Section &s : m_sections)
		for (const Machine &m : s.machines) m_machines.push_back(m);
	for (size_t i = 0; i < m_machines.size(); i++)
		for (const std::string &d : m_machines[i].drivers())
			m_machine_by_driver[d] = i;   // a later machine wins
}

//------------------------------------------------------------
//  Lookups and search
//------------------------------------------------------------

std::vector<Catalogue::Machine> Catalogue::machines() const
{
	return m_machines;
}

const Catalogue::Machine *Catalogue::machine_for_driver(std::string_view driver) const
{
	auto found = m_machine_by_driver.find(driver);
	return found == m_machine_by_driver.end() ? nullptr : &m_machines[found->second];
}

const DriverInfo *Catalogue::driver(std::string_view name) const
{
	auto found = m_drivers.find(name);
	return found == m_drivers.end() ? nullptr : &found->second;
}

std::vector<Catalogue::Machine> Catalogue::search(std::string_view text) const
{
	std::string needle = lower_ascii(trim_spaces(text));
	if (needle.empty()) return {};
	std::vector<Machine> result;
	for (const Machine &m : m_machines)
		if (lower_ascii(m.search_text).find(needle) != std::string::npos) result.push_back(m);
	return result;
}

//------------------------------------------------------------
//  Starting configuration
//------------------------------------------------------------

std::vector<std::string> Catalogue::starting_arguments(const std::string &driver) const
{
	std::vector<std::string> arguments{ driver };
	for (const std::string &tag : convenience_ports(driver))
	{
		arguments.push_back("-" + tag);
		arguments.push_back("");
	}
	return arguments;
}

std::vector<std::string> Catalogue::convenience_ports(const std::string &name) const
{
	const DriverInfo *info = driver(name);
	if (!info) return {};

	// every driver in MAME's clone family of the driver, the driver included
	std::string root = family_root(name);
	std::vector<const DriverInfo *> family;
	for (const auto &[n, d] : m_drivers)
		if (family_root(n) == root) family.push_back(&d);

	std::vector<std::string> result;
	for (const Slot &slot : info->slots)
	{
		if (!slot.is_port() || slot.fixed || slot.default_card.empty()) continue;
		std::vector<std::string> defaults;
		for (const DriverInfo *member : family)
			for (const Slot &s : member->slots)
				if (s.tag == slot.tag) { defaults.push_back(s.default_card); break; }
		std::set<std::string> distinct(defaults.begin(), defaults.end());
		if (defaults.size() >= 2 && distinct.size() == 1) result.push_back(slot.tag);
	}
	return result;
}

std::string Catalogue::family_root(const std::string &driver) const
{
	std::string name = driver;
	std::set<std::string> seen;
	for (;;)
	{
		const DriverInfo *info = this->driver(name);
		if (!info || !info->cloneof || !this->driver(*info->cloneof) || seen.count(*info->cloneof)) break;
		seen.insert(name);
		name = *info->cloneof;
	}
	return name;
}

} // namespace pinesy::core
