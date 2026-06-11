#include "crs/CrsAuthorityService.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include "crs/CrsProjRuntime.h"

#ifdef LASVIEWERCRS_HAS_PROJ
#include <proj.h>
#endif

namespace
{
using namespace lasviewer::crs;

#ifdef LASVIEWERCRS_HAS_PROJ
CoordinateSystemKind kindFromProjType(PJ_TYPE type, CoordinateSystemKind fallback)
{
    switch (type) {
    case PJ_TYPE_GEOGRAPHIC_2D_CRS:
    case PJ_TYPE_GEOGRAPHIC_3D_CRS:
    case PJ_TYPE_GEOGRAPHIC_CRS:
    case PJ_TYPE_GEODETIC_CRS:
    case PJ_TYPE_GEOCENTRIC_CRS:
        return CoordinateSystemKind::Geographic;
    case PJ_TYPE_PROJECTED_CRS:
#ifdef PJ_TYPE_DERIVED_PROJECTED_CRS
    case PJ_TYPE_DERIVED_PROJECTED_CRS:
#endif
        return CoordinateSystemKind::Projected;
    default:
        return fallback;
    }
}
#endif

CrsMatchConfidence confidenceFromProjScore(int score)
{
    if (score >= 100) {
        return CrsMatchConfidence::Exact;
    }
    if (score >= 70) {
        return CrsMatchConfidence::Equivalent;
    }
    if (score >= 40) {
        return CrsMatchConfidence::Alias;
    }
    if (score > 0) {
        return CrsMatchConfidence::Heuristic;
    }
    return CrsMatchConfidence::Unknown;
}

QString projSupportUnavailableMessage()
{
    return QCoreApplication::translate("CrsAuthorityService", "PROJ support is not available in this build.");
}

#ifdef LASVIEWERCRS_HAS_PROJ
CoordinateSystemDefinition definitionFromProjObject(
    PJ_CONTEXT* context,
    const PJ* object,
    const CoordinateSystemRef& fallbackRef = CoordinateSystemRef())
{
    CoordinateSystemDefinition definition;
    definition.reference = fallbackRef;

    if (object == nullptr) {
        return definition;
    }

    const char* authName = proj_get_id_auth_name(object, 0);
    const char* authCode = proj_get_id_code(object, 0);
    if (authName != nullptr && authName[0] != '\0') {
        definition.reference.authName = QString::fromUtf8(authName);
    } else if (definition.reference.authName.trimmed().isEmpty()) {
        definition.reference.authName = QStringLiteral("EPSG");
    }

    bool ok = false;
    const int parsedCode = authCode != nullptr ? QString::fromUtf8(authCode).toInt(&ok) : 0;
    if (ok) {
        definition.reference.code = parsedCode;
    }

    const char* name = proj_get_name(object);
    if (name != nullptr && name[0] != '\0') {
        definition.canonicalName = QString::fromUtf8(name);
        definition.reference.displayName = definition.canonicalName;
    }

    definition.reference.kind = kindFromProjType(proj_get_type(object), definition.reference.kind);
    definition.reference.deprecated = proj_is_deprecated(object) != 0;

    const char* remarks = proj_get_remarks(object);
    if (remarks != nullptr && remarks[0] != '\0') {
        definition.remarks = QString::fromUtf8(remarks);
    }

    const char* scope = proj_get_scope(object);
    if (scope != nullptr && scope[0] != '\0') {
        definition.scope = QString::fromUtf8(scope);
    }

    const char* areaName = nullptr;
    if (proj_get_area_of_use(context, object, nullptr, nullptr, nullptr, nullptr, &areaName)) {
        if (areaName != nullptr && areaName[0] != '\0') {
            definition.areaOfUse = QString::fromUtf8(areaName);
        }
    }

    const char* wkt = proj_as_wkt(context, object, PJ_WKT2_2019_SIMPLIFIED, nullptr);
    if (wkt != nullptr && wkt[0] != '\0') {
        definition.reference.wkt = QString::fromUtf8(wkt);
    }

    const char* projString = proj_as_proj_string(context, object, PJ_PROJ_5, nullptr);
    if (projString != nullptr && projString[0] != '\0') {
        definition.projString = QString::fromUtf8(projString);
    }

    const char* projJson = proj_as_projjson(context, object, nullptr);
    if (projJson != nullptr && projJson[0] != '\0') {
        definition.projJson = QString::fromUtf8(projJson);
    }

    return definition;
}
#endif
}

