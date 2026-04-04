#pragma once

#include <QList>
#include <QPointF>

#include "crs/CrsTypes.h"
#include "crs/LASViewerCrsExport.h"

namespace lasviewer::crs
{
class LASVIEWERCRS_EXPORT CrsTransformService
{
public:
    static bool canTransform(
        const CoordinateSystemRef& source,
        const CoordinateSystemRef& target,
        QString* errorMessage = nullptr);
    static bool transformPoint(
        const CoordinateSystemRef& source,
        const CoordinateSystemRef& target,
        const QPointF& input,
        QPointF* output,
        QString* errorMessage = nullptr);
    static bool transformPolyline(
        const CoordinateSystemRef& source,
        const CoordinateSystemRef& target,
        const QList<QPointF>& input,
        QList<QPointF>* output,
        QString* errorMessage = nullptr);
};
}
