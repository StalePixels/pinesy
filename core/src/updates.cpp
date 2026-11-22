// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  updates.cpp - the build's identity and the rules of the
//  out-of-date notice
//
//  CI stamps the identity in with the compile definitions
//  PINESY_BUILD_TRACK, PINESY_BUILD_VERSION and
//  PINESY_BUILD_KIND. A local build has none of them.
//
//============================================================

#include "pinesy_core.hpp"

#include <cctype>
#include <cstdlib>

#ifndef PINESY_BUILD_TRACK
#define PINESY_BUILD_TRACK ""
#endif
#ifndef PINESY_BUILD_VERSION
#define PINESY_BUILD_VERSION ""
#endif
#ifndef PINESY_BUILD_KIND
#define PINESY_BUILD_KIND ""
#endif

namespace pinesy::core::updates {

namespace {

// The character types of SUStandardVersionComparator.
enum class Type { number, period, punctuation, whitespace, dash, string };

Type type_of(char c)
{
	const auto u = static_cast<unsigned char>(c);
	if (c == '.') return Type::period;
	if (c == '-') return Type::dash;
	if (std::isdigit(u)) return Type::number;
	if (std::isspace(u)) return Type::whitespace;
	if (std::ispunct(u)) return Type::punctuation;
	return Type::string;
}

Type type_of(const std::string &part)
{
	return part.empty() ? Type::string : type_of(part[0]);
}

// Runs of one character type; each period is a part of its own.
std::vector<std::string> split(std::string_view version)
{
	std::vector<std::string> parts;
	if (version.empty()) return parts;
	std::string part(1, version[0]);
	Type old_type = type_of(version[0]);
	for (std::size_t i = 1; i < version.size(); i++)
	{
		const Type new_type = type_of(version[i]);
		if (old_type != new_type || old_type == Type::period)
		{
			parts.push_back(part);
			part.assign(1, version[i]);
		}
		else
		{
			part += version[i];
		}
		old_type = new_type;
	}
	parts.push_back(part);
	return parts;
}

// The number of number and period parts at the start of a version.
std::size_t leading_numbers(const std::vector<std::string> &parts)
{
	std::size_t n = 0;
	for (const std::string &part : parts)
	{
		const Type t = type_of(part);
		if (t != Type::number && t != Type::period) break;
		n++;
	}
	return n;
}

// Pads the version with fewer leading number parts with ".0" parts, so
// that "1.0" and "1.0.0" are the same version.
void balance(std::vector<std::string> &a, std::vector<std::string> &b)
{
	const std::size_t count_a = leading_numbers(a);
	const std::size_t count_b = leading_numbers(b);
	if (count_a == count_b) return;
	std::vector<std::string> &shorter = count_a < count_b ? a : b;
	const std::vector<std::string> &longer = count_a < count_b ? b : a;
	const std::size_t from = std::min(count_a, count_b);
	const std::size_t to = std::max(count_a, count_b);
	std::vector<std::string> padding;
	for (std::size_t i = from; i < to; i++)
		padding.push_back(type_of(longer[i]) == Type::period ? "." : "0");
	shorter.insert(shorter.begin() + std::ptrdiff_t(from), padding.begin(), padding.end());
}

long long number_of(const std::string &part)
{
	return std::strtoll(part.c_str(), nullptr, 10);
}

} // anonymous namespace

Identity build_identity()
{
	Identity identity{ PINESY_BUILD_TRACK, PINESY_BUILD_VERSION, PINESY_BUILD_KIND };
	return identity.ci_build() ? identity : Identity{};
}

int compare_versions(std::string_view version_a, std::string_view version_b)
{
	std::vector<std::string> a = split(version_a);
	std::vector<std::string> b = split(version_b);
	balance(a, b);

	const std::size_t n = std::min(a.size(), b.size());
	for (std::size_t i = 0; i < n; i++)
	{
		const Type type_a = type_of(a[i]);
		const Type type_b = type_of(b[i]);
		if (type_a == type_b)
		{
			if (type_a == Type::number)
			{
				const long long value_a = number_of(a[i]);
				const long long value_b = number_of(b[i]);
				if (value_a != value_b) return value_a > value_b ? 1 : -1;
			}
			else if (type_a == Type::string)
			{
				const int result = a[i].compare(b[i]);
				if (result != 0) return result > 0 ? 1 : -1;
			}
		}
		else if (type_a != Type::string && type_b == Type::string)
		{
			return 1;
		}
		else if (type_a == Type::string && type_b != Type::string)
		{
			return -1;
		}
		else
		{
			return type_a == Type::number ? 1 : -1;
		}
	}

	if (a.size() != b.size())
	{
		// The longer version's next part decides: a string makes it
		// older (1.0 is newer than 1.0b1), anything else newer.
		const bool a_longer = a.size() > b.size();
		const Type missing = type_of(a_longer ? a[n] : b[n]);
		const int longer_wins = a_longer ? 1 : -1;
		return missing == Type::string ? -longer_wins : longer_wins;
	}
	return 0;
}

bool checks(const Identity &identity, const Settings &settings)
{
	return identity.ci_build() && settings.update_checks;
}

bool notice(const Identity &identity, const Settings &settings, const std::string &build, bool newer_known)
{
	if (!checks(identity, settings) || build.empty() || build == settings.ignored_build) return false;
	return newer_known || compare_versions(build, identity.version) > 0;
}

Settings after_choice(Settings settings, Choice choice, const std::string &build)
{
	switch (choice)
	{
	case Choice::never_remind: settings.update_checks = false; break;
	case Choice::ignore_build: settings.ignored_build = build; break;
	case Choice::update: break;
	}
	return settings;
}

Texts texts(const Identity &identity, const std::string &build, bool newer_known)
{
	Texts t;
	t.title = "Pi/Ne-Sy is out of date";
	t.message = newer_known ? "A newer build of Pi/Ne-Sy is available. You have " + identity.version + "."
	                        : "Pi/Ne-Sy " + build + " is available. You have " + identity.version + ".";
	t.never_remind = "Never Remind Again";
	t.ignore_build = "Never Remind for This Version";
	t.update = "Update";
	t.package_manager = "Update Pi/Ne-Sy with your system's package manager.";
	t.new_permissions = "This update needs new permissions. Update Pi/Ne-Sy with your system's software tools.";
	t.failed = "The update failed.";
	return t;
}

std::string track_url(const Identity &identity)
{
	return std::string(server) + "/" + identity.track + "/";
}

std::string appcast_url(const Identity &identity)
{
	return track_url(identity) + "macos/appcast.xml";
}

std::string debian_architecture()
{
#if defined(__x86_64__)
	return "amd64";
#elif defined(__aarch64__)
	return "arm64";
#else
	return "";
#endif
}

std::string apt_packages_url(const Identity &identity, const std::string &architecture)
{
	return track_url(identity) + "dists/sid/main/binary-" + architecture + "/Packages";
}

std::string apt_newest_version(std::string_view packages, std::string_view package)
{
	// Paragraphs of "Field: value" lines, separated by empty lines.
	std::string newest;
	std::string name, version;
	auto finish = [&]() {
		if (name == package && !version.empty() && (newest.empty() || compare_versions(version, newest) > 0))
			newest = version;
		name.clear();
		version.clear();
	};
	std::size_t start = 0;
	while (start <= packages.size())
	{
		std::size_t end = packages.find('\n', start);
		if (end == std::string_view::npos) end = packages.size();
		std::string_view line = packages.substr(start, end - start);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty())
			finish();
		else if (line.starts_with("Package:"))
			name = std::string(line.substr(8));
		else if (line.starts_with("Version:"))
			version = std::string(line.substr(8));
		auto trim = [](std::string &s) {
			const auto first = s.find_first_not_of(" \t");
			s = first == std::string::npos ? std::string() : s.substr(first, s.find_last_not_of(" \t") - first + 1);
		};
		trim(name);
		trim(version);
		start = end + 1;
	}
	finish();
	return newest;
}

} // namespace pinesy::core::updates
