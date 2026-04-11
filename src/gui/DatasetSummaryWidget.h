#pragma once

#include <QGroupBox>

namespace Ui
{
class DatasetSummaryWidget;
}

class QFormLayout;
class QLabel;

class DatasetSummaryWidget final : public QGroupBox
{
    Q_OBJECT

public:
    explicit DatasetSummaryWidget(QWidget* parent = nullptr);
    ~DatasetSummaryWidget() override;

    void retranslateUi();

    QFormLayout* datasetLayout() const;
    QLabel* datasetNameValueLabel() const;
    QLabel* datasetPathValueLabel() const;
    QLabel* datasetPointsValueLabel() const;
    QLabel* datasetBoundsValueLabel() const;
    QLabel* datasetExtentValueLabel() const;
    QLabel* datasetColorValueLabel() const;

private:
    Ui::DatasetSummaryWidget* ui_ = nullptr;
};
