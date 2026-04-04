#include "crs/CrsCatalogService.h"

#include <QCoreApplication>

namespace
{
using namespace lasviewer::crs;

bool matchesKindFilter(const CrsCatalogEntry& entry, CoordinateSystemKindFilter kindFilter)
{
    switch (kindFilter) {
    case CoordinateSystemKindFilter::Geographic:
        return entry.reference.kind == CoordinateSystemKind::Geographic;
    case CoordinateSystemKindFilter::Projected:
        return entry.reference.kind == CoordinateSystemKind::Projected;
    case CoordinateSystemKindFilter::Any:
    default:
        return true;
    }
}

CrsCatalogEntry makeEntry(
    int code,
    CoordinateSystemKind kind,
    const QString& displayName,
    const QString& groupName,
    const QString& summary,
    const QStringList& aliases = {},
    bool deprecated = false)
{
    CrsCatalogEntry entry;
    entry.reference.authName = QStringLiteral("EPSG");
    entry.reference.code = code;
    entry.reference.displayName = displayName;
    entry.reference.kind = kind;
    entry.reference.deprecated = deprecated;
    entry.groupName = groupName;
    entry.summary = summary;
    entry.aliases = aliases;
    return entry;
}

QList<CrsCatalogEntry> buildCatalogEntries()
{
    QList<CrsCatalogEntry> entries;
    const QString geographicGroup = QCoreApplication::translate("CrsCatalogService", "Geographic Coordinate Systems");
    const QString projectedGroup = QCoreApplication::translate("CrsCatalogService", "Projected Coordinate Systems");

    entries.append(makeEntry(4326, CoordinateSystemKind::Geographic, QStringLiteral("WGS 84"), geographicGroup,
        QCoreApplication::translate("CrsCatalogService", "Global GPS latitude/longitude coordinate system."),
        { QStringLiteral("WGS84"), QStringLiteral("GPS") }));
    entries.append(makeEntry(4490, CoordinateSystemKind::Geographic, QStringLiteral("CGCS2000"), geographicGroup,
        QCoreApplication::translate("CrsCatalogService", "China Geodetic Coordinate System 2000 geographic CRS."),
        { QStringLiteral("China 2000"), QStringLiteral("CGCS") }));
    entries.append(makeEntry(4214, CoordinateSystemKind::Geographic, QStringLiteral("Beijing 1954"), geographicGroup,
        QCoreApplication::translate("CrsCatalogService", "Legacy Beijing 1954 geographic CRS."),
        { QStringLiteral("BJ54") }));
    entries.append(makeEntry(4610, CoordinateSystemKind::Geographic, QStringLiteral("Xian 1980"), geographicGroup,
        QCoreApplication::translate("CrsCatalogService", "Legacy Xian 1980 geographic CRS."),
        { QStringLiteral("XA80") }));
    entries.append(makeEntry(3857, CoordinateSystemKind::Projected, QStringLiteral("WGS 84 / Pseudo-Mercator"), projectedGroup,
        QCoreApplication::translate("CrsCatalogService", "Common web map projected CRS."),
        { QStringLiteral("Web Mercator"), QStringLiteral("WebMap") }));

    for (int zone = 1; zone <= 60; ++zone) {
        entries.append(makeEntry(32600 + zone, CoordinateSystemKind::Projected,
            QStringLiteral("WGS 84 / UTM zone %1N").arg(zone), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Global UTM projected CRS for the northern hemisphere."),
            { QStringLiteral("UTM"), QStringLiteral("Zone %1").arg(zone) }));
        entries.append(makeEntry(32700 + zone, CoordinateSystemKind::Projected,
            QStringLiteral("WGS 84 / UTM zone %1S").arg(zone), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Global UTM projected CRS for the southern hemisphere."),
            { QStringLiteral("UTM"), QStringLiteral("Zone %1").arg(zone) }));
    }

    for (int centralMeridian = 75; centralMeridian <= 135; centralMeridian += 3) {
        const int index = (centralMeridian - 75) / 3;
        const QString cmAlias = QStringLiteral("CM %1").arg(centralMeridian);
        entries.append(makeEntry(4534 + index, CoordinateSystemKind::Projected,
            QStringLiteral("CGCS2000 / 3-degree Gauss-Kruger CM %1E").arg(centralMeridian), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Common China projected CRS for engineering and surveying."),
            { QStringLiteral("CGCS2000"), QStringLiteral("3GK"), cmAlias }));
        entries.append(makeEntry(2422 + index, CoordinateSystemKind::Projected,
            QStringLiteral("Beijing 1954 / 3-degree Gauss-Kruger CM %1E").arg(centralMeridian), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Legacy Beijing 1954 projected CRS used in China."),
            { QStringLiteral("BJ54"), QStringLiteral("3GK"), cmAlias }));
        entries.append(makeEntry(2370 + index, CoordinateSystemKind::Projected,
            QStringLiteral("Xian 1980 / 3-degree Gauss-Kruger CM %1E").arg(centralMeridian), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Legacy Xian 1980 projected CRS used in China."),
            { QStringLiteral("XA80"), QStringLiteral("3GK"), cmAlias }));
    }

    for (int centralMeridian = 75; centralMeridian <= 135; centralMeridian += 6) {
        const int index = (centralMeridian - 75) / 6;
        const QString cmAlias = QStringLiteral("CM %1").arg(centralMeridian);
        entries.append(makeEntry(4491 + index, CoordinateSystemKind::Projected,
            QStringLiteral("CGCS2000 / Gauss-Kruger CM %1E").arg(centralMeridian), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Common China 6-degree Gauss-Kruger projected CRS."),
            { QStringLiteral("CGCS2000"), QStringLiteral("6GK"), cmAlias }));
        entries.append(makeEntry(21453 + index, CoordinateSystemKind::Projected,
            QStringLiteral("Beijing 1954 / Gauss-Kruger CM %1E").arg(centralMeridian), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Legacy Beijing 1954 6-degree Gauss-Kruger projected CRS."),
            { QStringLiteral("BJ54"), QStringLiteral("6GK"), cmAlias }));
        entries.append(makeEntry(2338 + index, CoordinateSystemKind::Projected,
            QStringLiteral("Xian 1980 / Gauss-Kruger CM %1E").arg(centralMeridian), projectedGroup,
            QCoreApplication::translate("CrsCatalogService", "Legacy Xian 1980 6-degree Gauss-Kruger projected CRS."),
            { QStringLiteral("XA80"), QStringLiteral("6GK"), cmAlias }));
    }

    return entries;
}

bool entryMatchesQuery(const CrsCatalogEntry& entry, const QString& query)
{
    const QString normalizedQuery = query.trimmed().toLower();
    if (normalizedQuery.isEmpty()) {
        return true;
    }

    const QString codeText = QStringLiteral("%1:%2")
        .arg(entry.reference.authName.toLower())
        .arg(QString::number(entry.reference.code));
    if (entry.reference.displayName.toLower().contains(normalizedQuery)
        || codeText.contains(normalizedQuery)
        || entry.summary.toLower().contains(normalizedQuery)) {
        return true;
    }

    for (const QString& alias : entry.aliases) {
        if (alias.toLower().contains(normalizedQuery)) {
            return true;
        }
    }
    return false;
}
}

