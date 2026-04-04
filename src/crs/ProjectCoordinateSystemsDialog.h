#pragma once

#include <QDialog>

#include "crs/CrsCatalogService.h"

class QLabel;

namespace lasviewer::crs
{
class LASVIEWERCRS_EXPORT ProjectCoordinateSystemsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectCoordinateSystemsDialog(QWidget* parent = nullptr);

    void setCoordinateSystems(const ProjectCoordinateSystems& coordinateSystems);
    ProjectCoordinateSystems coordinateSystems() const;

private:
    void buildUi();
    void refreshUi();
    QString summaryFor(const CoordinateSystemRef& crs) const;
    void choosePointCloudCrs();
    void chooseGeographicCrs();

    CrsCatalogService catalogService_;
    ProjectCoordinateSystems coordinateSystems_ { CoordinateSystemRef(), defaultGeographicCoordinateSystem() };

    QLabel* pointCloudNameValueLabel_ = nullptr;
    QLabel* pointCloudCodeValueLabel_ = nullptr;
    QLabel* pointCloudTypeValueLabel_ = nullptr;
    QLabel* pointCloudSummaryValueLabel_ = nullptr;
    QLabel* geographicNameValueLabel_ = nullptr;
    QLabel* geographicCodeValueLabel_ = nullptr;
    QLabel* geographicTypeValueLabel_ = nullptr;
    QLabel* geographicSummaryValueLabel_ = nullptr;
};
}
