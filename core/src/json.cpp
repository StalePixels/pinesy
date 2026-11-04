// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  json.cpp - a small JSON value, reader and writer
//
//============================================================

#include "json.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdio>

namespace pinesy::core::json {

bool Value::boolean() const
{
	if (auto p = std::get_if<bool>(&data)) return *p;
	throw Error("expected a boolean");
}

double Value::number() const
{
	if (auto p = std::get_if<double>(&data)) return *p;
	throw Error("expected a number");
}

const std::string &Value::string() const
{
	if (auto p = std::get_if<std::string>(&data)) return *p;
	throw Error("expected a string");
}

const Array &Value::array() const
{
	if (auto p = std::get_if<std::shared_ptr<Array>>(&data)) return **p;
	throw Error("expected an array");
}

const Object &Value::object() const
{
	if (auto p = std::get_if<std::shared_ptr<Object>>(&data)) return **p;
	throw Error("expected an object");
}

const Value *Value::find(std::string_view key) const
{
	for (const auto &[k, v] : object())
		if (k == key) return &v;
	return nullptr;
}

//------------------------------------------------------------
//  Reading
//------------------------------------------------------------

namespace {

class Parser
{
public:
	explicit Parser(std::string_view text) : m_text(text) {}

	Value document()
	{
		Value v = value(0);
		space();
		if (m_pos != m_text.size()) fail("text after the JSON value");
		return v;
	}

private:
	std::string_view m_text;
	size_t m_pos = 0;

	[[noreturn]] void fail(const char *what) { throw Error(std::string("JSON: ") + what); }

	void space()
	{
		while (m_pos < m_text.size() && (m_text[m_pos] == ' ' || m_text[m_pos] == '\t' || m_text[m_pos] == '\n' || m_text[m_pos] == '\r'))
			m_pos++;
	}

	bool take(std::string_view word)
	{
		if (m_text.substr(m_pos, word.size()) == word) { m_pos += word.size(); return true; }
		return false;
	}

	Value value(int depth)
	{
		if (depth > 256) fail("nested too deeply");
		space();
		if (m_pos >= m_text.size()) fail("unexpected end");
		char c = m_text[m_pos];
		if (c == '{') return object(depth);
		if (c == '[') return array(depth);
		if (c == '"') return Value(string());
		if (take("true")) return Value(true);
		if (take("false")) return Value(false);
		if (take("null")) return Value(nullptr);
		return Value(number());
	}

	Value object(int depth)
	{
		m_pos++;
		Object o;
		space();
		if (m_pos < m_text.size() && m_text[m_pos] == '}') { m_pos++; return Value(std::move(o)); }
		for (;;)
		{
			space();
			if (m_pos >= m_text.size() || m_text[m_pos] != '"') fail("expected a key");
			std::string key = string();
			space();
			if (m_pos >= m_text.size() || m_text[m_pos] != ':') fail("expected ':'");
			m_pos++;
			o.emplace_back(std::move(key), value(depth + 1));
			space();
			if (m_pos < m_text.size() && m_text[m_pos] == ',') { m_pos++; continue; }
			if (m_pos < m_text.size() && m_text[m_pos] == '}') { m_pos++; return Value(std::move(o)); }
			fail("expected ',' or '}'");
		}
	}

	Value array(int depth)
	{
		m_pos++;
		Array a;
		space();
		if (m_pos < m_text.size() && m_text[m_pos] == ']') { m_pos++; return Value(std::move(a)); }
		for (;;)
		{
			a.push_back(value(depth + 1));
			space();
			if (m_pos < m_text.size() && m_text[m_pos] == ',') { m_pos++; continue; }
			if (m_pos < m_text.size() && m_text[m_pos] == ']') { m_pos++; return Value(std::move(a)); }
			fail("expected ',' or ']'");
		}
	}

	unsigned hex4()
	{
		if (m_pos + 4 > m_text.size()) fail("short \\u escape");
		unsigned v = 0;
		for (int i = 0; i < 4; i++)
		{
			char c = m_text[m_pos++];
			v <<= 4;
			if (c >= '0' && c <= '9') v |= unsigned(c - '0');
			else if (c >= 'a' && c <= 'f') v |= unsigned(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v |= unsigned(c - 'A' + 10);
			else fail("bad \\u escape");
		}
		return v;
	}

	static void utf8(std::string &out, unsigned cp)
	{
		if (cp < 0x80) out += char(cp);
		else if (cp < 0x800) { out += char(0xc0 | (cp >> 6)); out += char(0x80 | (cp & 0x3f)); }
		else if (cp < 0x10000) { out += char(0xe0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3f)); out += char(0x80 | (cp & 0x3f)); }
		else { out += char(0xf0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3f)); out += char(0x80 | ((cp >> 6) & 0x3f)); out += char(0x80 | (cp & 0x3f)); }
	}

