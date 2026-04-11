#pragma once

#include <QObject>

#include <functional>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QTableWidget;
class PointCloudViewer;

class MeasurementAnalysisController final : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;
    using BoolCallback = std::function<void(bool)>;
    using IntCallback = std::function<void(int)>;
    using DoubleCallback = std::function<void(double)>;

    MeasurementAnalysisController(
        PointCloudViewer* viewer,
        QAction* measureAction,
        QAction* clearMeasurementAction,
        QAction* exportClearanceCsvAction,
        QAction* analyzeVegetationRisksAction,
        QAction* focusVegetationRiskAction,
        QAction* createIssueFromRiskAction,
        QAction* createIssuesFromRisksAction,
        QAction* clearVegetationRisksAction,
        QPushButton* measurementToggleButton,
        QPushButton* measurementClearButton,
        QDoubleSpinBox* clearanceThresholdSpinBox,
        QComboBox* clearanceRulePresetComboBox,
        QDoubleSpinBox* vegetationSearchRadiusSpinBox,
        QDoubleSpinBox* vegetationClusterGapSpinBox,
        QSpinBox* vegetationClusterPointCountSpinBox,
        QCheckBox* preferVegetationClassificationCheckBox,
        QTableWidget* clearanceSegmentsTableWidget,
        QTableWidget* vegetationRisksTableWidget,
        BoolCallback syncProfileDockForMeasurementMode,
        VoidCallback exportClearanceCsv,
        VoidCallback analyzeVegetationRisks,
        VoidCallback focusVegetationRisk,
        VoidCallback createIssueFromSelectedRisk,
        VoidCallback createIssuesFromRisks,
        VoidCallback clearVegetationRisks,
        DoubleCallback clearanceThresholdChanged,
        IntCallback clearanceRulePresetChanged,
        DoubleCallback vegetationSearchRadiusChanged,
        DoubleCallback vegetationClusterGapChanged,
        IntCallback vegetationClusterPointCountChanged,
        BoolCallback preferVegetationClassificationChanged,
        IntCallback clearanceSegmentSelectionChanged,
        IntCallback vegetationRiskSelectionChanged,
        QObject* parent = nullptr);

private:
    PointCloudViewer* viewer_ = nullptr;
};
