// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, D. Rimron-Soutter
#include "emu.h"
#include "memoryviewaccess.h"

#include "debug/debugcpu.h"
#include "debug/dvmemory.h"


namespace osd::debugger::gtk::memory_view_access {

namespace {

using mame_format = debug_view_memory::data_format;

static_assert(int(data_format::HEX_8BIT) == int(mame_format::HEX_8BIT));
static_assert(int(data_format::HEX_16BIT) == int(mame_format::HEX_16BIT));
static_assert(int(data_format::HEX_32BIT) == int(mame_format::HEX_32BIT));
static_assert(int(data_format::HEX_64BIT) == int(mame_format::HEX_64BIT));
static_assert(int(data_format::FLOAT_32BIT) == int(mame_format::FLOAT_32BIT));
static_assert(int(data_format::FLOAT_64BIT) == int(mame_format::FLOAT_64BIT));
static_assert(int(data_format::FLOAT_80BIT) == int(mame_format::FLOAT_80BIT));
static_assert(int(data_format::OCTAL_8BIT) == int(mame_format::OCTAL_8BIT));
static_assert(int(data_format::OCTAL_16BIT) == int(mame_format::OCTAL_16BIT));
static_assert(int(data_format::OCTAL_32BIT) == int(mame_format::OCTAL_32BIT));
static_assert(int(data_format::OCTAL_64BIT) == int(mame_format::OCTAL_64BIT));

debug_view_memory &memory(debug_view &view)
{
	return downcast<debug_view_memory &>(view);
}

} // anonymous namespace


bool is_data_format(int format)
{
	switch (mame_format(format))
	{
	case mame_format::HEX_8BIT:
	case mame_format::HEX_16BIT:
	case mame_format::HEX_32BIT:
	case mame_format::HEX_64BIT:
	case mame_format::OCTAL_8BIT:
	case mame_format::OCTAL_16BIT:
	case mame_format::OCTAL_32BIT:
	case mame_format::OCTAL_64BIT:
	case mame_format::FLOAT_32BIT:
	case mame_format::FLOAT_64BIT:
	case mame_format::FLOAT_80BIT:
		return true;
	default:
		return false;
	}
}


int get_data_format(debug_view &view) { return int(memory(view).get_data_format()); }
void set_data_format(debug_view &view, int format) { memory(view).set_data_format(mame_format(format)); }
bool physical(debug_view &view) { return memory(view).physical(); }
void set_physical(debug_view &view, bool physical) { memory(view).set_physical(physical); }
int address_radix(debug_view &view) { return memory(view).address_radix(); }
void set_address_radix(debug_view &view, int radix) { memory(view).set_address_radix(radix); }
bool reverse(debug_view &view) { return memory(view).reverse(); }
void set_reverse(debug_view &view, bool reverse) { memory(view).set_reverse(reverse); }
int chunks_per_row(debug_view &view) { return memory(view).chunks_per_row(); }
void set_chunks_per_row(debug_view &view, int chunks) { memory(view).set_chunks_per_row(chunks); }
std::string expression(debug_view &view) { return memory(view).expression(); }
void set_expression(debug_view &view, std::string_view expression) { memory(view).set_expression(std::string(expression)); }


int source_index_for_space(debug_view &view, address_space &space)
{
	int i = 0;
	for (auto &ptr : view.source_list())
	{
		auto const *const source = downcast<debug_view_memory_source const *>(ptr.get());
		auto const [mintf, spacenum] = source->space();
		assert(!mintf || ((0 <= spacenum) && mintf->has_space(spacenum)));
		if (mintf && (&mintf->space(spacenum) == &space))
			return i;
		++i;
	}
	return -1;
}


last_pc last_pc_at_cursor(debug_view &view)
{
	last_pc result;
	debug_view_memory &mem_view = memory(view);
	debug_view_memory_source const &source = downcast<debug_view_memory_source const &>(*mem_view.source());
	auto const [mintf, spacenum] = source.space();
	assert(!mintf || ((0 <= spacenum) && mintf->has_space(spacenum)));
	if (!mintf)
		return result;

	result.has_space = true;
	const address_space_config *config = mintf->logical_space_config(spacenum);
	debug_view_xy const pos = mem_view.cursor_position();
	offs_t const address = config->byte2addr(mem_view.addressAtCursorPosition(pos));
	offs_t a = address & config->logaddrmask();
	address_space *tspace;
	if (!mintf->translate(spacenum, device_memory_interface::TR_READ, a, tspace))
	{
		result.text = "Bad address";
		return result;
	}

	uint64_t mem_value = tspace->unmap();
	auto dis = tspace->device().machine().disable_side_effects();
	switch (tspace->data_width())
	{
	case  8: mem_value = tspace->read_byte(a);            break;
	case 16: mem_value = tspace->read_word_unaligned(a);  break;
	case 32: mem_value = tspace->read_dword_unaligned(a); break;
	case 64: mem_value = tspace->read_qword_unaligned(a); break;
	}

	offs_t const pc = source.device()->debug()->track_mem_pc_from_space_address_data(
			tspace->spacenum(),
			address,
			mem_value);
	if (pc != offs_t(-1))
	{
		if (tspace->is_octal())
			result.text = util::string_format("Address %2o written at PC=%2o", address, pc);
		else
			result.text = util::string_format("Address %2x written at PC=%2x", address, pc);
		result.known = true;
	}
	else
	{
		result.text = "Unknown PC";
	}
	return result;
}

} // namespace osd::debugger::gtk::memory_view_access
