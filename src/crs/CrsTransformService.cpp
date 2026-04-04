#include "crs/CrsTransformService.h"

#include <QCoreApplication>

#include <cmath>

#include "crs/CrsProjRuntime.h"

#ifdef LASVIEWERCRS_HAS_PROJ
#include <proj.h>
#endif

namespace lasviewer::crs
{
bool CrsTransformService::canTransform(
    const CoordinateSystemRef& source,
    const CoordinateSystemRef& target,
    QString* errorMessage)
{
    if (source.code <= 0 || target.code <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "CrsTransformService",
                "Coordinate transformation requires both source and target CRS.");
        }
        return false;
    }

#ifndef LASVIEWERCRS_HAS_PROJ
    if (source.code != target.code && errorMessage != nullptr) {
        *errorMessage = QCoreApplication::translate(
            "CrsTransformService",
            "PROJ support is not available in this build.");
    }
#endif
    return true;
}

bool CrsTransformService::transformPoint(
    const CoordinateSystemRef& source,
    const CoordinateSystemRef& target,
    const QPointF& input,
    QPointF* output,
    QString* errorMessage)
{
    if (output == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CrsTransformService", "Coordinate output pointer is null.");
        }
        return false;
    }
    if (!canTransform(source, target, errorMessage)) {
        return false;
    }
    if (coordinateSystemRefMatches(source, target)) {
        *output = input;
        return true;
    }

#ifdef LASVIEWERCRS_HAS_PROJ
    PJ_CONTEXT* context = proj_context_create();
    if (context == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CrsTransformService", "Failed to create PROJ context.");
        }
        return false;
    }
    configureProjSearchPaths(context);

    const QString sourceName = coordinateSystemCodeText(source);
    const QString targetName = coordinateSystemCodeText(target);
    PJ* transform = proj_create_crs_to_crs(
        context,
        sourceName.toUtf8().constData(),
        targetName.toUtf8().constData(),
        nullptr);
    if (transform == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "CrsTransformService",
                "Failed to create CRS transform from %1 to %2.")
                .arg(sourceName, targetName);
        }
        proj_context_destroy(context);
        return false;
    }

    PJ* normalized = proj_normalize_for_visualization(context, transform);
    proj_destroy(transform);
    if (normalized == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "CrsTransformService",
                "Failed to normalize CRS transform from %1 to %2.")
                .arg(sourceName, targetName);
        }
        proj_context_destroy(context);
        return false;
    }

    const PJ_COORD result = proj_trans(normalized, PJ_FWD, proj_coord(input.x(), input.y(), 0.0, 0.0));
    proj_destroy(normalized);
    proj_context_destroy(context);
    if (!std::isfinite(result.xy.x) || !std::isfinite(result.xy.y)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "CrsTransformService",
                "Coordinate transformation returned invalid result.");
        }
        return false;
    }

    *output = QPointF(result.xy.x, result.xy.y);
    return true;
#else
    Q_UNUSED(input);
    return false;
#endif
}

bool CrsTransformService::transformPolyline(
    const CoordinateSystemRef& source,
    const CoordinateSystemRef& target,
    const QList<QPointF>& input,
    QList<QPointF>* output,
    QString* errorMessage)
{
    if (output == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CrsTransformService", "Coordinate output pointer is null.");
        }
        return false;
    }

    output->clear();
    output->reserve(input.size());
    for (const QPointF& point : input) {
        QPointF transformedPoint;
        if (!transformPoint(source, target, point, &transformedPoint, errorMessage)) {
            output->clear();
            return false;
        }
        output->append(transformedPoint);
    }
    return true;
}
}
