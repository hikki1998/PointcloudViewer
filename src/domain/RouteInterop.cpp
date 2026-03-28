#include "domain/RouteInterop.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <array>
#include <cmath>
#include <cstdint>

namespace
{
struct ZipEntry
{
    QString name;
    QByteArray data;
    std::uint32_t crc = 0;
    std::uint32_t localHeaderOffset = 0;
};

QString formatCoordinate(double value, int precision = 8)
{
    return QString::number(value, 'f', precision);
}

QByteArray buildBasicKmlDocument(const InspectionRoute& routeWgs84)
{
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement(QStringLiteral("kml"));
    writer.writeDefaultNamespace(QStringLiteral("http://www.opengis.net/kml/2.2"));
    writer.writeStartElement(QStringLiteral("Document"));
    writer.writeTextElement(QStringLiteral("name"), routeWgs84.name);

    writer.writeStartElement(QStringLiteral("Placemark"));
    writer.writeTextElement(QStringLiteral("name"), QObject::tr("Inspection Route"));
    writer.writeStartElement(QStringLiteral("LineString"));
    writer.writeTextElement(QStringLiteral("tessellate"), QStringLiteral("1"));
    writer.writeStartElement(QStringLiteral("coordinates"));
    QStringList lineCoordinates;
    lineCoordinates.reserve(routeWgs84.waypoints.size());
    for (const InspectionWaypoint& waypoint : routeWgs84.waypoints) {
        lineCoordinates.append(
            QStringLiteral("%1,%2,%3")
                .arg(formatCoordinate(waypoint.longitude))
                .arg(formatCoordinate(waypoint.latitude))
                .arg(formatCoordinate(waypoint.altitude, 3)));
    }
    writer.writeCharacters(lineCoordinates.join(QLatin1Char(' ')));
    writer.writeEndElement(); // coordinates
    writer.writeEndElement(); // LineString
    writer.writeEndElement(); // Placemark

    for (int index = 0; index < routeWgs84.waypoints.size(); ++index) {
        const InspectionWaypoint& waypoint = routeWgs84.waypoints.at(index);
        writer.writeStartElement(QStringLiteral("Placemark"));
        writer.writeTextElement(QStringLiteral("name"), waypoint.id);

        writer.writeStartElement(QStringLiteral("ExtendedData"));
        writer.writeStartElement(QStringLiteral("Data"));
        writer.writeAttribute(QStringLiteral("name"), QStringLiteral("speedMps"));
        writer.writeTextElement(QStringLiteral("value"), formatCoordinate(waypoint.speedMps, 3));
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("Data"));
        writer.writeAttribute(QStringLiteral("name"), QStringLiteral("gimbalPitchDeg"));
        writer.writeTextElement(QStringLiteral("value"), formatCoordinate(waypoint.gimbalPitchDeg, 3));
        writer.writeEndElement();
        writer.writeEndElement(); // ExtendedData

        writer.writeStartElement(QStringLiteral("Point"));
        writer.writeTextElement(
            QStringLiteral("coordinates"),
            QStringLiteral("%1,%2,%3")
                .arg(formatCoordinate(waypoint.longitude))
                .arg(formatCoordinate(waypoint.latitude))
                .arg(formatCoordinate(waypoint.altitude, 3)));
        writer.writeEndElement(); // Point
        writer.writeEndElement(); // Placemark
    }

    writer.writeEndElement(); // Document
    writer.writeEndElement(); // kml
    writer.writeEndDocument();
    return xml;
}

