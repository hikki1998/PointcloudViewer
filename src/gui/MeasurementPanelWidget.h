#pragma once

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QToolBar;

class MeasurementPanelWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementPanelWidget(QWidget* parent = nullptr);

    QToolBar* toolBar() const;

    QGroupBox* measurementGroupBox() const;
    QFormLayout* measurementLayout() const;
    QPushButton* measurementToggleButton() const;
    QPushButton* measurementClearButton() const;
    QLabel* measurementStartValueLabel() const;
    QLabel* measurementEndValueLabel() const;
    QLabel* measurementDistanceValueLabel() const;
    QLabel* measurementHorizontalDistanceValueLabel() const;
    QLabel* measurementDeltaZValueLabel() const;
    QLabel* measurementSegmentsValueLabel() const;

    QGroupBox* clearanceGroupBox() const;
    QFormLayout* clearanceLayout() const;
    QDoubleSpinBox* clearanceThresholdSpinBox() const;
    QLabel* clearanceShortestValueLabel() const;
    QLabel* clearanceWarningCountValueLabel() const;
    QLabel* clearanceStatusValueLabel() const;
    QComboBox* clearanceRulePresetComboBox() const;
    QLabel* clearanceRuleBandsValueLabel() const;

    QGroupBox* clearanceSegmentsGroupBox() const;
    QLabel* clearanceSegmentsSummaryLabel() const;
    QTableWidget* clearanceSegmentsTableWidget() const;

private:
    QToolBar* toolBar_ = nullptr;

    QGroupBox* measurementGroupBox_ = nullptr;
    QFormLayout* measurementLayout_ = nullptr;
    QPushButton* measurementToggleButton_ = nullptr;
    QPushButton* measurementClearButton_ = nullptr;
    QLabel* measurementStartValueLabel_ = nullptr;
    QLabel* measurementEndValueLabel_ = nullptr;
    QLabel* measurementDistanceValueLabel_ = nullptr;
    QLabel* measurementHorizontalDistanceValueLabel_ = nullptr;
    QLabel* measurementDeltaZValueLabel_ = nullptr;
    QLabel* measurementSegmentsValueLabel_ = nullptr;

    QGroupBox* clearanceGroupBox_ = nullptr;
    QFormLayout* clearanceLayout_ = nullptr;
    QDoubleSpinBox* clearanceThresholdSpinBox_ = nullptr;
    QLabel* clearanceShortestValueLabel_ = nullptr;
    QLabel* clearanceWarningCountValueLabel_ = nullptr;
    QLabel* clearanceStatusValueLabel_ = nullptr;
    QComboBox* clearanceRulePresetComboBox_ = nullptr;
    QLabel* clearanceRuleBandsValueLabel_ = nullptr;

    QGroupBox* clearanceSegmentsGroupBox_ = nullptr;
    QLabel* clearanceSegmentsSummaryLabel_ = nullptr;
    QTableWidget* clearanceSegmentsTableWidget_ = nullptr;
};
