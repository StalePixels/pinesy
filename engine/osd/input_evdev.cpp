// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  input_evdev.cpp - joysticks and gamepads through Linux evdev
//
//  Reads /dev/input/event* directly, so it needs no library.
//  osd_common_t registers JOYSTICKINPUT_MACGAME first among the
//  joystick modules under OSD_MAC, so this module is defined
//  under that name and is the default joystick provider on
//  Linux. Its provider name is "evdev".
//
//  A device is a joystick when it has absolute X and Y axes and
//  at least one joystick, gamepad or BTN_TRIGGER_HAPPY button,
//  and is not a tablet, touchpad, mouse or accelerometer.
//
//  Items:
//  - absolute axes outside the hat and multi-touch ranges, with
//    X to RZ on MAME's X to RZ axes, throttle and rudder on the
//    two sliders, and the rest on the additional absolute axes
//  - ABS_HAT0X/Y to ABS_HAT3X/Y as four-way hat switches
//  - buttons in key code order from BTN_JOYSTICK to BTN_DIGI
//    and BTN_TRIGGER_HAPPY1 to BTN_TRIGGER_HAPPY40
//
//  The device id is the device's unique id (EVIOCGUNIQ), or its
//  physical path (EVIOCGPHYS) when it has none, or its name.
//  MAME takes new input devices only while the machine starts,
//  so the joysticks are the ones present then. inotify on
//  /dev/input finds device nodes that appear later: a joystick
//  with the id of one that was removed takes its place, with
//  its mappings, and any other is not used.
//
//============================================================

#include "modules/osdmodule.h"
#include "modules/input/input_module.h"

// MAME headers
#include "emu.h"
#include "inpttype.h"

// OSD headers
#include "modules/input/assignmenthelper.h"
#include "modules/input/input_common.h"
#include "osdmac.h"

#include "util/strformat.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>


namespace osd {

namespace {

constexpr char const INPUT_DIR[] = "/dev/input";

// hat switches have four directions, in MAME's item order
enum { HAT_UP, HAT_DOWN, HAT_LEFT, HAT_RIGHT };

constexpr unsigned MAX_HATS = 4;
constexpr unsigned MAX_ASSIGNED_BUTTONS = 16;


//============================================================
//  capability bits
//============================================================

constexpr size_t bits_longs(size_t bits)
{
	return (bits + (8 * sizeof(unsigned long)) - 1) / (8 * sizeof(unsigned long));
}

inline bool test_bit(unsigned bit, unsigned long const *bits)
{
	return (bits[bit / (8 * sizeof(unsigned long))] >> (bit % (8 * sizeof(unsigned long)))) & 1;
}

struct capabilities
{
	unsigned long ev[bits_longs(EV_CNT)] = { };
	unsigned long key[bits_longs(KEY_CNT)] = { };
	unsigned long abs[bits_longs(ABS_CNT)] = { };
	unsigned long prop[bits_longs(INPUT_PROP_CNT)] = { };

	bool read(int fd)
	{
		if (ioctl(fd, EVIOCGBIT(0, sizeof(ev)), ev) < 0)
			return false;
		if (test_bit(EV_KEY, ev))
			ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key)), key);
		if (test_bit(EV_ABS, ev))
			ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs);
		ioctl(fd, EVIOCGPROP(sizeof(prop)), prop);
		return true;
	}
};

// the buttons this module reports
inline bool is_joystick_button(unsigned code)
{
	return ((code >= BTN_JOYSTICK) && (code < BTN_DIGI))
			|| ((code >= BTN_TRIGGER_HAPPY1) && (code <= BTN_TRIGGER_HAPPY40));
}

// the hat axes come in X and Y pairs, ABS_HAT0X to ABS_HAT3Y
inline bool is_hat_axis(unsigned code)
{
	return (code >= ABS_HAT0X) && (code <= ABS_HAT3Y);
}