	std::string string()
	{
		m_pos++;
		std::string out;
		for (;;)
		{
			if (m_pos >= m_text.size()) fail("unterminated string");
			char c = m_text[m_pos++];
			if (c == '"') return out;
			if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
			if (c != '\\') { out += c; continue; }
			if (m_pos >= m_text.size()) fail("unterminated string");
			char e = m_text[m_pos++];
			switch (e)
			{
			case '"': out += '"'; break;
			case '\\': out += '\\'; break;
			case '/': out += '/'; break;
			case 'b': out += '\b'; break;
			case 'f': out += '\f'; break;
			case 'n': out += '\n'; break;
			case 'r': out += '\r'; break;
			case 't': out += '\t'; break;
			case 'u':
			{
				unsigned cp = hex4();
				if (cp >= 0xd800 && cp < 0xdc00 && take("\\u"))
				{
					unsigned low = hex4();
					if (low >= 0xdc00 && low < 0xe000)
						cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
					else
						fail("bad surrogate pair");
				}
				utf8(out, cp);
				break;
			}
			default: fail("bad escape");
			}
		}
	}

	double number()
	{
		size_t start = m_pos;
		if (m_pos < m_text.size() && m_text[m_pos] == '-') m_pos++;
		while (m_pos < m_text.size() && std::string_view("0123456789.eE+-").find(m_text[m_pos]) != std::string_view::npos)
			m_pos++;
		if (start == m_pos) fail("unexpected character");
		std::string s(m_text.substr(start, m_pos - start));
		char *end = nullptr;
		double v = std::strtod(s.c_str(), &end);
		if (!end || *end) fail("bad number");
		return v;
	}
};

void write_string(std::string &out, const std::string &s)
{
	out += '"';
	for (char c : s)
	{
		unsigned char u = static_cast<unsigned char>(c);
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		default:
			if (u < 0x20)
			{
				char buf[8];
				std::snprintf(buf, sizeof buf, "\\u%04x", u);
				out += buf;
			}
			else
				out += c;
		}
	}
	out += '"';
}

void write_number(std::string &out, double n)
{
	if (std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 9.0e15)
	{
		out += std::to_string(static_cast<long long>(n));
		return;
	}
	char buf[32];
	std::snprintf(buf, sizeof buf, "%.17g", n);
	out += buf;
}

void write_value(std::string &out, const Value &v, bool pretty, int indent)
{
	auto newline = [&](int level) {
		if (!pretty) return;
		out += '\n';
		out.append(size_t(level) * 2, ' ');
	};

	if (v.is_null()) { out += "null"; return; }
	if (auto b = std::get_if<bool>(&v.data)) { out += *b ? "true" : "false"; return; }
	if (auto n = std::get_if<double>(&v.data)) { write_number(out, *n); return; }
	if (auto s = std::get_if<std::string>(&v.data)) { write_string(out, *s); return; }
	if (v.is_array())
	{
		const Array &a = v.array();
		out += '[';
		for (size_t i = 0; i < a.size(); i++)
		{
			if (i) out += ',';
			newline(indent + 1);
			write_value(out, a[i], pretty, indent + 1);
		}
		if (!a.empty()) newline(indent);
		out += ']';
		return;
	}
	const Object &o = v.object();
	std::vector<const std::pair<std::string, Value> *> members;
	for (const auto &m : o) members.push_back(&m);
	if (pretty)
		std::stable_sort(members.begin(), members.end(), [](auto a, auto b) { return a->first < b->first; });
	out += '{';
	for (size_t i = 0; i < members.size(); i++)
	{
		if (i) out += ',';
		newline(indent + 1);
		write_string(out, members[i]->first);
		out += pretty ? " : " : ":";
		write_value(out, members[i]->second, pretty, indent + 1);
	}
	if (!members.empty()) newline(indent);
	out += '}';
}

} // anonymous namespace

Value parse(std::string_view text)
{
	return Parser(text).document();
}

std::string write(const Value &value)
{
	std::string out;
	write_value(out, value, false, 0);
	return out;
}

std::string write_pretty_sorted(const Value &value)
{
	std::string out;
	write_value(out, value, true, 0);
	return out;
}

} // namespace pinesy::core::json
