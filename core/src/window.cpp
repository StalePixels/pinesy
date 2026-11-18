// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  window.cpp - the machine window's size
//
//  The engine renders each frame at whole multiples of the
//  machine's pixels, in device pixels. The window shows the
//  frame at a further whole multiple, so every frame pixel
//  covers the same number of screen pixels, or in full screen,
//  where the host draws the frame at its own shape.
//
//============================================================

#include "pinesy_core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pinesy::core::window {

bool is_window_size(int window_size)
{
	return window_size >= full_screen;
}

int choice(int window_size, bool custom_chosen)
{
	if (!is_window_size(window_size)) return fit;
	const int first_custom = custom_chosen ? 1 : listed_magnifications + 1;
	return window_size >= first_custom ? custom : window_size;
}

int size_for_choice(int choice, int window_size)
{
	if (choice == custom) return window_size >= 1 ? window_size : 1;
	return choice >= full_screen && choice <= listed_magnifications ? choice : fit;
}

bool opens_full_screen(int window_size)
{
	return window_size == full_screen;
}

ContentSize content_size(int frame_width, int frame_height, int window_size, double pixel_ratio,
		int usable_width, int usable_height)
{
	if (frame_width <= 0 || frame_height <= 0 || !(pixel_ratio > 0.0))
		return {};

	// a magnification too big for an int size is cut to the largest int
	auto logical = [&](int frame, double device_scale) {
		const double size = std::round(frame * device_scale / pixel_ratio);
		return size >= double(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : std::max(1, int(size));
	};
	auto size_for = [&](double device_scale, int multiple) {
		return ContentSize{ logical(frame_width, device_scale), logical(frame_height, device_scale), multiple };
	};

	const bool fitting = window_size <= fit;
	if (usable_width <= 0 || usable_height <= 0)
	{
		const int multiple = fitting ? 1 : window_size;
		return size_for(multiple, multiple);
	}

	// the room in device pixels, and how many frames fit across and down;
	// the small allowance keeps an exact fit from rounding down
	const double room_width = usable_width * pixel_ratio;
	const double room_height = usable_height * pixel_ratio;
	const int largest = int(std::floor(std::min(room_width / frame_width, room_height / frame_height) + 1e-9));
	if (largest < 1)
		return size_for(std::min(room_width / frame_width, room_height / frame_height), 0);

	const int multiple = fitting ? largest : std::min(window_size, largest);
	return size_for(multiple, multiple);
}

} // namespace pinesy::core::window
