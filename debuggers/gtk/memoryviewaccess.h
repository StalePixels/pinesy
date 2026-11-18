// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#ifndef MAME_DEBUGGER_GTK_MEMORYVIEWACCESS_H
#define MAME_DEBUGGER_GTK_MEMORYVIEWACCESS_H

#pragma once

// The memory window's way to MAME's memory debug view.
//
// MAME's dvmemory.h includes softfloat3, which defines float16_t,
// float32_t and float64_t as structs. On arm64, GTK's headers include
// arm_neon.h through graphene, which defines the same names as vector
// element types. The two cannot be in one translation unit, so only
// memoryviewaccess.cpp includes dvmemory.h, and it includes no GTK header.
// This header includes neither.

#include <string>
#include <string_view>


class address_space;
class debug_view;


namespace osd::debugger::gtk::memory_view_access {

// The data formats of a memory view. Each value is the value of the
// debug_view_memory::data_format with the same name.
enum class data_format : int
{
	HEX_8BIT = 1,
	HEX_16BIT = 2,
	HEX_32BIT = 4,
	HEX_64BIT = 8,
	FLOAT_32BIT = 9,
	FLOAT_64BIT = 10,
	FLOAT_80BIT = 11,
	OCTAL_8BIT = 12,
	OCTAL_16BIT = 13,
	OCTAL_32BIT = 14,
	OCTAL_64BIT = 15
};

// true when format is one of the data formats the memory window offers
bool is_data_format(int format);

// Each view argument is a memory debug view (DVT_MEMORY).
int get_data_format(debug_view &view);
void set_data_format(debug_view &view, int format);
bool physical(debug_view &view);
void set_physical(debug_view &view, bool physical);
int address_radix(debug_view &view);
void set_address_radix(debug_view &view, int radix);
bool reverse(debug_view &view);
void set_reverse(debug_view &view, bool reverse);
int chunks_per_row(debug_view &view);
void set_chunks_per_row(debug_view &view, int chunks);
std::string expression(debug_view &view);
void set_expression(debug_view &view, std::string_view expression);

// the index of the view's source for this address space, or -1 when it
// has none
int source_index_for_space(debug_view &view, address_space &space);

// The last known PC that wrote the memory at the cursor.
struct last_pc
{
	bool has_space = false;     // false when the view's source is not an address space
	bool known = false;         // true when text names a PC
	std::string text;           // the text for the context menu
};
last_pc last_pc_at_cursor(debug_view &view);

} // namespace osd::debugger::gtk::memory_view_access

#endif // MAME_DEBUGGER_GTK_MEMORYVIEWACCESS_H
