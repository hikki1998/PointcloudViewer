#pragma once

#include <QWidget>

namespace Ui
{
class BackstageProjectPropertiesWidget;
}

class QLabel;
class QPushButton;

class BackstageProjectPropertiesWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit BackstageProjectPropertiesWidget(QWidget* parent = nullptr);
    ~BackstageProjectPropertiesWidget() override;

    void retranslateUi();

    QLabel* projectFileLabel() const;
    QLabel* datasetCountLabel() const;
    QLabel* coordinateSystemsLabel() const;
    QLabel* projectFileValueLabel() const;
    QLabel* datasetCountValueLabel() const;
    QLabel* coordinateSystemsValueLabel() const;
    QPushButton* editCoordinateSystemsButton() const;

private:
    Ui::BackstageProjectPropertiesWidget* ui_ = nullptr;
};