// absolute axes reported as axes: everything below the
// multi-touch range that is not a hat axis
inline bool is_plain_axis(unsigned code)
{
	return (code < ABS_MT_SLOT) && !is_hat_axis(code);
}

bool looks_like_joystick(capabilities const &caps)
{
	if (!test_bit(EV_KEY, caps.ev) || !test_bit(EV_ABS, caps.ev))
		return false;
	if (!test_bit(ABS_X, caps.abs) || !test_bit(ABS_Y, caps.abs))
		return false;

	// accelerometers, tablets, touchpads and mice also have X and Y
	if (test_bit(INPUT_PROP_ACCELEROMETER, caps.prop))
		return false;
	if (test_bit(BTN_STYLUS, caps.key) || test_bit(BTN_TOOL_PEN, caps.key))
		return false;
	if (test_bit(BTN_TOOL_FINGER, caps.key))
		return false;
	if (test_bit(BTN_MOUSE, caps.key))
		return false;

	for (unsigned code = BTN_JOYSTICK; code < KEY_CNT; code++)
	{
		if (is_joystick_button(code) && test_bit(code, caps.key))
			return true;
	}
	return false;
}

std::string read_string(int fd, unsigned long request)
{
	char buffer[256];
	int const length = ioctl(fd, request, buffer);
	if (length <= 0)
		return std::string();
	buffer[std::min<size_t>(length, sizeof(buffer)) - 1] = '\0';
	return std::string(buffer);
}

char const *axis_name(unsigned code)
{
	switch (code)
	{
	case ABS_X:         return "X";
	case ABS_Y:         return "Y";
	case ABS_Z:         return "Z";
	case ABS_RX:        return "RX";
	case ABS_RY:        return "RY";
	case ABS_RZ:        return "RZ";
	case ABS_THROTTLE:  return "Throttle";
	case ABS_RUDDER:    return "Rudder";
	case ABS_WHEEL:     return "Wheel";
	case ABS_GAS:       return "Gas";
	case ABS_BRAKE:     return "Brake";
	default:            return nullptr;
	}
}

input_item_id axis_item(unsigned code)
{
	switch (code)
	{
	case ABS_X:         return ITEM_ID_XAXIS;
	case ABS_Y:         return ITEM_ID_YAXIS;
	case ABS_Z:         return ITEM_ID_ZAXIS;
	case ABS_RX:        return ITEM_ID_RXAXIS;
	case ABS_RY:        return ITEM_ID_RYAXIS;
	case ABS_RZ:        return ITEM_ID_RZAXIS;
	case ABS_THROTTLE:  return ITEM_ID_SLIDER1;
	case ABS_RUDDER:    return ITEM_ID_SLIDER2;
	default:            return ITEM_ID_INVALID;
	}
}


//============================================================
//  evdev_joystick_device
//============================================================

class evdev_joystick_device : public device_info, protected joystick_assignment_helper
{
public:
	evdev_joystick_device(
			std::string &&name,
			std::string &&id,
			input_module &module,
			int fd,
			std::string const &path,
			capabilities const &caps) :
		device_info(std::move(name), std::move(id), module),
		m_fd(-1),
		m_resync(true),
		m_dropped(false)
	{
		std::fill(std::begin(m_axis_index), std::end(m_axis_index), -1);
		std::fill(std::begin(m_button_index), std::end(m_button_index), -1);

		for (unsigned code = 0; code < ABS_CNT; code++)
		{
			if (!test_bit(code, caps.abs))
				continue;
			if (is_plain_axis(code))
			{
				m_axis_index[code] = int(m_axes.size());
				m_axes.emplace_back(axis_info{ code, 0, 0, 0 });
			}
			else if (is_hat_axis(code))
			{
				m_hats[(code - ABS_HAT0X) / 2].present = true;
			}
		}

		for (unsigned code = BTN_JOYSTICK; code < KEY_CNT; code++)
		{
			if (is_joystick_button(code) && test_bit(code, caps.key))
			{
				m_button_index[code] = int(m_buttons.size());
				m_buttons.emplace_back(button_info{ code, 0 });
			}
		}

		attach(fd, path);
	}

