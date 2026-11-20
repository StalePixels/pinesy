#include "desktopservice.h"

#include <QDBusConnection>

namespace {

const QString serviceName = QStringLiteral("com.stalepixels.pinesy.kde");
const QString objectPath = QStringLiteral("/com/stalepixels/pinesy/kde");

// On Wayland the window that comes to the front needs the activation
// token the desktop sent; Qt reads it from the environment.
void takeActivationToken(const QVariantMap &platformData)
{
	const QByteArray token = platformData.value(QStringLiteral("activation-token")).toByteArray();
	if (!token.isEmpty())
		qputenv("XDG_ACTIVATION_TOKEN", token);
}

} // namespace

DesktopService::DesktopService(QObject *parent)
	: QObject(parent)
{
	new DesktopServiceAdaptor(this);
}

bool DesktopService::registerOnBus()
{
	QDBusConnection bus = QDBusConnection::sessionBus();
	return bus.isConnected() && bus.registerObject(objectPath, this, QDBusConnection::ExportAdaptors)
			&& bus.registerService(serviceName);
}

DesktopServiceAdaptor::DesktopServiceAdaptor(DesktopService *service)
	: QDBusAbstractAdaptor(service)
	, m_service(service)
{
}

void DesktopServiceAdaptor::Activate(const QVariantMap &platformData)
{
	takeActivationToken(platformData);
	Q_EMIT m_service->activateRequested();
}

void DesktopServiceAdaptor::Open(const QStringList &uris, const QVariantMap &platformData)
{
	takeActivationToken(platformData);
	Q_EMIT m_service->openRequested(QUrl::fromStringList(uris));
}

// The app has no actions; an action request brings it to the front.
void DesktopServiceAdaptor::ActivateAction(const QString &, const QVariantList &, const QVariantMap &platformData)
{
	takeActivationToken(platformData);
	Q_EMIT m_service->activateRequested();
}