namespace lasviewer::crs
{
bool CrsAuthorityService::isAvailable()
{
#ifdef LASVIEWERCRS_HAS_PROJ
    return true;
#else
    return false;
#endif
}

CrsResolveResult CrsAuthorityService::resolveFromAuthority(const QString& authName, int code)
{
    CrsResolveResult result;

    if (code <= 0) {
        result.errorMessage = QCoreApplication::translate(
            "CrsAuthorityService",
            "Coordinate system authority code is invalid.");
        return result;
    }

#ifndef LASVIEWERCRS_HAS_PROJ
    result.errorMessage = projSupportUnavailableMessage();
    return result;
#else
    PJ_CONTEXT* context = proj_context_create();
    if (context == nullptr) {
        result.errorMessage = QCoreApplication::translate(
            "CrsAuthorityService",
            "Failed to create PROJ context.");
        return result;
    }
    configureProjSearchPaths(context);

    const QString normalizedAuthName = authName.trimmed().isEmpty()
        ? QStringLiteral("EPSG")
        : authName.trimmed().toUpper();
    const QByteArray authNameUtf8 = normalizedAuthName.toUtf8();
    const QByteArray codeUtf8 = QString::number(code).toUtf8();
    PJ* object = proj_create_from_database(
        context,
        authNameUtf8.constData(),
        codeUtf8.constData(),
        PJ_CATEGORY_CRS,
        false,
        nullptr);
    if (object == nullptr) {
        result.errorMessage = projContextErrorMessage(
            context,
            QCoreApplication::translate(
                "CrsAuthorityService",
                "Failed to resolve coordinate system %1:%2.")
                .arg(normalizedAuthName)
                .arg(code));
        proj_context_destroy(context);
        return result;
    }

    CoordinateSystemRef fallbackRef;
    fallbackRef.authName = normalizedAuthName;
    fallbackRef.code = code;
    fallbackRef.kind = CoordinateSystemKind::Projected;
    result.definition = definitionFromProjObject(context, object, fallbackRef);
    result.definition.reference.authName = normalizedAuthName;
    result.definition.reference.code = code;
    result.ok = true;
    result.confidence = CrsMatchConfidence::Exact;

    proj_destroy(object);
    proj_context_destroy(context);
    return result;
#endif
}

CrsResolveResult CrsAuthorityService::resolveFromAuthorityText(const QString& authorityText)
{
    static const QRegularExpression kAuthorityPattern(
        QStringLiteral("^\\s*([A-Za-z0-9_\\-]+)\\s*:\\s*(\\d+)\\s*$"));

    CrsResolveResult result;
    const QRegularExpressionMatch match = kAuthorityPattern.match(authorityText);
    if (!match.hasMatch()) {
        result.errorMessage = QCoreApplication::translate(
            "CrsAuthorityService",
            "Authority text must look like EPSG:4326.");
        return result;
    }

    return resolveFromAuthority(match.captured(1), match.captured(2).toInt());
}

CrsResolveResult CrsAuthorityService::resolveFromWkt(const QString& wkt)
{
    CrsResolveResult result;
    if (wkt.trimmed().isEmpty()) {
        result.errorMessage = QCoreApplication::translate(
            "CrsAuthorityService",
            "Coordinate system WKT is empty.");
        return result;
    }

#ifndef LASVIEWERCRS_HAS_PROJ
    result.errorMessage = projSupportUnavailableMessage();
    return result;
#else
    PJ_CONTEXT* context = proj_context_create();
    if (context == nullptr) {
        result.errorMessage = QCoreApplication::translate(
            "CrsAuthorityService",
            "Failed to create PROJ context.");
        return result;
    }
    configureProjSearchPaths(context);

    PROJ_STRING_LIST warnings = nullptr;
    PROJ_STRING_LIST grammarErrors = nullptr;
    const QByteArray wktUtf8 = wkt.toUtf8();
    PJ* object = proj_create_from_wkt(
        context,
        wktUtf8.constData(),
        nullptr,
        &warnings,
        &grammarErrors);
    proj_string_list_destroy(warnings);
    proj_string_list_destroy(grammarErrors);

    if (object == nullptr) {
        result.errorMessage = projContextErrorMessage(
            context,
            QCoreApplication::translate(
                "CrsAuthorityService",
                "Failed to parse coordinate system WKT."));
        proj_context_destroy(context);
        return result;
    }

    result.definition = definitionFromProjObject(context, object);
    if (result.definition.reference.wkt.trimmed().isEmpty()) {
        result.definition.reference.wkt = wkt.trimmed();
    }

    int* confidenceValues = nullptr;
    PJ_OBJ_LIST* identified = proj_identify(context, object, nullptr, nullptr, &confidenceValues);
    if (identified != nullptr) {
        const int count = proj_list_get_count(identified);
        for (int index = 0; index < count; ++index) {
            PJ* candidateObject = proj_list_get(context, identified, index);
            if (candidateObject == nullptr) {
                continue;
            }
            CoordinateSystemDefinition candidate = definitionFromProjObject(context, candidateObject);
            if (candidate.reference.wkt.trimmed().isEmpty()) {
                candidate.reference.wkt = result.definition.reference.wkt;
            }
            result.candidates.append(candidate);
            proj_destroy(candidateObject);
        }
        if (!result.candidates.isEmpty()) {
            result.definition = result.candidates.first();
            result.confidence = confidenceFromProjScore(confidenceValues != nullptr ? confidenceValues[0] : 0);
        }
        proj_list_destroy(identified);
    }
    proj_int_list_destroy(confidenceValues);

    result.ok = true;
    if (result.confidence == CrsMatchConfidence::Unknown) {
        result.confidence = CrsMatchConfidence::Heuristic;
    }

    proj_destroy(object);
    proj_context_destroy(context);
    return result;
#endif
}

QList<CoordinateSystemDefinition> CrsAuthorityService::findByName(
    const QString& name,
    CoordinateSystemKindFilter kindFilter,
    int limit)
{
    QList<CoordinateSystemDefinition> results;
    if (name.trimmed().isEmpty() || limit <= 0) {
        return results;
    }

#ifndef LASVIEWERCRS_HAS_PROJ
    Q_UNUSED(kindFilter);
    return results;
#else
    PJ_CONTEXT* context = proj_context_create();
    if (context == nullptr) {
        return results;
    }
    configureProjSearchPaths(context);

    PJ_TYPE geographicTypes[] = { PJ_TYPE_GEOGRAPHIC_2D_CRS, PJ_TYPE_GEOGRAPHIC_3D_CRS, PJ_TYPE_GEODETIC_CRS };
    PJ_TYPE projectedTypes[] = {
        PJ_TYPE_PROJECTED_CRS
#ifdef PJ_TYPE_DERIVED_PROJECTED_CRS
        , PJ_TYPE_DERIVED_PROJECTED_CRS
#endif
    };
    PJ_TYPE allTypes[] = {
        PJ_TYPE_GEOGRAPHIC_2D_CRS,
        PJ_TYPE_GEOGRAPHIC_3D_CRS,
        PJ_TYPE_GEODETIC_CRS,
        PJ_TYPE_PROJECTED_CRS
#ifdef PJ_TYPE_DERIVED_PROJECTED_CRS
        ,
        PJ_TYPE_DERIVED_PROJECTED_CRS
#endif
    };

    const PJ_TYPE* types = nullptr;
    size_t typeCount = 0;
    switch (kindFilter) {
    case CoordinateSystemKindFilter::Geographic:
        types = geographicTypes;
        typeCount = sizeof(geographicTypes) / sizeof(geographicTypes[0]);
        break;
    case CoordinateSystemKindFilter::Projected:
        types = projectedTypes;
        typeCount = sizeof(projectedTypes) / sizeof(projectedTypes[0]);
        break;
    case CoordinateSystemKindFilter::Any:
    default:
        types = allTypes;
        typeCount = sizeof(allTypes) / sizeof(allTypes[0]);
        break;
    }

    const QByteArray nameUtf8 = name.trimmed().toUtf8();
    PJ_OBJ_LIST* objects = proj_create_from_name(
        context,
        nullptr,
        nameUtf8.constData(),
        types,
        typeCount,
        true,
        static_cast<size_t>(limit),
        nullptr);
    if (objects != nullptr) {
        const int count = proj_list_get_count(objects);
        for (int index = 0; index < count; ++index) {
            PJ* object = proj_list_get(context, objects, index);
            if (object == nullptr) {
                continue;
            }
            results.append(definitionFromProjObject(context, object));
            proj_destroy(object);
        }
        proj_list_destroy(objects);
    }

    proj_context_destroy(context);
    return results;
#endif
}

bool CrsAuthorityService::normalizeCoordinateSystem(
    const CoordinateSystemRef& input,
    CoordinateSystemRef* output,
    QString* errorMessage)
{
    if (output == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "CrsAuthorityService",
                "Coordinate system output pointer is null.");
        }
        return false;
    }

    CoordinateSystemRef normalized = input;
    if (normalized.authName.trimmed().isEmpty()) {
        normalized.authName = QStringLiteral("EPSG");
    }

    if (!normalized.wkt.trimmed().isEmpty()) {
        const CrsResolveResult wktResult = resolveFromWkt(normalized.wkt);
        if (wktResult.ok) {
            normalized = wktResult.definition.reference;
            if (input.code > 0 && normalized.code <= 0) {
                normalized.code = input.code;
            }
            if (!input.authName.trimmed().isEmpty() && normalized.authName.trimmed().isEmpty()) {
                normalized.authName = input.authName.trimmed();
            }
            *output = normalized;
            return true;
        }
    }

    if (normalized.code > 0) {
        const CrsResolveResult authorityResult = resolveFromAuthority(normalized.authName, normalized.code);
        if (authorityResult.ok) {
            normalized = authorityResult.definition.reference;
            *output = normalized;
            return true;
        }
        if (errorMessage != nullptr) {
            *errorMessage = authorityResult.errorMessage;
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QCoreApplication::translate(
            "CrsAuthorityService",
            "Coordinate system does not contain enough information to normalize.");
    }
    *output = normalized;
    return false;
}