	virtual ~evdev_joystick_device()
	{
		if (m_fd >= 0)
			close(m_fd);
	}

	bool connected() const { return m_fd >= 0; }
	std::string const &path() const { return m_path; }

	unsigned axis_count() const { return m_axes.size(); }
	unsigned button_count() const { return m_buttons.size(); }
	unsigned hat_count() const
	{
		return std::count_if(std::begin(m_hats), std::end(m_hats), [] (hat_info const &hat) { return hat.present; });
	}

	// takes a newly opened device node, for this device or for one
	// that comes back with its id
	void attach(int fd, std::string const &path)
	{
		if (m_fd >= 0)
			close(m_fd);
		m_fd = fd;
		m_path = path;

		for (auto &axis : m_axes)
			read_range(axis.code, axis.min, axis.max);
		for (unsigned hat = 0; hat < MAX_HATS; hat++)
		{
			read_range(ABS_HAT0X + (hat * 2), m_hats[hat].xmin, m_hats[hat].xmax);
			read_range(ABS_HAT0Y + (hat * 2), m_hats[hat].ymin, m_hats[hat].ymax);
		}

		m_resync = true;
	}

	virtual void poll(bool relative_reset) override
	{
		if (m_fd < 0)
			return;

		if (m_resync)
		{
			resync();
			if (m_fd < 0)
				return;
		}

		input_event events[64];
		for (;;)
		{
			ssize_t const length = read(m_fd, events, sizeof(events));
			if (length < 0)
			{
				if (errno == EINTR)
					continue;
				if (errno != EAGAIN)
					disconnect();
				break;
			}
			if (length < ssize_t(sizeof(input_event)))
				break;

			for (size_t i = 0; i < size_t(length) / sizeof(input_event); i++)
				process_event(events[i]);
		}

		// the kernel dropped events: read the whole state again
		if (m_dropped)
			resync();
	}

	virtual void reset() override
	{
		clear_state();

		// state changes while not polling are not seen, so read the
		// whole state on the next poll
		m_resync = true;
	}