QByteArray buildDjiTemplateKml(
    const InspectionRoute& routeWgs84,
    const RoutePlanningOptions& planningOptions)
{
    const DjiAircraftProfileMapping profileMapping = djiAircraftProfileMapping(planningOptions.aircraftProfile);
    const double referenceLongitude = routeWgs84.waypoints.isEmpty() ? 0.0 : routeWgs84.waypoints.first().longitude;
    const double referenceLatitude = routeWgs84.waypoints.isEmpty() ? 0.0 : routeWgs84.waypoints.first().latitude;
    const double referenceAltitude = routeWgs84.waypoints.isEmpty() ? 0.0 : routeWgs84.waypoints.first().altitude;

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement(QStringLiteral("kml"));
    writer.writeDefaultNamespace(QStringLiteral("http://www.opengis.net/kml/2.2"));
    writer.writeNamespace(QStringLiteral("http://www.dji.com/wpmz/1.0.2"), QStringLiteral("wpml"));
    writer.writeStartElement(QStringLiteral("Document"));

    writer.writeTextElement(QStringLiteral("wpml:author"), QStringLiteral("LASPointCloudViewer"));
    const QString epochMs = QString::number(QDateTime::currentMSecsSinceEpoch());
    writer.writeTextElement(QStringLiteral("wpml:createTime"), epochMs);
    writer.writeTextElement(QStringLiteral("wpml:updateTime"), epochMs);

    writer.writeStartElement(QStringLiteral("wpml:missionConfig"));
    writer.writeTextElement(QStringLiteral("wpml:flyToWaylineMode"), QStringLiteral("safely"));
    writer.writeTextElement(QStringLiteral("wpml:finishAction"), QStringLiteral("goHome"));
    writer.writeTextElement(QStringLiteral("wpml:exitOnRCLost"), QStringLiteral("goContinue"));
    writer.writeTextElement(QStringLiteral("wpml:executeRCLostAction"), QStringLiteral("hover"));
    writer.writeTextElement(QStringLiteral("wpml:takeOffSecurityHeight"), formatCoordinate(planningOptions.safety.safetyHeightMeters, 3));
    writer.writeTextElement(QStringLiteral("wpml:globalRTHHeight"), formatCoordinate(planningOptions.safety.globalRthHeightMeters, 3));
    writer.writeTextElement(QStringLiteral("wpml:globalTransitionalSpeed"), formatCoordinate(planningOptions.safety.globalTransitionalSpeedMps, 3));
    writer.writeTextElement(
        QStringLiteral("wpml:takeOffRefPoint"),
        QStringLiteral("%1,%2,%3")
            .arg(formatCoordinate(referenceLatitude))
            .arg(formatCoordinate(referenceLongitude))
            .arg(formatCoordinate(referenceAltitude, 3)));

    writer.writeStartElement(QStringLiteral("wpml:droneInfo"));
    writer.writeTextElement(QStringLiteral("wpml:droneEnumValue"), QString::number(profileMapping.droneEnumValue));
    writer.writeTextElement(QStringLiteral("wpml:droneSubEnumValue"), QString::number(profileMapping.droneSubEnumValue));
    writer.writeEndElement();

    writer.writeStartElement(QStringLiteral("wpml:payloadInfo"));
    writer.writeTextElement(QStringLiteral("wpml:payloadEnumValue"), QString::number(profileMapping.payloadEnumValue));
    writer.writeTextElement(QStringLiteral("wpml:payloadPositionIndex"), QString::number(profileMapping.payloadPositionIndex));
    writer.writeEndElement();
    writer.writeEndElement(); // missionConfig

    writer.writeStartElement(QStringLiteral("Folder"));
    writer.writeTextElement(QStringLiteral("wpml:templateType"), QStringLiteral("waypoint"));
    writer.writeTextElement(QStringLiteral("wpml:templateId"), QStringLiteral("0"));
    writer.writeStartElement(QStringLiteral("wpml:waylineCoordinateSysParam"));
    writer.writeTextElement(QStringLiteral("wpml:coordinateMode"), QStringLiteral("WGS84"));
    writer.writeTextElement(QStringLiteral("wpml:heightMode"), QStringLiteral("relativeToStartPoint"));
    writer.writeTextElement(QStringLiteral("wpml:positioningType"), QStringLiteral("GPS"));
    writer.writeEndElement();
    writer.writeTextElement(QStringLiteral("wpml:autoFlightSpeed"), formatCoordinate(planningOptions.safety.defaultWaypointSpeedMps, 3));
    writer.writeTextElement(QStringLiteral("wpml:gimbalPitchMode"), QStringLiteral("usePointSetting"));

    for (int index = 0; index < routeWgs84.waypoints.size(); ++index) {
        const InspectionWaypoint& waypoint = routeWgs84.waypoints.at(index);
        writer.writeStartElement(QStringLiteral("Placemark"));
        writer.writeStartElement(QStringLiteral("Point"));
        writer.writeTextElement(
            QStringLiteral("coordinates"),
            QStringLiteral("%1,%2")
                .arg(formatCoordinate(waypoint.longitude))
                .arg(formatCoordinate(waypoint.latitude)));
        writer.writeEndElement(); // Point
        writer.writeTextElement(QStringLiteral("wpml:index"), QString::number(index));
        writer.writeTextElement(QStringLiteral("wpml:useGlobalHeight"), QStringLiteral("0"));
        writer.writeTextElement(QStringLiteral("wpml:height"), formatCoordinate(waypoint.altitude, 3));
        writer.writeTextElement(QStringLiteral("wpml:useGlobalSpeed"), QStringLiteral("0"));
        writer.writeTextElement(QStringLiteral("wpml:waypointSpeed"), formatCoordinate(waypoint.speedMps, 3));
        writer.writeTextElement(QStringLiteral("wpml:useGlobalHeadingParam"), QStringLiteral("1"));
        writer.writeTextElement(QStringLiteral("wpml:useGlobalTurnParam"), QStringLiteral("1"));
        writer.writeTextElement(QStringLiteral("wpml:gimbalPitchAngle"), formatCoordinate(waypoint.gimbalPitchDeg, 3));
        writer.writeEndElement(); // Placemark
    }

    writer.writeEndElement(); // Folder
    writer.writeEndElement(); // Document
    writer.writeEndElement(); // kml
    writer.writeEndDocument();
    return xml;
}

