#pragma once

#include <QColor>
#include <QStyle>

#include "QtnRibbonMainWindow.h"
#include "QtnRibbonStyle.h"

class QAction;
class QActionGroup;
class QByteArray;
class QCheckBox;
class QComboBox;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QIcon;
class QLabel;
class QObject;
class QPushButton;
class QSpinBox;
class QToolButton;
class QWidget;

namespace Qtitan
{
class RibbonBar;
}

class PointCloudViewer;

class MainWindow final : public Qtitan::RibbonMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif

private:
    void createActions();
    void createRibbon();
    void createWindowControls();
    void createInspectorPanel();
    void createStatusBar();
    void createConnections();
    void openPointCloud();
    bool loadPointCloudFile(const QString& filePath);
    void clearPointCloud();
    void choosePointColor();
    void chooseBackgroundColor();
    void applyOfficeTheme(Qtitan::RibbonStyle::Theme theme);
    void updateWindowChromePalette(Qtitan::RibbonStyle::Theme theme);
    void updateWindowControlButtons();
    void updateWindowControlAppearance(Qtitan::RibbonStyle::Theme theme);
    void toggleMaximizedWindow();
    bool isDraggableRibbonArea(const QPoint& position) const;
    bool isWindowControlWidget(const QWidget* widget) const;
    void syncUiFromViewer();
    void updateDatasetPanel();
    void updateActionState();
    void setColorButtonAppearance(QPushButton* button, const QColor& color, const QString& fallbackText) const;

    PointCloudViewer* viewer_ = nullptr;
    Qtitan::RibbonBar* ribbonBar_ = nullptr;
    QDockWidget* inspectorDock_ = nullptr;

    QLabel* datasetNameValueLabel_ = nullptr;
    QLabel* datasetPathValueLabel_ = nullptr;
    QLabel* datasetPointsValueLabel_ = nullptr;
    QLabel* datasetBoundsValueLabel_ = nullptr;
    QLabel* datasetExtentValueLabel_ = nullptr;
    QLabel* datasetColorValueLabel_ = nullptr;

    QSpinBox* pointSizeSpinBox_ = nullptr;
    QComboBox* colorModeComboBox_ = nullptr;
    QPushButton* pointColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QCheckBox* axesCheckBox_ = nullptr;
    QCheckBox* boundingBoxCheckBox_ = nullptr;
    QWidget* windowControlsWidget_ = nullptr;
    QToolButton* minimizeButton_ = nullptr;
    QToolButton* maximizeButton_ = nullptr;
    QToolButton* closeButton_ = nullptr;

    QAction* openAction_ = nullptr;
    QAction* clearAction_ = nullptr;
    QAction* exitAction_ = nullptr;
    QAction* fitSceneAction_ = nullptr;
    QAction* topViewAction_ = nullptr;
    QAction* frontViewAction_ = nullptr;
    QAction* rightViewAction_ = nullptr;
    QAction* showAxesAction_ = nullptr;
    QAction* showBoundingBoxAction_ = nullptr;
    QAction* darkBackgroundAction_ = nullptr;
    QAction* lightBackgroundAction_ = nullptr;
    QAction* rgbColorAction_ = nullptr;
    QAction* elevationColorAction_ = nullptr;
    QAction* singleColorAction_ = nullptr;
    QAction* themeColorfulAction_ = nullptr;
    QAction* themeWhiteAction_ = nullptr;
    QAction* themeDarkGrayAction_ = nullptr;

    QActionGroup* colorModeActionGroup_ = nullptr;
    QActionGroup* themeActionGroup_ = nullptr;
};
