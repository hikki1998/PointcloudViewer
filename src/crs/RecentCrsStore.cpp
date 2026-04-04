#include "crs/RecentCrsStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

namespace
{
const char kRecentCrsSettingsKey[] = "crs/recentCoordinateSystems";
}

namespace lasviewer::crs
{
QList<CoordinateSystemRef> RecentCrsStore::load() const
{
    QSettings settings;
    const QByteArray data = settings.value(QString::fromLatin1(kRecentCrsSettingsKey)).toByteArray();
    if (data.isEmpty()) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isArray()) {
        return {};
    }

    QList<CoordinateSystemRef> entries;
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const CoordinateSystemRef ref = coordinateSystemRefFromJson(value.toObject());
        if (ref.code > 0) {
            entries.append(ref);
        }
    }
    return entries;
}

void RecentCrsStore::save(const QList<CoordinateSystemRef>& entries) const
{
    QJsonArray array;
    int count = 0;
    for (const CoordinateSystemRef& entry : entries) {
        if (entry.code <= 0) {
            continue;
        }
        array.append(coordinateSystemRefToJson(entry));
        ++count;
        if (count >= kMaxRecentEntries) {
            break;
        }
    }

    QSettings settings;
    settings.setValue(
        QString::fromLatin1(kRecentCrsSettingsKey),
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void RecentCrsStore::add(const CoordinateSystemRef& crs) const
{
    if (crs.code <= 0) {
        return;
    }

    QList<CoordinateSystemRef> entries = load();
    for (int index = 0; index < entries.size(); ++index) {
        if (coordinateSystemRefMatches(entries.at(index), crs)) {
            entries.removeAt(index);
            break;
        }
    }

    entries.prepend(crs);
    save(entries);
}
}