	virtual void configure(input_device &device) override
	{
		input_device::assignment_vector assignments;

		// axes, on additional absolute items when the axis has no item
		// of its own or its item is taken
		input_item_id xaxis = ITEM_ID_INVALID;
		input_item_id yaxis = ITEM_ID_INVALID;
		bool used[ITEM_ID_ABSOLUTE_MAXIMUM] = { };
		unsigned additional = 0;
		for (auto &axis : m_axes)
		{
			input_item_id item = axis_item(axis.code);
			if ((ITEM_ID_INVALID == item) || used[item])
			{
				if (additional < INPUT_MAX_ADD_ABSOLUTE)
					item = input_item_id(ITEM_ID_ADD_ABSOLUTE1 + additional++);
				else
					item = ITEM_ID_OTHER_AXIS_ABSOLUTE;
			}
			used[item] = true;

			char const *const name = axis_name(axis.code);
			input_item_id const actual = device.add_item(
					name ? std::string(name) : util::string_format("Axis %u", axis.code),
					std::string_view(),
					item,
					generic_axis_get_state<std::int32_t>,
					&axis.value);

			if (ABS_X == axis.code)
				xaxis = actual;
			else if (ABS_Y == axis.code)
				yaxis = actual;
		}

		// buttons, then additional switches
		input_item_id firstbutton = ITEM_ID_INVALID;
		unsigned buttonnum = 0;
		for (auto &button : m_buttons)
		{
			input_item_id item;
			if (buttonnum < INPUT_MAX_BUTTONS)
				item = input_item_id(ITEM_ID_BUTTON1 + buttonnum);
			else if (buttonnum < (INPUT_MAX_BUTTONS + INPUT_MAX_ADD_SWITCH))
				item = input_item_id(ITEM_ID_ADD_SWITCH1 + buttonnum - INPUT_MAX_BUTTONS);
			else
				item = ITEM_ID_OTHER_SWITCH;

			input_item_id const actual = device.add_item(
					default_button_name(buttonnum),
					std::string_view(),
					item,
					generic_button_get_state<std::uint8_t>,
					&button.value);

			if (ITEM_ID_INVALID == firstbutton)
				firstbutton = actual;

			// there are sixteen action button types
			if (buttonnum < MAX_ASSIGNED_BUTTONS)
				add_button_assignment(assignments, ioport_type(IPT_BUTTON1 + buttonnum), { actual });
			buttonnum++;
		}

		// hats
		input_item_id hat1[4] = { ITEM_ID_INVALID, ITEM_ID_INVALID, ITEM_ID_INVALID, ITEM_ID_INVALID };
		unsigned hatnum = 0;
		for (auto &hat : m_hats)
		{
			if (!hat.present)
				continue;

			static char const *const directions[4] = { "Up", "Down", "Left", "Right" };
			for (int dir = 0; dir < 4; dir++)
			{
				input_item_id const actual = device.add_item(
						util::string_format("Hat %u %s", hatnum + 1, directions[dir]),
						std::string_view(),
						input_item_id(ITEM_ID_HAT1UP + (hatnum * 4) + dir),
						generic_button_get_state<std::uint8_t>,
						&hat.state[dir]);
				if (0 == hatnum)
					hat1[dir] = actual;
			}
			hatnum++;
		}

		// the main stick and the first hat move; the first button
		// selects in the UI
		add_directional_assignments(assignments, xaxis, yaxis, hat1[HAT_LEFT], hat1[HAT_RIGHT], hat1[HAT_UP], hat1[HAT_DOWN]);
		if (ITEM_ID_INVALID != firstbutton)
			add_button_assignment(assignments, IPT_UI_SELECT, { firstbutton });

		device.set_default_assignments(std::move(assignments));
	}

private:
	struct axis_info
	{
		unsigned        code;
		std::int32_t    min, max;
		std::int32_t    value;
	};

	struct button_info
	{
		unsigned        code;
		std::uint8_t    value;
	};

	struct hat_info
	{
		bool            present = false;
		std::int32_t    xmin = -1, xmax = 1;
		std::int32_t    ymin = -1, ymax = 1;
		std::uint8_t    state[4] = { 0, 0, 0, 0 };
	};

	void read_range(unsigned code, std::int32_t &min, std::int32_t &max)
	{
		input_absinfo info;
		if (ioctl(m_fd, EVIOCGABS(code), &info) >= 0)
		{
			min = info.minimum;
			max = info.maximum;
		}
	}

	// a quarter of the range from the centre counts as pressed, so
	// analogue hats need a firm push
	static void set_hat_direction(std::int32_t value, std::int32_t min, std::int32_t max, std::uint8_t &negative, std::uint8_t &positive)
	{
		std::int64_t const centre2 = std::int64_t(min) + max;
		std::int64_t const quarter2 = (std::int64_t(max) - min) / 2;
		std::int64_t const value2 = std::int64_t(value) * 2;
		negative = (value2 < (centre2 - quarter2)) ? 0x80 : 0x00;
		positive = (value2 > (centre2 + quarter2)) ? 0x80 : 0x00;
	}

	void set_abs(unsigned code, std::int32_t value)
	{
		if (is_hat_axis(code))
		{
			hat_info &hat = m_hats[(code - ABS_HAT0X) / 2];
			if ((code - ABS_HAT0X) & 1)
			{
				set_hat_direction(value, hat.ymin, hat.ymax, hat.state[HAT_UP], hat.state[HAT_DOWN]);
			}
			else
			{
				set_hat_direction(value, hat.xmin, hat.xmax, hat.state[HAT_LEFT], hat.state[HAT_RIGHT]);
			}
		}
		else if ((code < ABS_CNT) && (m_axis_index[code] >= 0))
		{
			axis_info &axis = m_axes[m_axis_index[code]];
			axis.value = normalize_absolute_axis(value, axis.min, axis.max);
		}
	}

