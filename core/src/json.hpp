// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  json.hpp - a small JSON value, reader and writer
//
//  The core reads the JSON documents the engine returns and
//  writes the JSON it gives the front ends and the .pinesy
//  files. Objects keep their keys in the order they were read
//  or added. Numbers are kept as doubles.
//
//============================================================

#ifndef PINESY_CORE_JSON_HPP
#define PINESY_CORE_JSON_HPP

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace pinesy::core::json {

struct Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

// Thrown when text is not JSON, or a value is not of the type asked for.
struct Error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct Value
{
	std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<Array>, std::shared_ptr<Object>> data;

	Value() : data(nullptr) {}
	Value(std::nullptr_t) : data(nullptr) {}
	Value(bool b) : data(b) {}
	Value(int n) : data(double(n)) {}
	Value(double n) : data(n) {}
	Value(const char *s) : data(std::string(s)) {}
	Value(std::string s) : data(std::move(s)) {}
	Value(Array a) : data(std::make_shared<Array>(std::move(a))) {}
	Value(Object o) : data(std::make_shared<Object>(std::move(o))) {}

	bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
	bool is_object() const { return std::holds_alternative<std::shared_ptr<Object>>(data); }
	bool is_array() const { return std::holds_alternative<std::shared_ptr<Array>>(data); }

	// These throw Error when the value has another type.
	bool boolean() const;
	double number() const;
	const std::string &string() const;
	const Array &array() const;
	const Object &object() const;

	// The member with a key, or null when the object has none.
	// Throws Error when the value is not an object.
	const Value *find(std::string_view key) const;
};

// Parses one JSON document. Throws Error.
Value parse(std::string_view text);

// Compact JSON text.
std::string write(const Value &value);

// JSON text laid out as Swift's JSONEncoder does with prettyPrinted
// and sortedKeys: two-space indents, " : " between key and value,
// object keys in byte order. Slashes are not escaped.
std::string write_pretty_sorted(const Value &value);

} // namespace pinesy::core::json

#endif // PINESY_CORE_JSON_HPP