namespace lasviewer::crs
{
CrsCatalogService::CrsCatalogService()
    : entries_(buildCatalogEntries())
{
}

QList<CrsCatalogEntry> CrsCatalogService::allEntries() const
{
    return entries_;
}

QList<CrsCatalogEntry> CrsCatalogService::filter(
    const QString& query,
    CoordinateSystemKindFilter kindFilter,
    bool includeDeprecated) const
{
    QList<CrsCatalogEntry> results;
    for (const CrsCatalogEntry& entry : entries_) {
        if (!matchesKindFilter(entry, kindFilter)) {
            continue;
        }
        if (!includeDeprecated && entry.reference.deprecated) {
            continue;
        }
        if (!entryMatchesQuery(entry, query)) {
            continue;
        }
        results.append(entry);
    }
    return results;
}

QList<CrsCatalogEntry> CrsCatalogService::recentEntries(CoordinateSystemKindFilter kindFilter) const
{
    QList<CrsCatalogEntry> results;
    for (const CoordinateSystemRef& ref : recentStore_.load()) {
        CrsCatalogEntry entry;
        if (!findByCode(ref.authName, ref.code, &entry)) {
            continue;
        }
        if (matchesKindFilter(entry, kindFilter)) {
            results.append(entry);
        }
    }
    return results;
}

bool CrsCatalogService::findByCode(const QString& authName, int code, CrsCatalogEntry* outputEntry) const
{
    for (const CrsCatalogEntry& entry : entries_) {
        if (entry.reference.code == code
            && entry.reference.authName.compare(authName, Qt::CaseInsensitive) == 0) {
            if (outputEntry != nullptr) {
                *outputEntry = entry;
            }
            return true;
        }
    }
    return false;
}

void CrsCatalogService::markRecentlyUsed(const CoordinateSystemRef& crs) const
{
    recentStore_.add(crs);
}
}
