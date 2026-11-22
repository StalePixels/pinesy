// Pinesy for KDE Plasma. With arguments, it runs MAME headless with that
// command line and exits with MAME's exit code. Without, it starts the
// app.

#include "core.h"
#include "desktopservice.h"
#include "headless.h"
#include "updates.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>

namespace {

// The core needs the resources folder and the platform's default data
// folder, AppDataLocation: ~/.local/share/pinesy. The resources folder
// is an installed copy's own (PINESY_INSTALLED_RESOURCES, relative to
// the program's folder) when it exists, otherwise this build's
// (PINESY_RESOURCES_FOLDER).
void createCore()
{
	QCoreApplication::setApplicationName(QStringLiteral("pinesy"));
	const QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	char *resources = pinesy_core_resources_folder(PINESY_INSTALLED_RESOURCES, PINESY_RESOURCES_FOLDER);
	Core::createInstance(QString::fromUtf8(resources), data);
	pinesy_core_free(resources);
}

// The KDE style when it is installed; Qt's own otherwise.
void chooseStyle(const QQmlEngine &engine)
{
	if (!qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
		return;
	for (const QString &path : engine.importPathList()) {
		if (QDir(path + QStringLiteral("/org/kde/desktop")).exists()) {
			QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
			return;
		}
	}
	QQuickStyle::setStyle(QStringLiteral("Fusion"));
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) {
		createCore();
		const int code = runHeadless(argc, argv);
		Core::destroyInstance();
		return code;
	}

	QApplication app(argc, argv);
	KLocalizedString::setApplicationDomain("pinesy-kde");

	KAboutData about(QStringLiteral("pinesy"), i18n("Pi/Ne-Sy"), QStringLiteral(PINESY_VERSION));
	about.setDesktopFileName(QStringLiteral("com.stalepixels.pinesy.kde"));
	KAboutData::setApplicationData(about);
	QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("com.stalepixels.pinesy.kde")));

	createCore();

	QQmlApplicationEngine engine;
	KLocalization::setupLocalizedContext(&engine);
	chooseStyle(engine);
	engine.loadFromModule("Pinesy", "Main");
	if (engine.rootObjects().isEmpty())
		return 1;

	QObject *root = engine.rootObjects().constFirst();
	DesktopService desktop;
	QObject::connect(&desktop, &DesktopService::activateRequested, root, [root] {
		QMetaObject::invokeMethod(root, "activate");
	});
	QObject::connect(&desktop, &DesktopService::openRequested, root, [root](const QList<QUrl> &urls) {
		for (const QUrl &url : urls)
			QMetaObject::invokeMethod(root, "openFromDesktop", Q_ARG(QVariant, QVariant(url)));
	});
	desktop.registerOnBus(!qEnvironmentVariableIsEmpty(replaceVariable));

	Updates updates;
	QObject::connect(&updates, &Updates::quitRequested, root, [root] {
		QMetaObject::invokeMethod(root, "quit");
	});
	updates.start();

	const int code = app.exec();
	Core::destroyInstance();
	return code;
}