QString CrsAuthorityService::authorityText(const CoordinateSystemRef& crs)
{
    CoordinateSystemRef normalized;
    if (normalizeCoordinateSystem(crs, &normalized, nullptr)) {
        return coordinateSystemCodeText(normalized);
    }
    return coordinateSystemCodeText(crs);
}

QString CrsAuthorityService::displayName(const CoordinateSystemRef& crs)
{
    CoordinateSystemRef normalized;
    if (normalizeCoordinateSystem(crs, &normalized, nullptr)
        && !normalized.displayName.trimmed().isEmpty()) {
        return normalized.displayName;
    }
    if (!crs.displayName.trimmed().isEmpty()) {
        return crs.displayName;
    }
    return authorityText(crs);
}

QString CrsAuthorityService::exportWkt(const CoordinateSystemRef& crs)
{
    if (!crs.wkt.trimmed().isEmpty()) {
        return crs.wkt;
    }

    CoordinateSystemRef normalized;
    if (normalizeCoordinateSystem(crs, &normalized, nullptr)) {
        return normalized.wkt;
    }
    return QString();
}

QString CrsAuthorityService::exportProjString(const CoordinateSystemRef& crs)
{
    CoordinateSystemRef normalized;
    if (!normalizeCoordinateSystem(crs, &normalized, nullptr)) {
        return QString();
    }
    const CrsResolveResult result = resolveFromAuthority(normalized.authName, normalized.code);
    return result.ok ? result.definition.projString : QString();
}

QString CrsAuthorityService::exportProjJson(const CoordinateSystemRef& crs)
{
    CoordinateSystemRef normalized;
    if (!normalizeCoordinateSystem(crs, &normalized, nullptr)) {
        return QString();
    }
    const CrsResolveResult result = resolveFromAuthority(normalized.authName, normalized.code);
    return result.ok ? result.definition.projJson : QString();
}
}