	void process_event(input_event const &event)
	{
		// after SYN_DROPPED, events up to the next SYN_REPORT are partial
		if (m_dropped)
			return;

		switch (event.type)
		{
		case EV_SYN:
			if (SYN_DROPPED == event.code)
				m_dropped = true;
			break;

		case EV_KEY:
			if ((event.code < KEY_CNT) && (m_button_index[event.code] >= 0))
				m_buttons[m_button_index[event.code]].value = event.value ? 0x80 : 0x00;
			break;

		case EV_ABS:
			if (event.code < ABS_CNT)
				set_abs(event.code, event.value);
			break;
		}
	}

	// discards queued events and reads the current state of every item
	void resync()
	{
		m_resync = false;
		m_dropped = false;

		input_event events[64];
		for (;;)
		{
			ssize_t const length = read(m_fd, events, sizeof(events));
			if ((length < 0) && (errno == EINTR))
				continue;
			if ((length < 0) && (errno != EAGAIN))
			{
				disconnect();
				return;
			}
			if (length <= 0)
				break;
		}

		unsigned long keys[bits_longs(KEY_CNT)] = { };
		if (ioctl(m_fd, EVIOCGKEY(sizeof(keys)), keys) >= 0)
		{
			for (auto &button : m_buttons)
				button.value = test_bit(button.code, keys) ? 0x80 : 0x00;
		}

		input_absinfo info;
		for (auto &axis : m_axes)
		{
			if (ioctl(m_fd, EVIOCGABS(axis.code), &info) >= 0)
				set_abs(axis.code, info.value);
		}
		for (unsigned hat = 0; hat < MAX_HATS; hat++)
		{
			if (!m_hats[hat].present)
				continue;
			if (ioctl(m_fd, EVIOCGABS(ABS_HAT0X + (hat * 2)), &info) >= 0)
				set_abs(ABS_HAT0X + (hat * 2), info.value);
			if (ioctl(m_fd, EVIOCGABS(ABS_HAT0Y + (hat * 2)), &info) >= 0)
				set_abs(ABS_HAT0Y + (hat * 2), info.value);
		}
	}

	void disconnect()
	{
		osd_printf_verbose("evdev: %s [ID %s] disconnected\n", name(), id());
		close(m_fd);
		m_fd = -1;
		clear_state();
	}

	void clear_state()
	{
		for (auto &axis : m_axes)
			axis.value = 0;
		for (auto &button : m_buttons)
			button.value = 0;
		for (auto &hat : m_hats)
			std::fill(std::begin(hat.state), std::end(hat.state), 0);
	}

	int m_fd;
	std::string m_path;
	bool m_resync;
	bool m_dropped;

	std::vector<axis_info> m_axes;
	std::vector<button_info> m_buttons;
	hat_info m_hats[MAX_HATS];

	int m_axis_index[ABS_CNT];
	int m_button_index[KEY_CNT];
};


//============================================================
//  joystick_input_evdev
//============================================================

class joystick_input_evdev : public input_module_impl<evdev_joystick_device, mac_osd_interface>
{
public:
	joystick_input_evdev() :
		input_module_impl<evdev_joystick_device, mac_osd_interface>(OSD_JOYSTICKINPUT_PROVIDER, "evdev"),
		m_inotify(-1),
		m_starting(false)
	{
	}

	virtual void input_init(running_machine &machine) override
	{
		input_module_impl<evdev_joystick_device, mac_osd_interface>::input_init(machine);

		// new nodes are made by the kernel and then given their
		// permissions by udev, so both creation and attribute
		// changes are watched
		m_inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
		if ((m_inotify >= 0) && (inotify_add_watch(m_inotify, INPUT_DIR, IN_CREATE | IN_ATTRIB) < 0))
		{
			close(m_inotify);
			m_inotify = -1;
		}
		if (m_inotify < 0)
			osd_printf_verbose("evdev: cannot watch %s, so only devices present now are used\n", INPUT_DIR);

		// the devices present now, in event number order
		std::vector<int> numbers;
		DIR *const dir = opendir(INPUT_DIR);
		if (dir)
		{
			while (dirent const *const entry = readdir(dir))
			{
				int number;
				if (event_number(entry->d_name, number))
					numbers.push_back(number);
			}
			closedir(dir);
		}
		std::sort(numbers.begin(), numbers.end());
		m_starting = true;
		for (int number : numbers)
			try_add(util::string_format("%s/event%d", INPUT_DIR, number));
		m_starting = false;
	}

