// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  configuration.cpp - the configuration's command line, its
//  edits, and what the configuration window shows
//
//  A configuration is a MAME command line: a driver, then
//  options. The editor changes that command line; the summary
//  turns MAME's description of it into the rows the
//  configuration window shows.
//
//============================================================

#include "pinesy_core.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace pinesy::core {

namespace {

bool is_option(const std::string &token) { return !token.empty() && token[0] == '-'; }

} // anonymous namespace

//------------------------------------------------------------
//  Arguments
//------------------------------------------------------------

Arguments Arguments::parse(const std::vector<std::string> &array)
{
	Arguments result;
	size_t i = 0;
	if (!array.empty() && !is_option(array[0])) result.driver = array[0];
	if (!result.driver.empty()) i = 1;
	while (i < array.size())
	{
		const std::string &token = array[i++];
		if (!is_option(token))
		{
			result.options.push_back(Option{ "", token, true });
			continue;
		}
		// an option's value never starts with "-": that is the next option
		if (i < array.size() && !is_option(array[i]))
			result.options.push_back(Option{ token.substr(1), array[i++], true });
		else
			result.options.push_back(Option{ token.substr(1), "", false });
	}
	return result;
}

std::vector<std::string> Arguments::array() const
{
	std::vector<std::string> result{ driver };
	for (const Option &o : options)
	{
		if (o.name.empty())
			result.push_back(o.value);
		else if (o.has_value)
		{
			result.push_back("-" + o.name);
			result.push_back(o.value);
		}
		else
			result.push_back("-" + o.name);
	}
	return result;
}

std::optional<std::string> Arguments::get(std::string_view name) const
{
	if (name.empty()) return std::nullopt;
	for (const Option &o : options)
		if (o.name == name && o.has_value) return o.value;
	return std::nullopt;
}

void Arguments::set(const Option &option)
{
	if (!option.name.empty())
		for (Option &o : options)
			if (o.name == option.name) { o = option; return; }
	options.push_back(option);
}

void Arguments::set(const std::string &name, std::optional<std::string> value)
{
	if (value)
	{
		for (Option &o : options)
			if (o.name == name)
			{
				o.value = *value;
				o.has_value = true;
				return;
			}
		options.push_back(Option{ name, *value, true });
	}
	else
		std::erase_if(options, [&](const Option &o) { return o.name == name; });
}

std::string card_choice(const std::string &card, const std::string &bios)
{
	return bios.empty() ? card : card + ",bios=" + bios;
}

//------------------------------------------------------------
//  Editing
//------------------------------------------------------------

