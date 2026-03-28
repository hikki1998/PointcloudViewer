#pragma once

#include <QColor>
#include <QStringList>
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
class QDoubleSpinBox;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QFormLayout;
class QGroupBox;
class QIcon;
class QLabel;
class QLineEdit;
class QObject;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QMenu;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;
class QToolBar;
class QTranslator;
class QWidget;

namespace Qtitan
{
class RibbonBar;
class RibbonGroup;
class RibbonPage;
}

class PointCloudViewer;
class ProfilePlotWidget;
struct ClearanceAnalysisResult;

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
    void createProjectDock();
    void createInspectorPanel();
    void createProfileDock();
    void createLogDock();
    void createStatusBar();
    void createConnections();
    void retranslateUi();
    void openPointCloud();
    void addPointCloudFiles();
    void openProject();
    void saveProject();
    void saveProjectAs();
    bool loadPointCloudFile(const QString& filePath);
    bool loadPointCloudFiles(const QStringList& filePaths);
    bool appendPointCloudFiles(const QStringList& filePaths);
    bool loadProjectFile(const QString& filePath);
    bool saveProjectFile(const QString& filePath);
    void clearPointCloud();
    void removeSelectedDataset();
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
    void loadMeasurementSettings();
    void persistMeasurementSettings() const;
    void loadLanguageSettings();
    void persistLanguageSettings() const;
    void loadWindowSettings();
    void persistWindowSettings() const;
    void loadThemeSettings();
    void persistThemeSettings() const;
    void updateNavigationHelpText();
    void updateMeasurementPanel();
    void updateClearanceSegmentsTable(const ClearanceAnalysisResult& clearanceAnalysis);
    void rebuildProjectTree();
    void refreshProjectTreeFilter();
    void updateTowerPanel();
    void updateIssuePanel();
    void updateTowerDetailEditor();
    void updateIssueDetailEditor();
    QWidget* createSliderControl(QSlider*& slider, QLabel*& valueLabel, int minimum, int maximum, int step);
    void updateSliderValueLabel(QSlider* slider, QLabel* valueLabel, const QString& formatText) const;
    void updateVisualizationTooltips();
    QString nextDefaultTowerName() const;
    QString nextDefaultIssueTitle() const;
    QString selectedDatasetPath() const;
    void setTowerEditingEnabled(bool enabled);
    void applyLanguage(UiLanguage language);

    PointCloudViewer* viewer_ = nullptr;
    Qtitan::RibbonBar* ribbonBar_ = nullptr;
    QDockWidget* projectDock_ = nullptr;
    QDockWidget* inspectorDock_ = nullptr;
    QDockWidget* profileDock_ = nullptr;
    QDockWidget* logDock_ = nullptr;
    QTabWidget* inspectorTabWidget_ = nullptr;
    QTranslator* appTranslator_ = nullptr;
    QTranslator* qtTranslator_ = nullptr;
    UiLanguage currentLanguage_ = UiLanguage::English;

    Qtitan::RibbonPage* homePage_ = nullptr;
    Qtitan::RibbonPage* towerPage_ = nullptr;
    Qtitan::RibbonPage* appearancePage_ = nullptr;
    Qtitan::RibbonGroup* datasetRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* cameraRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* sceneRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* measureRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* workspaceRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* towerRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* colorRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* themeRibbonGroup_ = nullptr;
    Qtitan::RibbonGroup* languageRibbonGroup_ = nullptr;

    QGroupBox* datasetGroupBox_ = nullptr;
    QFormLayout* datasetLayout_ = nullptr;
    QGroupBox* renderingGroupBox_ = nullptr;
    QFormLayout* renderingLayout_ = nullptr;
    QGroupBox* measurementGroupBox_ = nullptr;
    QFormLayout* measurementLayout_ = nullptr;
    QGroupBox* clearanceGroupBox_ = nullptr;
    QFormLayout* clearanceLayout_ = nullptr;
    QGroupBox* clearanceSegmentsGroupBox_ = nullptr;
    QGroupBox* navigationGroupBox_ = nullptr;
    QFormLayout* navigationToggleLayout_ = nullptr;
    QGroupBox* towerDetailsGroupBox_ = nullptr;
    QFormLayout* towerDetailsLayout_ = nullptr;
    QGroupBox* issueDetailsGroupBox_ = nullptr;
    QFormLayout* issueDetailsLayout_ = nullptr;

    QLabel* datasetNameValueLabel_ = nullptr;
    QLabel* datasetPathValueLabel_ = nullptr;
    QLabel* datasetPointsValueLabel_ = nullptr;
    QLabel* datasetBoundsValueLabel_ = nullptr;
    QLabel* datasetExtentValueLabel_ = nullptr;
    QLabel* datasetColorValueLabel_ = nullptr;
    QLabel* measurementStartValueLabel_ = nullptr;
    QLabel* measurementEndValueLabel_ = nullptr;
    QLabel* measurementDistanceValueLabel_ = nullptr;
    QLabel* measurementHorizontalDistanceValueLabel_ = nullptr;
    QLabel* measurementDeltaZValueLabel_ = nullptr;
    QLabel* measurementSegmentsValueLabel_ = nullptr;
    QLabel* clearanceShortestValueLabel_ = nullptr;
    QLabel* clearanceWarningCountValueLabel_ = nullptr;
    QLabel* clearanceStatusValueLabel_ = nullptr;
    QLabel* clearanceSegmentsSummaryLabel_ = nullptr;
    QLabel* towerCountValueLabel_ = nullptr;
    QLabel* towerToolStatusLabel_ = nullptr;
    QLabel* issueCountValueLabel_ = nullptr;
    QLabel* issueToolStatusLabel_ = nullptr;
    QLabel* issueLocationValueLabel_ = nullptr;
    QLabel* issueCreatedAtValueLabel_ = nullptr;

    QWidget* pointSizeControl_ = nullptr;
    QWidget* pointOpacityControl_ = nullptr;
    QWidget* depthCueControl_ = nullptr;
    QWidget* edlStrengthControl_ = nullptr;
    QSlider* pointSizeSlider_ = nullptr;
    QSlider* pointOpacitySlider_ = nullptr;
    QSlider* depthCueSlider_ = nullptr;
    QSlider* edlStrengthSlider_ = nullptr;
    QLabel* pointSizeValueLabel_ = nullptr;
    QLabel* pointOpacityValueLabel_ = nullptr;
    QLabel* depthCueValueLabel_ = nullptr;
    QLabel* edlStrengthValueLabel_ = nullptr;
    QComboBox* colorModeComboBox_ = nullptr;
    QPushButton* pointColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QPushButton* measurementToggleButton_ = nullptr;
    QPushButton* measurementClearButton_ = nullptr;
    QDoubleSpinBox* clearanceThresholdSpinBox_ = nullptr;
    QCheckBox* roundSplatsCheckBox_ = nullptr;
    QCheckBox* axesCheckBox_ = nullptr;
    QCheckBox* boundingBoxCheckBox_ = nullptr;
    QCheckBox* invertOrbitCheckBox_ = nullptr;
    QCheckBox* invertPanCheckBox_ = nullptr;
    QCheckBox* invertWheelCheckBox_ = nullptr;
    QLabel* navigationTipsLabel_ = nullptr;
    QLineEdit* projectSearchEdit_ = nullptr;
    QTreeWidget* projectTreeWidget_ = nullptr;
    QTableWidget* towerTableWidget_ = nullptr;
    QTableWidget* issueTableWidget_ = nullptr;
    QTableWidget* clearanceSegmentsTableWidget_ = nullptr;
    QToolBar* measurementToolBar_ = nullptr;
    QToolBar* projectToolBar_ = nullptr;
    QToolBar* towerToolBar_ = nullptr;
    QToolBar* issueToolBar_ = nullptr;
    ProfilePlotWidget* profilePlotWidget_ = nullptr;
    QToolButton* towerMenuButton_ = nullptr;
    QMenu* towerActionsMenu_ = nullptr;
    QToolButton* issueMenuButton_ = nullptr;
    QMenu* issueActionsMenu_ = nullptr;
    QPlainTextEdit* logTextEdit_ = nullptr;
    QLineEdit* towerCodeEdit_ = nullptr;
    QLineEdit* towerLineNameEdit_ = nullptr;
    QLineEdit* towerVoltageLevelEdit_ = nullptr;
    QLineEdit* towerStructureTypeEdit_ = nullptr;
    QLineEdit* towerInspectionDateEdit_ = nullptr;
    QLineEdit* towerStatusEdit_ = nullptr;
    QPlainTextEdit* towerNotesEdit_ = nullptr;
    QLineEdit* issueTitleEdit_ = nullptr;
    QComboBox* issueCategoryComboBox_ = nullptr;
    QComboBox* issueSeverityComboBox_ = nullptr;
    QComboBox* issueStatusComboBox_ = nullptr;
    QComboBox* issueRelatedTowerComboBox_ = nullptr;
    QLineEdit* issueImagePathEdit_ = nullptr;
    QPlainTextEdit* issueDescriptionEdit_ = nullptr;
    QWidget* windowControlsWidget_ = nullptr;
    QToolButton* minimizeButton_ = nullptr;
    QToolButton* maximizeButton_ = nullptr;
    QToolButton* closeButton_ = nullptr;

    QAction* openAction_ = nullptr;
    QAction* addPointCloudAction_ = nullptr;
    QAction* removeDatasetAction_ = nullptr;
    QAction* locateDatasetAction_ = nullptr;
    QAction* copyDatasetPathAction_ = nullptr;
    QAction* expandProjectTreeAction_ = nullptr;
    QAction* collapseProjectTreeAction_ = nullptr;
    QAction* openProjectAction_ = nullptr;
    QAction* saveProjectAction_ = nullptr;
    QAction* saveProjectAsAction_ = nullptr;
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
    QAction* exportClearanceCsvAction_ = nullptr;
    QAction* showProfileDockAction_ = nullptr;
    QAction* startTowerEditAction_ = nullptr;
    QAction* finishTowerEditAction_ = nullptr;
    QAction* addTowerAction_ = nullptr;
    QAction* insertTowerAction_ = nullptr;
    QAction* moveTowerAction_ = nullptr;
    QAction* focusTowerAction_ = nullptr;
    QAction* removeTowerAction_ = nullptr;
    QAction* clearTowersAction_ = nullptr;
    QAction* cancelTowerToolAction_ = nullptr;
    QAction* showTowerXAction_ = nullptr;
    QAction* showTowerYAction_ = nullptr;
    QAction* showTowerZAction_ = nullptr;
    QAction* startIssueMarkAction_ = nullptr;
    QAction* cancelIssueToolAction_ = nullptr;
    QAction* focusIssueAction_ = nullptr;
    QAction* removeIssueAction_ = nullptr;
    QAction* clearIssuesAction_ = nullptr;
    QAction* exportIssuesCsvAction_ = nullptr;
    QAction* exportInspectionReportAction_ = nullptr;
    QAction* showLogAction_ = nullptr;
    QAction* languageEnglishAction_ = nullptr;
    QAction* languageChineseAction_ = nullptr;

    QActionGroup* colorModeActionGroup_ = nullptr;
    QActionGroup* themeActionGroup_ = nullptr;
    QActionGroup* languageActionGroup_ = nullptr;
    QString currentProjectFilePath_;
    bool towerEditingEnabled_ = false;
    double clearanceWarningThresholdMeters_ = 10.0;
    bool updatingTowerDetails_ = false;
    bool updatingIssueDetails_ = false;
};