QByteArray buildDjiWaylinesWpml(
    const InspectionRoute& routeWgs84,
    const RoutePlanningOptions& planningOptions)
{
    const DjiAircraftProfileMapping profileMapping = djiAircraftProfileMapping(planningOptions.aircraftProfile);
    const float referenceHeight = routeWgs84.waypoints.isEmpty()
        ? 0.0f
        : routeWgs84.waypoints.first().altitude;

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement(QStringLiteral("kml"));
    writer.writeDefaultNamespace(QStringLiteral("http://www.opengis.net/kml/2.2"));
    writer.writeNamespace(QStringLiteral("http://www.dji.com/wpmz/1.0.2"), QStringLiteral("wpml"));
    writer.writeStartElement(QStringLiteral("Document"));

    writer.writeStartElement(QStringLiteral("wpml:missionConfig"));
    writer.writeTextElement(QStringLiteral("wpml:flyToWaylineMode"), QStringLiteral("safely"));
    writer.writeTextElement(QStringLiteral("wpml:finishAction"), QStringLiteral("goHome"));
    writer.writeTextElement(QStringLiteral("wpml:exitOnRCLost"), QStringLiteral("goContinue"));
    writer.writeTextElement(QStringLiteral("wpml:executeRCLostAction"), QStringLiteral("hover"));
    writer.writeTextElement(QStringLiteral("wpml:takeOffSecurityHeight"), formatCoordinate(planningOptions.safety.safetyHeightMeters, 3));
    writer.writeTextElement(QStringLiteral("wpml:globalRTHHeight"), formatCoordinate(planningOptions.safety.globalRthHeightMeters, 3));
    writer.writeTextElement(QStringLiteral("wpml:globalTransitionalSpeed"), formatCoordinate(planningOptions.safety.globalTransitionalSpeedMps, 3));
    writer.writeStartElement(QStringLiteral("wpml:droneInfo"));
    writer.writeTextElement(QStringLiteral("wpml:droneEnumValue"), QString::number(profileMapping.droneEnumValue));
    writer.writeTextElement(QStringLiteral("wpml:droneSubEnumValue"), QString::number(profileMapping.droneSubEnumValue));
    writer.writeEndElement();
    writer.writeStartElement(QStringLiteral("wpml:payloadInfo"));
    writer.writeTextElement(QStringLiteral("wpml:payloadEnumValue"), QString::number(profileMapping.payloadEnumValue));
    writer.writeTextElement(QStringLiteral("wpml:payloadPositionIndex"), QString::number(profileMapping.payloadPositionIndex));
    writer.writeEndElement();
    writer.writeEndElement(); // missionConfig

    writer.writeStartElement(QStringLiteral("Folder"));
    writer.writeTextElement(QStringLiteral("wpml:templateId"), QStringLiteral("0"));
    writer.writeTextElement(QStringLiteral("wpml:executeHeightMode"), QStringLiteral("relativeToStartPoint"));
    writer.writeTextElement(QStringLiteral("wpml:waylineId"), QStringLiteral("0"));
    writer.writeTextElement(QStringLiteral("wpml:autoFlightSpeed"), formatCoordinate(planningOptions.safety.defaultWaypointSpeedMps, 3));

    for (int index = 0; index < routeWgs84.waypoints.size(); ++index) {
        const InspectionWaypoint& waypoint = routeWgs84.waypoints.at(index);
        writer.writeStartElement(QStringLiteral("Placemark"));
        writer.writeStartElement(QStringLiteral("Point"));
        writer.writeTextElement(
            QStringLiteral("coordinates"),
            QStringLiteral("%1,%2")
                .arg(formatCoordinate(waypoint.longitude))
                .arg(formatCoordinate(waypoint.latitude)));
        writer.writeEndElement(); // Point
        writer.writeTextElement(QStringLiteral("wpml:index"), QString::number(index));
        writer.writeTextElement(
            QStringLiteral("wpml:executeHeight"),
            formatCoordinate(std::max(0.0f, waypoint.altitude - referenceHeight + planningOptions.safety.safetyHeightMeters), 3));
        writer.writeTextElement(QStringLiteral("wpml:waypointSpeed"), formatCoordinate(waypoint.speedMps, 3));

        writer.writeStartElement(QStringLiteral("wpml:waypointHeadingParam"));
        writer.writeTextElement(QStringLiteral("wpml:waypointHeadingMode"), QStringLiteral("followWayline"));
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("wpml:waypointTurnParam"));
        writer.writeTextElement(QStringLiteral("wpml:waypointTurnMode"), QStringLiteral("toPointAndStopWithDiscontinuityCurvature"));
        writer.writeTextElement(QStringLiteral("wpml:waypointTurnDampingDist"), QStringLiteral("0"));
        writer.writeEndElement();

        writer.writeEndElement(); // Placemark
    }

    writer.writeEndElement(); // Folder
    writer.writeEndElement(); // Document
    writer.writeEndElement(); // kml
    writer.writeEndDocument();
    return xml;
}

