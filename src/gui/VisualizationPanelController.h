#pragma once

#include <QObject>

#include <functional>

class QAction;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class PointCloudViewer;

class VisualizationPanelController final : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;

    VisualizationPanelController(
        PointCloudViewer* viewer,
        QAction* showAxesAction,
        QAction* showBoundingBoxAction,
        QAction* darkBackgroundAction,
        QAction* lightBackgroundAction,
        QAction* rgbColorAction,
        QAction* elevationColorAction,
        QAction* singleColorAction,
        QAction* classificationColorAction,
        QSlider* pointSizeSlider,
        QLabel* pointSizeValueLabel,
        QSlider* pointOpacitySlider,
        QLabel* pointOpacityValueLabel,
        QSlider* depthCueSlider,
        QLabel* depthCueValueLabel,
        QSlider* edlStrengthSlider,
        QLabel* edlStrengthValueLabel,
        QComboBox* colorModeComboBox,
        QPushButton* pointColorButton,
        QPushButton* backgroundColorButton,
        VoidCallback choosePointColor,
        VoidCallback chooseBackgroundColor,
        QObject* parent = nullptr);

private:
    void refreshSliderValueLabels();
    void updateSliderValueLabel(QSlider* slider, QLabel* valueLabel, const QString& formatText) const;

    PointCloudViewer* viewer_ = nullptr;
    QSlider* pointSizeSlider_ = nullptr;
    QLabel* pointSizeValueLabel_ = nullptr;
    QSlider* pointOpacitySlider_ = nullptr;
    QLabel* pointOpacityValueLabel_ = nullptr;
    QSlider* depthCueSlider_ = nullptr;
    QLabel* depthCueValueLabel_ = nullptr;
    QSlider* edlStrengthSlider_ = nullptr;
    QLabel* edlStrengthValueLabel_ = nullptr;
    VoidCallback choosePointColor_;
    VoidCallback chooseBackgroundColor_;
};
