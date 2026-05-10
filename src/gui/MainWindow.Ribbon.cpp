#include "gui/MainWindow.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QPalette>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>

#include "QtnRibbonBar.h"
#include "QtnRibbonGroup.h"
#include "QtnRibbonPage.h"
#include "QtnRibbonQuickAccessBar.h"
#include "QtnRibbonStyle.h"
#include "gui/support/RibbonIconFactory.h"

using lasviewer::gui::RibbonGlyph;
using lasviewer::gui::WindowControlGlyph;
using lasviewer::gui::createRibbonIcon;
using lasviewer::gui::createWindowControlIcon;

void MainWindow::createRibbon()
{
    ribbonBar_ = new Qtitan::RibbonBar(this);
    // Avoid Qtitan's DWM-integrated frame path, which paints a black caption area on this setup.
    ribbonBar_->setFrameThemeEnabled(false);
    ribbonBar_->setTitleBarVisible(false);
    ribbonBar_->showQuickAccess(true);
    ribbonBar_->setStyleSheet(QStringLiteral(
        "QAbstractButton {"
        "background-color: transparent;"
        "border: none;"
        "color: #0f172a;"
        "}"
        "QAbstractButton:hover {"
        "background-color: rgba(37, 99, 235, 0.16);"
        "color: #0b1220;"
        "}"
        "QAbstractButton:checked, QAbstractButton:pressed {"
        "background-color: #1d4ed8;"
        "color: #ffffff;"
        "}"
        "QAbstractButton:disabled {"
        "background-color: transparent;"
        "color: #64748b;"
        "}"));
    ribbonBar_->installEventFilter(this);
    setRibbonBar(ribbonBar_);
    createWindowControls();
    ribbonBar_->addSystemButton(createRibbonIcon(RibbonGlyph::Open), tr("File"));
    backstageSystemButton_ = ribbonBar_->getSystemButton();
    createBackstageView();

    ribbonBar_->quickAccessBar()->addAction(openAction_);
    ribbonBar_->quickAccessBar()->addAction(saveProjectAction_);
    ribbonBar_->quickAccessBar()->addAction(fitSceneAction_);
    ribbonBar_->quickAccessBar()->addAction(showAxesAction_);
    ribbonBar_->quickAccessBar()->addAction(measureAction_);
    ribbonBar_->quickAccessBar()->addAction(captureScreenshotAction_);

    homePage_ = ribbonBar_->addPage(tr("Home"));
    datasetRibbonGroup_ = homePage_->addGroup(tr("Dataset"));
    datasetRibbonGroup_->addAction(openAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(addPointCloudAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(projectCoordinateSystemsAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(clearAction_, Qt::ToolButtonTextUnderIcon);

    cameraRibbonGroup_ = homePage_->addGroup(tr("Camera"));
    cameraRibbonGroup_->addAction(fitSceneAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(topViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(frontViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(rightViewAction_, Qt::ToolButtonTextUnderIcon);

    sceneRibbonGroup_ = homePage_->addGroup(tr("Scene"));
    sceneRibbonGroup_->addAction(showAxesAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(showBoundingBoxAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(darkBackgroundAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(lightBackgroundAction_, Qt::ToolButtonTextUnderIcon);

    captureRibbonGroup_ = homePage_->addGroup(tr("Capture"));
    captureRibbonGroup_->addAction(captureScreenshotAction_, Qt::ToolButtonTextUnderIcon);
    captureRibbonGroup_->addAction(toggleScreenRecordingAction_, Qt::ToolButtonTextUnderIcon);

    measureRibbonGroup_ = homePage_->addGroup(tr("Measure"));
    measureRibbonGroup_->addAction(measureAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(clearMeasurementAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(showProfileDockAction_, Qt::ToolButtonTextUnderIcon);

    classificationRibbonGroup_ = homePage_->addGroup(tr("Classification"));
    classificationRibbonGroup_->addAction(profileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(showProfileClassificationDockAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(saveProfileClassificationEditsAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(undoProfileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(redoProfileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(clearProfileClassificationEditsAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(exportClearanceCsvAction_, Qt::ToolButtonTextUnderIcon);

    towerRibbonGroup_ = homePage_->addGroup(tr("Tower Editing"));
    towerRibbonGroup_->addAction(startTowerEditAction_, Qt::ToolButtonTextUnderIcon);
    towerRibbonGroup_->addAction(finishTowerEditAction_, Qt::ToolButtonTextUnderIcon);

    routePage_ = ribbonBar_->addPage(tr("Route"));
    routePlanningRibbonGroup_ = routePage_->addGroup(tr("Route Planning"));
    routePlanningRibbonGroup_->addAction(generateInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    routePlanningRibbonGroup_->addAction(regenerateInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    routePlanningRibbonGroup_->addAction(clearInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    routePlanningRibbonGroup_->addAction(toggleRouteEditingAction_, Qt::ToolButtonTextUnderIcon);

    routeFileRibbonGroup_ = routePage_->addGroup(tr("Route Files"));
    routeFileRibbonGroup_->addAction(importRouteFileAction_, Qt::ToolButtonTextUnderIcon);
    routeFileRibbonGroup_->addAction(saveRouteFileAction_, Qt::ToolButtonTextUnderIcon);
    routeFileRibbonGroup_->addAction(saveRouteFileAsAction_, Qt::ToolButtonTextUnderIcon);
    routeFileRibbonGroup_->addAction(reloadRouteFileAction_, Qt::ToolButtonTextUnderIcon);

    routeExchangeRibbonGroup_ = routePage_->addGroup(tr("Route Exchange"));
    routeExchangeRibbonGroup_->addAction(importRouteKmlAction_, Qt::ToolButtonTextUnderIcon);
    routeExchangeRibbonGroup_->addAction(exportRouteKmlAction_, Qt::ToolButtonTextUnderIcon);
    routeExchangeRibbonGroup_->addAction(exportRouteDjiKmzAction_, Qt::ToolButtonTextUnderIcon);

    clipRibbonGroup_ = homePage_->addGroup(tr("Clip"));
    clipRibbonGroup_->addAction(clipModeNoneAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipModeBoxAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipModePolygonAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipBoxWorldAlignedAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipBoxViewAlignedAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipScopeActiveDatasetAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipScopeVisibleDatasetsAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipToggleInsideAction_, Qt::ToolButtonTextUnderIcon);
    clipRibbonGroup_->addAction(clipApplyExportAction_, Qt::ToolButtonTextUnderIcon);

    const QList<QWidget*> ribbonWidgets = ribbonBar_->findChildren<QWidget*>();
    for (QWidget* ribbonWidget : ribbonWidgets) {
        ribbonWidget->installEventFilter(this);
    }
}

void MainWindow::createViewQuickToolBar()
{
    const double quickToolUiScale = std::clamp(static_cast<double>(logicalDpiX()) / 96.0, 1.0, 2.0);
    const int quickToolIconSize = static_cast<int>(std::lround(18.0 * quickToolUiScale));
    const int quickToolSpacing = static_cast<int>(std::lround(4.0 * quickToolUiScale));
    const int quickToolPaddingY = static_cast<int>(std::lround(6.0 * quickToolUiScale));
    const int quickToolPaddingX = static_cast<int>(std::lround(3.0 * quickToolUiScale));
    const int quickToolButtonPadding = static_cast<int>(std::lround(6.0 * quickToolUiScale));
    const int quickToolButtonRadius = static_cast<int>(std::lround(8.0 * quickToolUiScale));
    const int quickToolButtonSize = quickToolIconSize + quickToolButtonPadding * 2;
    const int quickToolSeparatorMarginY = static_cast<int>(std::lround(6.0 * quickToolUiScale));
    const int quickToolSeparatorMarginX = static_cast<int>(std::lround(8.0 * quickToolUiScale));
    const int quickToolTipPaddingY = static_cast<int>(std::lround(4.0 * quickToolUiScale));
    const int quickToolTipPaddingX = static_cast<int>(std::lround(8.0 * quickToolUiScale));

    viewQuickToolBar_ = new QToolBar(tr("View Toolbar"), this);
    viewQuickToolBar_->setObjectName(QStringLiteral("viewQuickToolBar"));
    viewQuickToolBar_->setOrientation(Qt::Vertical);
    viewQuickToolBar_->setMovable(false);
    viewQuickToolBar_->setFloatable(false);
    viewQuickToolBar_->setIconSize(QSize(quickToolIconSize, quickToolIconSize));
    viewQuickToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewQuickToolBar_->setContextMenuPolicy(Qt::PreventContextMenu);
    viewQuickToolBar_->setStyleSheet(QStringLiteral(
        "QToolBar#viewQuickToolBar {"
        "background-color: #f8fbff;"
        "border-right: 1px solid #d5e2f0;"
        "spacing: %1px;"
        "padding: %2px %3px;"
        "}"
        "QToolButton {"
        "background: transparent;"
        "border: 1px solid transparent;"
        "border-radius: %4px;"
        "padding: %5px;"
        "min-width: %6px;"
        "min-height: %6px;"
        "color: #0f172a;"
        "}"
        "QToolButton:hover {"
        "background-color: #e0ebfb;"
        "border-color: #c4d7f2;"
        "}"
        "QToolButton:checked {"
        "background-color: #d0e2ff;"
        "border-color: #97b8ea;"
        "}"
        "QToolButton:pressed {"
        "background-color: #bfdbfe;"
        "border-color: #7ea6de;"
        "}"
        "QToolButton:disabled {"
        "color: #94a3b8;"
        "}"
        "QToolTip {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #94a3b8;"
        "padding: %7px %8px;"
        "border-radius: 4px;"
        "}"
        "QToolBar::separator {"
        "background: #d8e3f2;"
        "width: 1px;"
        "height: 1px;"
        "margin: %9px %10px;"
        "}")
        .arg(quickToolSpacing)
        .arg(quickToolPaddingY)
        .arg(quickToolPaddingX)
        .arg(quickToolButtonRadius)
        .arg(quickToolButtonPadding)
        .arg(quickToolButtonSize)
        .arg(quickToolTipPaddingY)
        .arg(quickToolTipPaddingX)
        .arg(quickToolSeparatorMarginY)
        .arg(quickToolSeparatorMarginX));

    viewQuickToolBar_->addAction(fitSceneAction_);
    viewQuickToolBar_->addAction(topViewAction_);
    viewQuickToolBar_->addAction(frontViewAction_);
    viewQuickToolBar_->addAction(rightViewAction_);
    viewQuickToolBar_->addSeparator();
    viewQuickToolBar_->addAction(showAxesAction_);
    viewQuickToolBar_->addAction(showBoundingBoxAction_);
    viewQuickToolBar_->addSeparator();
    viewQuickToolBar_->addAction(darkBackgroundAction_);
    viewQuickToolBar_->addAction(lightBackgroundAction_);

    addToolBar(Qt::LeftToolBarArea, viewQuickToolBar_);
}

void MainWindow::createWindowControls()
{
    if (ribbonBar_ == nullptr) {
        return;
    }

    windowControlsWidget_ = new QWidget(ribbonBar_);
    auto* controlsLayout = new QHBoxLayout(windowControlsWidget_);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);

    minimizeButton_ = new QToolButton(windowControlsWidget_);
    maximizeButton_ = new QToolButton(windowControlsWidget_);
    closeButton_ = new QToolButton(windowControlsWidget_);

    const QList<QToolButton*> buttons = { minimizeButton_, maximizeButton_, closeButton_ };
    for (QToolButton* button : buttons) {
        button->setAutoRaise(true);
        button->setCursor(Qt::ArrowCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(34, 26);
        controlsLayout->addWidget(button);
    }

    minimizeButton_->setObjectName(QStringLiteral("windowMinimizeButton"));
    maximizeButton_->setObjectName(QStringLiteral("windowMaximizeButton"));
    closeButton_->setObjectName(QStringLiteral("windowCloseButton"));

    connect(minimizeButton_, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximizeButton_, &QToolButton::clicked, this, [this]() { toggleMaximizedWindow(); });
    connect(closeButton_, &QToolButton::clicked, this, &QWidget::close);

    ribbonBar_->setCornerWidget(windowControlsWidget_, Qt::TopRightCorner);
    updateWindowControlButtons();
}

void MainWindow::updateWindowControlButtons()
{
    if (minimizeButton_ == nullptr || maximizeButton_ == nullptr || closeButton_ == nullptr) {
        return;
    }

    Qtitan::RibbonStyle::Theme theme = Qtitan::RibbonStyle::Office2016White;
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        theme = ribbonStyle->getTheme();
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor iconColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);

    minimizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Minimize, iconColor));
    closeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Close, iconColor));

    if (isMaximized()) {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Restore, iconColor));
        maximizeButton_->setToolTip(tr("Restore Down"));
    } else {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Maximize, iconColor));
        maximizeButton_->setToolTip(tr("Maximize"));
    }

    minimizeButton_->setToolTip(tr("Minimize"));
    closeButton_->setToolTip(tr("Close"));
}

void MainWindow::updateWindowControlAppearance(Qtitan::RibbonStyle::Theme theme)
{
    if (windowControlsWidget_ == nullptr) {
        return;
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QString hoverColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(15, 23, 42, 0.08)");
    const QString pressedColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(15, 23, 42, 0.14)");
    windowControlsWidget_->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "border: none;"
        "background: transparent;"
        "padding: 0;"
        "}"
        "QToolButton:hover {"
        "background: %1;"
        "}"
        "QToolButton:pressed {"
        "background: %2;"
        "}"
        "QToolButton#windowCloseButton:hover {"
        "background: #e11d48;"
        "}"
        "QToolButton#windowCloseButton:pressed {"
        "background: #be123c;"
        "}").arg(hoverColor, pressedColor));
}