namespace editor {

std::vector<std::string> new_configuration(const Catalogue::Machine &machine, const Catalogue &catalogue)
{
	return catalogue.starting_arguments(machine.default_driver());
}

std::vector<std::string> set_driver(const std::vector<std::string> &arguments, const std::string &driver,
		const Catalogue &catalogue, const Describer &describe)
{
	Arguments old = Arguments::parse(arguments);
	if (old.driver == driver) return arguments;
	Arguments old_start = Arguments::parse(catalogue.starting_arguments(old.driver));
	Arguments next = Arguments::parse(catalogue.starting_arguments(driver));
	for (const Arguments::Option &option : old.options)
	{
		if (option.name == "bios") continue;
		// a port the old model left empty only because it starts empty
		// follows the new model's start instead
		if (option.has_value && option.value.empty() && old_start.get(option.name) == std::string()
				&& !next.get(option.name))
			continue;
		next.set(option);
	}
	if (auto bios = old.get("bios"))
		if (const DriverInfo *info = catalogue.driver(driver))
			if (std::any_of(info->biossets.begin(), info->biossets.end(), [&](const BiosSet &b) { return b.name == *bios; }))
				next.set("bios", *bios);
	return normalise(next.array(), describe);
}

std::vector<std::string> set_rom(const std::vector<std::string> &arguments, const std::string &driver,
		const std::optional<std::string> &bios, const Catalogue &catalogue, const Describer &describe)
{
	Arguments args = Arguments::parse(set_driver(arguments, driver, catalogue, describe));
	bool is_default = !bios;
	if (!is_default)
		if (const DriverInfo *info = catalogue.driver(driver))
			if (const BiosSet *set = default_set(info->biossets))
				is_default = set->name == *bios;
	args.set("bios", is_default ? std::nullopt : bios);
	return normalise(args.array(), describe);
}

std::vector<std::string> set_slot(const std::vector<std::string> &arguments, const Slot &slot,
		const std::string &card, const Catalogue &catalogue, const Describer &describe)
{
	Arguments args = Arguments::parse(arguments);
	bool starts_empty = Arguments::parse(catalogue.starting_arguments(args.driver)).get(slot.tag) == std::string();
	if (card == slot.default_card && !starts_empty)
		args.set(slot.tag, std::nullopt);
	else
		args.set(slot.tag, card_choice(card));
	return normalise(args.array(), describe);
}

std::vector<std::string> set_card_rom(const std::vector<std::string> &arguments, const Slot &slot,
		const std::string &bios, const Catalogue &catalogue, const Describer &describe)
{
	Arguments args = Arguments::parse(arguments);
	const BiosSet *set = default_set(slot.biossets.value_or(std::vector<BiosSet>{}));
	bool is_default = set && set->name == bios;
	std::string choice_bios = is_default ? std::string() : bios;
	bool starts_empty = Arguments::parse(catalogue.starting_arguments(args.driver)).get(slot.tag) == std::string();
	if (slot.current == slot.default_card && choice_bios.empty() && !starts_empty)
		args.set(slot.tag, std::nullopt);
	else
		args.set(slot.tag, card_choice(slot.current, choice_bios));
	return normalise(args.array(), describe);
}

std::vector<std::string> set_media(const std::vector<std::string> &arguments, const std::string &media,
		const std::optional<std::string> &path)
{
	Arguments args = Arguments::parse(arguments);
	args.set(media, path);
	return args.array();
}

std::vector<std::string> normalise(const std::vector<std::string> &arguments, const Describer &describe)
{
	// Options are added back in their order, so a card's own slots and
	// media are kept once the card is fitted. When MAME rejects the
	// driver itself, no option can be checked, so the command line
	// stays as it is and the window shows MAME's error.
	Arguments original = Arguments::parse(arguments);
	if (describe(original.array()).is_valid()) return arguments;

	Arguments result(original.driver);
	if (!describe(result.array()).is_valid()) return arguments;
	std::vector<Arguments::Option> pending = original.options;
	bool progress = true;
	while (progress && !pending.empty())
	{
		progress = false;
		const std::vector<Arguments::Option> snapshot = pending;
		for (const Arguments::Option &option : snapshot)
		{
			Arguments trial = result;
			trial.set(option);
			if (describe(trial.array()).is_valid())
			{
				result = trial;
				std::erase(pending, option);
				progress = true;
			}
		}
	}
	return result.array();
}

} // namespace editor

//------------------------------------------------------------
//  The summary
//------------------------------------------------------------