	virtual void exit() override
	{
		if (m_inotify >= 0)
		{
			close(m_inotify);
			m_inotify = -1;
		}

		input_module_impl<evdev_joystick_device, mac_osd_interface>::exit();
	}

protected:
	virtual void before_poll() override
	{
		input_module_impl<evdev_joystick_device, mac_osd_interface>::before_poll();

		if (m_inotify < 0)
			return;

		alignas(inotify_event) char buffer[4096];
		for (;;)
		{
			ssize_t const length = read(m_inotify, buffer, sizeof(buffer));
			if (length <= 0)
				break;

			for (ssize_t offset = 0; offset < length; )
			{
				inotify_event const &event = *reinterpret_cast<inotify_event const *>(buffer + offset);
				int number;
				if (event.len && event_number(event.name, number))
					try_add(util::string_format("%s/%s", INPUT_DIR, event.name));
				offset += sizeof(inotify_event) + event.len;
			}
		}
	}

private:
	static bool event_number(char const *name, int &number)
	{
		if (std::strncmp(name, "event", 5) || !name[5])
			return false;
		char *end;
		number = int(std::strtol(name + 5, &end, 10));
		return !*end;
	}

	void try_add(std::string const &path)
	{
		// a node that a connected device already reads
		bool open_already = false;
		devicelist().for_each_device(
				[&path, &open_already] (evdev_joystick_device &device)
				{
					if (device.connected() && (device.path() == path))
						open_already = true;
				});
		if (open_already)
			return;

		int const fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
		{
			if (errno == EACCES)
				osd_printf_verbose("evdev: no permission to read %s\n", path);
			return;
		}

		capabilities caps;
		if (!caps.read(fd) || !looks_like_joystick(caps))
		{
			close(fd);
			return;
		}

		std::string name = read_string(fd, EVIOCGNAME(256));
		if (name.empty())
			name = "Joystick";
		std::string id = read_string(fd, EVIOCGUNIQ(256));
		if (id.empty())
			id = read_string(fd, EVIOCGPHYS(256));
		if (id.empty())
			id = name;

		// a removed device that comes back keeps its place
		evdev_joystick_device *returning = nullptr;
		devicelist().for_each_device(
				[&id, &returning] (evdev_joystick_device &device)
				{
					if (!returning && !device.connected() && (device.id() == id))
						returning = &device;
				});
		if (returning)
		{
			returning->attach(fd, path);
			osd_printf_verbose("evdev: %s [ID %s] reconnected at %s\n", returning->name(), returning->id(), path);
			return;
		}
		if (!m_starting)
		{
			osd_printf_verbose("evdev: %s [ID %s] at %s is not used until the next start\n", name, id, path);
			close(fd);
			return;
		}

		evdev_joystick_device &device = create_device<evdev_joystick_device>(
				DEVICE_CLASS_JOYSTICK,
				std::move(name),
				std::move(id),
				fd,
				path,
				caps);
		osd_printf_verbose(
				"evdev: %s [ID %s] at %s: %u axes, %u buttons, %u hats\n",
				device.name(),
				device.id(),
				path,
				device.axis_count(),
				device.button_count(),
				device.hat_count());
	}

	int m_inotify;
	bool m_starting; // devices can be added to MAME's input manager
};

} // anonymous namespace

} // namespace osd

MODULE_DEFINITION(JOYSTICKINPUT_MACGAME, osd::joystick_input_evdev)
