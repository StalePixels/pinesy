#include "recent.h"

#include "core.h"

#include "pinesy_core.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

KConfigGroup recentGroup()
{
	return KSharedConfig::openConfig(QStringLiteral("pinesyrc"))->group(QStringLiteral("Recent Files"));
}

QByteArray take(char *text)
{
	if (!text)
		return {};
	QByteArray result(text);
	pinesy_core_free(text);
	return result;
}

} // namespace

RecentConfigurations::RecentConfigurations(QObject *parent)
	: QObject(parent)
{
}

RecentConfigurations *RecentConfigurations::instance()
{
	static RecentConfigurations *s_instance = new RecentConfigurations;
	return s_instance;
}

RecentConfigurations *RecentConfigurations::create(QQmlEngine *, QJSEngine *)
{
	RecentConfigurations *recent = instance();
	QJSEngine::setObjectOwnership(recent, QJSEngine::CppOwnership);
	return recent;
}

QStringList RecentConfigurations::paths() const
{
	const KConfigGroup group = recentGroup();
	QStringList result;
	for (int i = 1; i <= PINESY_CORE_RECENT_LIMIT; ++i) {
		const QString path = group.readPathEntry(QStringLiteral("File%1").arg(i), QString());
		if (path.isEmpty())
			break;
		result.append(path);
	}
	return result;
}

void RecentConfigurations::store(const QStringList &paths)
{
	KConfigGroup group = recentGroup();
	group.deleteGroup();
	for (int i = 0; i < paths.size(); ++i) {
		group.writePathEntry(QStringLiteral("File%1").arg(i + 1), paths[i]);
		group.writeEntry(QStringLiteral("Name%1").arg(i + 1), QUrl::fromLocalFile(paths[i]).fileName());
	}
	group.sync();
	Q_EMIT changed();
}

QVariantList RecentConfigurations::entries() const
{
	const QByteArray json = take(pinesy_core_recent_configurations(Core::toJson(paths()).constData()));
	QVariantList result;
	for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
		const QJsonObject entry = value.toObject();
		const QString path = entry.value(QStringLiteral("path")).toString();
		result.append(QVariantMap{
			{ QStringLiteral("url"), QUrl::fromLocalFile(path) },
			{ QStringLiteral("path"), path },
			{ QStringLiteral("name"), entry.value(QStringLiteral("name")).toString() },
			{ QStringLiteral("folder"), entry.value(QStringLiteral("folder")).toString() },
		});
	}
	return result;
}

void RecentConfigurations::note(const QUrl &url)
{
	if (!url.isLocalFile())
		return;
	const QByteArray json = take(pinesy_core_note_recent(Core::toJson(paths()).constData(),
			url.toLocalFile().toUtf8().constData()));
	store(Core::fromJsonList(json.constData()));
}

void RecentConfigurations::clear()
{
	store({});
}

bool RecentConfigurations::launchShowsRecent() const
{
	return pinesy_core_launch(0, int(entries().size())) == PINESY_CORE_LAUNCH_RECENT;
}