std::uint32_t crc32(const QByteArray& data)
{
    static std::array<std::uint32_t, 256> table {};
    static bool initialized = false;
    if (!initialized) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
            }
            table[i] = c;
        }
        initialized = true;
    }

    std::uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : data) {
        c = table[(c ^ ch) & 0xFFu] ^ (c >> 8u);
    }
    return c ^ 0xFFFFFFFFu;
}

void appendLe16(QByteArray& output, std::uint16_t value)
{
    output.append(static_cast<char>(value & 0xFFu));
    output.append(static_cast<char>((value >> 8u) & 0xFFu));
}

void appendLe32(QByteArray& output, std::uint32_t value)
{
    output.append(static_cast<char>(value & 0xFFu));
    output.append(static_cast<char>((value >> 8u) & 0xFFu));
    output.append(static_cast<char>((value >> 16u) & 0xFFu));
    output.append(static_cast<char>((value >> 24u) & 0xFFu));
}

bool writeStoredZip(
    const QString& filePath,
    QList<ZipEntry> entries,
    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to create KMZ file.");
        }
        return false;
    }

    QByteArray output;
    output.reserve(4096);

    for (ZipEntry& entry : entries) {
        entry.crc = crc32(entry.data);
        entry.localHeaderOffset = static_cast<std::uint32_t>(output.size());

        const QByteArray fileNameUtf8 = entry.name.toUtf8();
        appendLe32(output, 0x04034B50u);
        appendLe16(output, 20u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe32(output, entry.crc);
        appendLe32(output, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(output, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(output, static_cast<std::uint16_t>(fileNameUtf8.size()));
        appendLe16(output, 0u);
        output.append(fileNameUtf8);
        output.append(entry.data);
    }

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t>(output.size());
    for (const ZipEntry& entry : entries) {
        const QByteArray fileNameUtf8 = entry.name.toUtf8();
        appendLe32(output, 0x02014B50u);
        appendLe16(output, 20u);
        appendLe16(output, 20u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe32(output, entry.crc);
        appendLe32(output, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(output, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(output, static_cast<std::uint16_t>(fileNameUtf8.size()));
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe16(output, 0u);
        appendLe32(output, 0u);
        appendLe32(output, entry.localHeaderOffset);
        output.append(fileNameUtf8);
    }

    const std::uint32_t centralDirectorySize =
        static_cast<std::uint32_t>(output.size()) - centralDirectoryOffset;
    appendLe32(output, 0x06054B50u);
    appendLe16(output, 0u);
    appendLe16(output, 0u);
    appendLe16(output, static_cast<std::uint16_t>(entries.size()));
    appendLe16(output, static_cast<std::uint16_t>(entries.size()));
    appendLe32(output, centralDirectorySize);
    appendLe32(output, centralDirectoryOffset);
    appendLe16(output, 0u);

    if (file.write(output) != output.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to write KMZ content.");
        }
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool parseCoordinatesText(const QString& coordinatesText, QList<InspectionWaypoint>* outputWaypoints)
{
    if (outputWaypoints == nullptr) {
        return false;
    }

    const QStringList coordinateTokens = coordinatesText.split(QRegularExpression(QStringLiteral("[\\s\\n\\r\\t]+")), Qt::SkipEmptyParts);
    for (int index = 0; index < coordinateTokens.size(); ++index) {
        const QString token = coordinateTokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }

        const QStringList components = token.split(QLatin1Char(','), Qt::KeepEmptyParts);
        if (components.size() < 2) {
            continue;
        }

        bool okLongitude = false;
        bool okLatitude = false;
        const double longitude = components.at(0).toDouble(&okLongitude);
        const double latitude = components.at(1).toDouble(&okLatitude);
        if (!okLongitude || !okLatitude) {
            continue;
        }

        float altitude = 0.0f;
        if (components.size() >= 3) {
            bool okAltitude = false;
            altitude = static_cast<float>(components.at(2).toDouble(&okAltitude));
            if (!okAltitude) {
                altitude = 0.0f;
            }
        }

        InspectionWaypoint waypoint;
        waypoint.id = QStringLiteral("import_wp_%1").arg(index + 1);
        waypoint.longitude = longitude;
        waypoint.latitude = latitude;
        waypoint.altitude = altitude;
        waypoint.localPoint.z = altitude;
        outputWaypoints->append(waypoint);
    }

    return !outputWaypoints->isEmpty();
}
}

bool exportRouteKml(
    const QString& filePath,
    const InspectionRoute& routeWgs84,
    QString* errorMessage)
{
    if (routeWgs84.waypoints.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Route is empty.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to create KML file.");
        }
        return false;
    }

    const QByteArray xml = buildBasicKmlDocument(routeWgs84);
    if (file.write(xml) != xml.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to write KML file.");
        }
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool importRouteKml(
    const QString& filePath,
    InspectionRoute* routeWgs84,
    QString* errorMessage)
{
    if (routeWgs84 == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Route output pointer is null.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to open KML file.");
        }
        return false;
    }

    QXmlStreamReader reader(&file);
    QString lineStringCoordinates;
    QList<InspectionWaypoint> waypointsFromPoints;
    bool insideLineString = false;
    bool insidePoint = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("LineString")) {
                insideLineString = true;
            } else if (name == QStringLiteral("Point")) {
                insidePoint = true;
            } else if (name == QStringLiteral("coordinates")) {
                const QString coordinatesText = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (insideLineString && lineStringCoordinates.isEmpty()) {
                    lineStringCoordinates = coordinatesText;
                } else if (insidePoint) {
                    QList<InspectionWaypoint> parsed;
                    if (parseCoordinatesText(coordinatesText, &parsed) && !parsed.isEmpty()) {
                        InspectionWaypoint pointWaypoint = parsed.first();
                        pointWaypoint.id = QStringLiteral("import_wp_%1").arg(waypointsFromPoints.size() + 1);
                        waypointsFromPoints.append(pointWaypoint);
                    }
                }
            }
        } else if (reader.isEndElement()) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("LineString")) {
                insideLineString = false;
            } else if (name == QStringLiteral("Point")) {
                insidePoint = false;
            }
        }
    }
    file.close();

    if (reader.hasError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("KML parsing error: %1").arg(reader.errorString());
        }
        return false;
    }

    InspectionRoute route;
    route.name = QFileInfo(filePath).baseName();
    route.source = QStringLiteral("KMLImport");
    route.generatedAtUtc = QDateTime::currentDateTimeUtc();

    if (!lineStringCoordinates.isEmpty()) {
        parseCoordinatesText(lineStringCoordinates, &route.waypoints);
    }
    if (route.waypoints.isEmpty()) {
        route.waypoints = waypointsFromPoints;
    }

    if (route.waypoints.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("No waypoint coordinates were found in KML.");
        }
        return false;
    }

    *routeWgs84 = route;
    return true;
}

bool exportRouteDjiKmz(
    const QString& filePath,
    const InspectionRoute& routeWgs84,
    const RoutePlanningOptions& planningOptions,
    QString* errorMessage)
{
    if (routeWgs84.waypoints.size() < 2) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("DJI KMZ export requires at least 2 waypoints.");
        }
        return false;
    }

    const QByteArray templateKml = buildDjiTemplateKml(routeWgs84, planningOptions);
    const QByteArray waylinesWpml = buildDjiWaylinesWpml(routeWgs84, planningOptions);
    QList<ZipEntry> entries;
    entries.append({ QStringLiteral("wpmz/template.kml"), templateKml, 0u, 0u });
    entries.append({ QStringLiteral("wpmz/waylines.wpml"), waylinesWpml, 0u, 0u });
    return writeStoredZip(filePath, entries, errorMessage);
}
