#include "headless.h"

#include "core.h"
#include "runwindow.h"

#include <QGuiApplication>

#include <vector>

int runHeadless(int argc, char **argv)
{
	std::vector<const char *> arguments(argv + 1, argv + argc);
	const int count = int(arguments.size());
	const char *const *values = arguments.data();

	// With -video none MAME needs no window, and no toolkit at all.
	if (pinesy_core_video_none(count, values))
		return pinesy_core_headless(Core::instance()->handle(), count, values, nullptr);

	// The machine gets its own window; closing it stops the machine. Qt
	// sees only the program name, so none of MAME's options reach it.
	int qtArgc = 1;
	QGuiApplication app(qtArgc, argv);
	RunWindow window;
	QString title = QStringLiteral("Pinesy");
	for (const char *argument : arguments) {
		if (argument[0] != '-') {
			title = QString::fromUtf8(argument);
			break;
		}
	}
	window.setTitle(title);
	window.show();
	window.requestActivate();
	window.prepare();
	return pinesy_core_headless(Core::instance()->handle(), count, values, window.host());
}