namespace {

std::string base_label(const Slot &slot)
{
	if (slot.kind == "memory") return "Memory";
	if (slot.kind == "sound") return "Sound chip";
	if (slot.kind == "drive") return "Disk drive";
	return slot.typedescription;
}

std::string option_label(const Slot &slot, const SlotOption &option)
{
	return slot.kind == "memory" ? Summary::memory_size(option.name) : option.description;
}

std::vector<Summary::HardwareRow> hardware_rows(const std::vector<Slot> &slots)
{
	std::map<std::string, const Slot *> by_tag;
	for (const Slot &s : slots) by_tag.emplace(s.tag, &s);
	auto depth = [&](const Slot &slot) {
		int count = 0;
		std::string parent = slot.parent;
		while (!parent.empty() && count < 16)
		{
			auto next = by_tag.find(parent);
			if (next == by_tag.end()) break;
			count++;
			parent = next->second->parent;
		}
		return count;
	};

	std::vector<const Slot *> shown;
	for (const Slot &s : slots)
		if (!(s.fixed || s.options.empty()) || !s.current.empty()) shown.push_back(&s);

	std::vector<Summary::HardwareRow> rows;
	for (const Slot *slot : shown)
	{
		std::vector<const Slot *> siblings;
		for (const Slot *s : shown)
			if (s->parent == slot->parent && base_label(*s) == base_label(*slot)) siblings.push_back(s);

		Summary::HardwareRow row;
		row.slot = *slot;
		row.label = base_label(*slot);
		auto index = std::find_if(siblings.begin(), siblings.end(), [&](const Slot *s) { return *s == *slot; });
		if ((siblings.size() > 1 || slot->kind == "drive") && index != siblings.end())
			row.label += " " + std::to_string(index - siblings.begin() + 1);
		row.depth = depth(*slot);

		if (slot->kind != "memory") row.choices.push_back(Summary::Choice{ "", "None" });
		for (const SlotOption &o : slot->options) row.choices.push_back(Summary::Choice{ o.name, option_label(*slot, o) });

		std::string fitted = slot->currentdescription.value_or(slot->current);
		for (const SlotOption &o : slot->options)
			if (o.name == slot->current) { fitted = o.description; break; }
		if (slot->fixed || slot->options.empty()) row.fixed_text = fitted;

		const std::vector<BiosSet> sets = slot->biossets.value_or(std::vector<BiosSet>{});
		if (sets.size() > 1)
			for (const BiosSet &b : sets) row.roms.push_back(Summary::Choice{ b.name, b.description });
		if (slot->bios && !slot->bios->empty())
			row.current_rom = *slot->bios;
		else if (const BiosSet *set = default_set(sets))
			row.current_rom = set->name;

		// Ports of one kind are told apart by the card each was made
		// for, whatever is fitted now.
		if (slot->is_port() && siblings.size() > 1)
			for (const SlotOption &o : slot->options)
				if (o.name == slot->default_card) { row.meant_for = o.description; break; }

		rows.push_back(std::move(row));
	}
	return rows;
}

std::vector<Summary::MediaRow> media_rows(const std::vector<Media> &media, const std::vector<Slot> &slots,
		const std::map<std::string, std::string> &label_by_slot)
{
	std::set<std::string> drives;
	for (const Slot &s : slots)
		if (s.kind == "drive") drives.insert(s.tag);
	auto base = [&](const Media &item) {
		if (!item.owner.partof.empty()) return item.owner.partof;
		if (drives.count(item.owner.slot))
			if (auto label = label_by_slot.find(item.owner.slot); label != label_by_slot.end()) return label->second;
		return item.owner.description;
	};

	std::vector<Summary::MediaRow> rows;
	for (const Media &item : media)
	{
		std::string label = base(item);
		std::vector<const Media *> same;
		for (const Media &m : media)
			if (base(m) == label) same.push_back(&m);
		auto index = std::find_if(same.begin(), same.end(), [&](const Media *m) { return *m == item; });
		if (same.size() > 1 && !drives.count(item.owner.slot) && index != same.end())
			label += " " + std::to_string(index - same.begin() + 1);
		rows.push_back(Summary::MediaRow{ item, label });
	}
	return rows;
}

} // anonymous namespace

std::string Summary::memory_size(const std::string &name)
{
	std::string lower = name;
	for (char &c : lower)
		if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
	auto number = [](const std::string &digits) -> std::optional<long long> {
		if (digits.empty()) return std::nullopt;
		size_t i = (digits[0] == '+' || digits[0] == '-') ? 1 : 0;
		if (i == digits.size()) return std::nullopt;
		for (size_t j = i; j < digits.size(); j++)
			if (digits[j] < '0' || digits[j] > '9') return std::nullopt;
		if (digits.size() > 18) return std::nullopt;
		return std::stoll(digits);
	};
	if (!lower.empty())
	{
		char unit = lower.back();
		auto n = number(lower.substr(0, lower.size() - 1));
		if (n && unit == 'k') return std::to_string(*n) + "K";
		if (n && unit == 'm') return std::to_string(*n) + " MB";
		if (n && unit == 'g') return std::to_string(*n) + " GB";
	}
	return name;
}

