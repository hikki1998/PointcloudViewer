#include "gui/VisualizationPanelController.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSlider>

#include "gui/PointCloudViewer.h"

namespace
{
const QColor kDarkBackground(20, 28, 38);
const QColor kLightBackground(241, 244, 249);

QString trMainWindow(const char* sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

VisualizationPanelController::VisualizationPanelController(
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
    QObject* parent)
    : QObject(parent)
    , viewer_(viewer)
    , pointSizeSlider_(pointSizeSlider)
    , pointSizeValueLabel_(pointSizeValueLabel)
    , pointOpacitySlider_(pointOpacitySlider)
    , pointOpacityValueLabel_(pointOpacityValueLabel)
    , depthCueSlider_(depthCueSlider)
    , depthCueValueLabel_(depthCueValueLabel)
    , edlStrengthSlider_(edlStrengthSlider)
    , edlStrengthValueLabel_(edlStrengthValueLabel)
    , choosePointColor_(std::move(choosePointColor))
    , chooseBackgroundColor_(std::move(chooseBackgroundColor))
{
    if (viewer_ == nullptr) {
        return;
    }

    if (showAxesAction != nullptr) {
        connect(showAxesAction, &QAction::toggled, viewer_, &PointCloudViewer::setShowAxes);
    }
    if (showBoundingBoxAction != nullptr) {
        connect(showBoundingBoxAction, &QAction::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    }
    if (darkBackgroundAction != nullptr) {
        connect(darkBackgroundAction, &QAction::triggered, this, [this]() {
            viewer_->setBackgroundColor(kDarkBackground);
        });
    }
    if (lightBackgroundAction != nullptr) {
        connect(lightBackgroundAction, &QAction::triggered, this, [this]() {
            viewer_->setBackgroundColor(kLightBackground);
        });
    }

    if (rgbColorAction != nullptr) {
        connect(rgbColorAction, &QAction::triggered, this, [this]() {
            viewer_->setColorMode(PointCloudColorMode::Rgb);
        });
    }
    if (elevationColorAction != nullptr) {
        connect(elevationColorAction, &QAction::triggered, this, [this]() {
            viewer_->setColorMode(PointCloudColorMode::Elevation);
        });
    }
    if (singleColorAction != nullptr) {
        connect(singleColorAction, &QAction::triggered, this, [this]() {
            viewer_->setColorMode(PointCloudColorMode::SingleColor);
        });
    }
    if (classificationColorAction != nullptr) {
        connect(classificationColorAction, &QAction::triggered, this, [this]() {
            viewer_->setColorMode(PointCloudColorMode::Classification);
        });
    }

    if (pointSizeSlider_ != nullptr) {
        connect(pointSizeSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setPointSize);
        connect(pointSizeSlider_, &QSlider::valueChanged, this, [this](int) {
            updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, trMainWindow("%1 px"));
        });
    }
    if (pointOpacitySlider_ != nullptr) {
        connect(pointOpacitySlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setPointOpacity);
        connect(pointOpacitySlider_, &QSlider::valueChanged, this, [this](int) {
            updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, trMainWindow("%1%"));
        });
    }
    if (depthCueSlider_ != nullptr) {
        connect(depthCueSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setDepthCueStrength);
        connect(depthCueSlider_, &QSlider::valueChanged, this, [this](int) {
            updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, trMainWindow("%1%"));
        });
    }
    if (edlStrengthSlider_ != nullptr) {
        connect(edlStrengthSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setEdlStrength);
        connect(edlStrengthSlider_, &QSlider::valueChanged, this, [this](int) {
            updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, trMainWindow("%1%"));
        });
    }

    if (colorModeComboBox != nullptr) {
        connect(
            colorModeComboBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            viewer_,
            static_cast<void (PointCloudViewer::*)(int)>(&PointCloudViewer::setColorMode));
    }

    if (pointColorButton != nullptr) {
        connect(pointColorButton, &QPushButton::clicked, this, [this]() {
            if (choosePointColor_) {
                choosePointColor_();
            }
        });
    }
    if (backgroundColorButton != nullptr) {
        connect(backgroundColorButton, &QPushButton::clicked, this, [this]() {
            if (chooseBackgroundColor_) {
                chooseBackgroundColor_();
            }
        });
    }

    refreshSliderValueLabels();
}

void VisualizationPanelController::refreshSliderValueLabels()
{
    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, trMainWindow("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, trMainWindow("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, trMainWindow("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, trMainWindow("%1%"));
}

void VisualizationPanelController::updateSliderValueLabel(QSlider* slider, QLabel* valueLabel, const QString& formatText) const
{
    if (slider == nullptr || valueLabel == nullptr || formatText.trimmed().isEmpty()) {
        return;
    }

    valueLabel->setText(formatText.arg(QLocale().toString(slider->value())));
}
