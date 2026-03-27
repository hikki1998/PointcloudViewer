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
class QCloseEvent;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QFormLayout;
class QGroupBox;
class QIcon;
class QLabel;
class QObject;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QToolButton;
class QTranslator;
class QWidget;

namespace Qtitan
{
class RibbonBar;
class RibbonGroup;
class RibbonPage;
}

class PointCloudViewer;

class MainWindow final : public Qtitan::RibbonMainWindow
{
    Q_OBJECT

public:
    enum class UiLanguage
    {
        English,
        Chinese
    };

    explicit MainWindow(QTranslator* appTranslator, QTranslator* qtTranslator, QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif

private:
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    void createActions();
    void createRibbon();
    void createWindowControls();
    void createInspectorPanel();
    void createLogDock();
    void createStatusBar();
    void createConnections();
    void retranslateUi();
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
    bool isInteractiveRibbonWidget(const QWidget* widget) const;
    bool isWindowControlWidget(const QWidget* widget) const;
    void syncUiFromViewer();
    void updateDatasetPanel();
    void updateActionState();
    void setColorButtonAppearance(QPushButton* button, const QColor& color, const QString& fallbackText) const;
    void appendLog(LogLevel level, const QString& message);
    void showUserMessage(LogLevel level, const QString& message, int timeoutMs = 3000);
    void loadVisualizationSettings();
    void persistVisualizationSettings() const;
    void loadInteractionSettings();
    void persistInteractionSettings() const;
    void loadLanguageSettings();
    void persistLanguageSettings() const;
    void loadWindowSettings();
    void persistWindowSettings() const;
    void loadThemeSettings();
    void persistThemeSettings() const;
    void updateNavigationHelpText();
    void updateMeasurementPanel();
    void applyLanguage(UiLanguage language);

    PointCloudViewer* viewer_ = nullptr;
    Qtitan::RibbonBar* ribbonBar_ = nullptr;
    QDockWidget* inspectorDock_ = nullptr;
    QDockWidget* logDock_ = nullptr;
    QTabWidget* inspectorTabWidget_ = nullptr;
    QTranslator* appTranslator_ = nullptr;
    QTranslator* qtTranslator_ = nullptr;
    UiLanguage currentLanguage_ = UiLanguage::English;

    Qtitan::RibbonPage* homePage_ = nullptr;
    Qtitan::RibbonPage* appearancePage_ = nullptr;
    Qtitan::RibbonGroup* datasetRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* cameraRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* sceneRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* measureRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* workspaceRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* colorRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* themeRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* languageRibbonGroup_ = nullptr;

    QGroupBox* datasetGroupBox_ = nullptr;
    QFormLayout* datasetLayout_ = nullptr;
    QGroupBox* renderingGroupBox_ = nullptr;
    QFormLayout* renderingLayout_ = nullptr;
    QGroupBox* measurementGroupBox_ = nullptr;
    QFormLayout* measurementLayout_ = nullptr;
    QGroupBox* navigationGroupBox_ = nullptr;
    QFormLayout* navigationToggleLayout_ = nullptr;

    QLabel* datasetNameValueLabel_ = nullptr;
    QLabel* datasetPathValueLabel_ = nullptr;
    QLabel* datasetPointsValueLabel_ = nullptr;
    QLabel* datasetBoundsValueLabel_ = nullptr;
    QLabel* datasetExtentValueLabel_ = nullptr;
    QLabel* datasetColorValueLabel_ = nullptr;
    QLabel* measurementStartValueLabel_ = nullptr;
    QLabel* measurementEndValueLabel_ = nullptr;
    QLabel* measurementDistanceValueLabel_ = nullptr;
    QLabel* measurementDeltaZValueLabel_ = nullptr;

    QSpinBox* pointSizeSpinBox_ = nullptr;
    QComboBox* colorModeComboBox_ = nullptr;
    QPushButton* pointColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QPushButton* measurementToggleButton_ = nullptr;
    QPushButton* measurementClearButton_ = nullptr;
    QCheckBox* axesCheckBox_ = nullptr;
    QCheckBox* boundingBoxCheckBox_ = nullptr;
    QCheckBox* invertOrbitCheckBox_ = nullptr;
    QCheckBox* invertPanCheckBox_ = nullptr;
    QCheckBox* invertWheelCheckBox_ = nullptr;
    QLabel* navigationTipsLabel_ = nullptr;
    QPlainTextEdit* logTextEdit_ = nullptr;
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
    QAction* measureAction_ = nullptr;
    QAction* clearMeasurementAction_ = nullptr;
    QAction* showLogAction_ = nullptr;
    QAction* languageEnglishAction_ = nullptr;
    QAction* languageChineseAction_ = nullptr;

    QActionGroup* colorModeActionGroup_ = nullptr;
    QActionGroup* themeActionGroup_ = nullptr;
    QActionGroup* languageActionGroup_ = nullptr;
};