Summary::Summary(const std::vector<std::string> &arguments, const MachineDescription &description,
		const Catalogue &catalogue, const std::string &machine_hint)
{
	Arguments args = Arguments::parse(arguments);
	driver = args.driver;
	const Catalogue::Machine *found = catalogue.machine_for_driver(args.driver);
	if (!found) found = catalogue.machine_for_driver(machine_hint);
	if (found) machine = *found;
	const DriverInfo *running = catalogue.driver(args.driver);
	year = running ? running->year : machine ? machine->year : "";
	preliminary = running ? running->status == "preliminary" : machine ? machine->preliminary : false;
	errors = description.errors;
	warnings = description.warnings.value_or("");

	// the models offered, plus the reference model when a document names it
	if (machine)
	{
		models = machine->models;
		for (const auto &other : machine->other_models)
			if (other.driver == args.driver) { models.push_back(other); break; }
	}

	// ROMs: the running model's sets, then the variants'
	bool is_variant = machine && std::any_of(machine->variants.begin(), machine->variants.end(),
			[&](const Catalogue::Variant &v) { return v.driver == args.driver; });
	std::string model_driver = is_variant ? machine->default_driver() : args.driver;
	if (const DriverInfo *info = catalogue.driver(model_driver))
		for (const BiosSet &b : info->biossets) roms.push_back(ROMChoice{ model_driver, b.name, b.description });
	if (machine)
		for (const auto &variant : machine->variants)
		{
			if (variant.biossets.empty())
				variant_roms.push_back(ROMChoice{ variant.driver, std::nullopt, variant.name });
			else
				for (const BiosSet &b : variant.biossets)
					variant_roms.push_back(ROMChoice{ variant.driver, b.name, variant.name + ", " + b.description });
		}
	const std::vector<BiosSet> current_sets = running ? running->biossets : std::vector<BiosSet>{};
	std::optional<std::string> current_bios = args.get("bios");
	if (!current_bios)
		if (const BiosSet *set = default_set(current_sets)) current_bios = set->name;
	for (const auto *list : { &roms, &variant_roms })
	{
		for (const ROMChoice &r : *list)
			if (r.driver == args.driver && (r.bios == current_bios || (!r.bios && current_sets.empty())))
			{
				current_rom = r;
				break;
			}
		if (current_rom) break;
	}

	const std::vector<Slot> slots = description.slots.value_or(std::vector<Slot>{});
	hardware = hardware_rows(slots);
	std::map<std::string, std::string> label_by_slot;
	for (const auto &row : hardware) label_by_slot.emplace(row.slot.tag, row.label);   // the first wins
	media = media_rows(description.media.value_or(std::vector<Media>{}), slots, label_by_slot);
	for (const auto &row : media)
	{
		if (row.media.mandatory && row.media.current.empty()) missing_media.push_back(row.label);
		std::error_code error;
		if (!row.media.current.empty() && !std::filesystem::exists(row.media.current, error))
			missing_files.insert(row.media.name);
		if (row.media.interface == "sdcard" && !row.media.current.empty())
			if (std::string problem = sd_card::problem(row.media.current); !problem.empty())
				sd_card_problems.emplace(row.media.name, problem);
	}
}

namespace sd_card {

bool usable(std::uint64_t blocks)
{
	if (blocks == 0) return false;
	std::uint64_t mantissa = blocks;
	unsigned exponent = 0;
	while (!(mantissa & 1))
	{
		mantissa >>= 1;
		++exponent;
	}
	bool const sd = exponent >= 2 && (mantissa << (exponent > 9 ? exponent - 9 : 0)) <= (std::uint64_t(1) << 12);
	bool const sdhc = exponent >= 10 && (mantissa << (exponent - 10)) <= (std::uint64_t(1) << 16);
	return sd || sdhc;
}

std::string problem(std::uint64_t bytes)
{
	std::uint64_t const blocks = bytes / 512;
	if (usable(blocks)) return {};
	if (blocks > (std::uint64_t(1) << 26))
		return "This image is too big. The emulated SD card takes images up to 32 GiB.";
	return "The emulated SD card cannot use an image of this size. Images up to 32 GiB that are a multiple of 512 KiB work.";
}

std::string problem(const std::string &path)
{
	std::string extension = std::filesystem::path(path).extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	if (extension == ".chd") return {};
	std::error_code error;
	std::uintmax_t const size = std::filesystem::file_size(path, error);
	if (error) return {};
	return problem(std::uint64_t(size));
}

} // namespace sd_card

} // namespace pinesy::core
