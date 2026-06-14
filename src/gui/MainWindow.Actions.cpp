#include "gui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QStyle>

#include "gui/support/RibbonIconFactory.h"

using lasviewer::gui::RibbonGlyph;
using lasviewer::gui::createRibbonIcon;

void MainWindow::createActions()
{
    openAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Open"), this);
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setToolTip(tr("Open a point cloud, route file, or project"));
    addPointCloudAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Add LAS Files"), this);
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
    removeDatasetAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Selected Dataset"), this);
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_ = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Folder"), this);
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Copy Path"), this);
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Expand All"), this);
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Collapse All"), this);
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));

    openProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open Project"), this);
    saveProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project"), this);
    saveProjectAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project As"), this);
    projectCoordinateSystemsAction_ = new QAction(
        createRibbonIcon(RibbonGlyph::Open),
        tr("Project Management"),
        this);
    projectCoordinateSystemsAction_->setToolTip(tr("Open project management in the backstage view"));

    clearAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear"), this);
    clearAction_->setToolTip(tr("Clear the current scene"));

    exitAction_ = new QAction(createRibbonIcon(RibbonGlyph::Exit), tr("Exit"), this);
    exitAction_->setShortcut(QKeySequence::Quit);

    fitSceneAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Fit Scene"), this);
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));

    topViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Top), tr("Top"), this);
    topViewAction_->setToolTip(tr("Switch to top view"));
    frontViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Front), tr("Front"), this);
    frontViewAction_->setToolTip(tr("Switch to front view"));
    rightViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Right), tr("Right"), this);
    rightViewAction_->setToolTip(tr("Switch to right view"));

    showAxesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Axes), tr("Axes"), this);
    showAxesAction_->setCheckable(true);
    showAxesAction_->setToolTip(tr("Show or hide XYZ axes"));

    showBoundingBoxAction_ = new QAction(createRibbonIcon(RibbonGlyph::Bounds), tr("Bounds"), this);
    showBoundingBoxAction_->setCheckable(true);
    showBoundingBoxAction_->setToolTip(tr("Show or hide point cloud bounds"));

    captureScreenshotAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Screenshot"), this);
    captureScreenshotAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    captureScreenshotAction_->setShortcutContext(Qt::WindowShortcut);
    captureScreenshotAction_->setToolTip(tr("Capture the current application window as a PNG image"));

    toggleScreenRecordingAction_ = new QAction(createRibbonIcon(RibbonGlyph::Log), tr("Start Recording"), this);
    toggleScreenRecordingAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    toggleScreenRecordingAction_->setShortcutContext(Qt::WindowShortcut);
    toggleScreenRecordingAction_->setToolTip(tr("Start or stop MP4 screen recording for the current application window"));

    darkBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::DarkBackground), tr("Dark"), this);
    darkBackgroundAction_->setToolTip(tr("Switch to dark background"));
    lightBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::LightBackground), tr("Light"), this);
    lightBackgroundAction_->setToolTip(tr("Switch to light background"));

    colorModeActionGroup_ = new QActionGroup(this);
    colorModeActionGroup_->setExclusive(true);

    rgbColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Rgb), tr("RGB"), this);
    rgbColorAction_->setCheckable(true);
    elevationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Elevation), tr("Elevation"), this);
    elevationColorAction_->setCheckable(true);
    singleColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::SingleColor), tr("Single"), this);
    singleColorAction_->setCheckable(true);
    classificationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Classification"), this);
    classificationColorAction_->setCheckable(true);

    colorModeActionGroup_->addAction(rgbColorAction_);
    colorModeActionGroup_->addAction(elevationColorAction_);
    colorModeActionGroup_->addAction(singleColorAction_);
    colorModeActionGroup_->addAction(classificationColorAction_);

    themeActionGroup_ = new QActionGroup(this);
    themeActionGroup_->setExclusive(true);

    themeColorfulAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeColorful), tr("Colorful"), this);
    themeColorfulAction_->setCheckable(true);
    themeWhiteAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeWhite), tr("White"), this);
    themeWhiteAction_->setCheckable(true);
    themeDarkGrayAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeDarkGray), tr("Dark Gray"), this);
    themeDarkGrayAction_->setCheckable(true);

    themeActionGroup_->addAction(themeColorfulAction_);
    themeActionGroup_->addAction(themeWhiteAction_);
    themeActionGroup_->addAction(themeDarkGrayAction_);

    measureAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Measure"), this);
    measureAction_->setCheckable(true);
    profileClassificationAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Profile Classify"), this);
    profileClassificationAction_->setCheckable(true);
    profileClassificationAction_->setToolTip(tr("Enable profile classification and choose rectangle or polygon selection in the panel"));
    showProfileClassificationDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Classify Panel"), this);
    showProfileClassificationDockAction_->setCheckable(true);
    showProfileClassificationDockAction_->setChecked(false);
    saveProfileClassificationEditsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Classify Result"), this);
    undoProfileClassificationAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Undo Classify"), this);
    redoProfileClassificationAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Redo Classify"), this);
    clearProfileClassificationEditsAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Classify Edits"), this);

    clearMeasurementAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Measure"), this);
    exportClearanceCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Clearance CSV"), this);
    showProfileDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Profile View"), this);
    showProfileDockAction_->setObjectName(QStringLiteral("showProfileDockAction"));
    showProfileDockAction_->setCheckable(true);
    showProfileDockAction_->setChecked(false);
    showWebPanelAction_ = new QAction(createRibbonIcon(RibbonGlyph::Settings), tr("Web Panel"), this);
    showWebPanelAction_->setObjectName(QStringLiteral("showWebPanelAction"));
    showWebPanelAction_->setCheckable(true);
    showWebPanelAction_->setChecked(false);
    showWebPanelAction_->setToolTip(tr("Show or hide the embedded web panel"));
    analyzeVegetationRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Analyze Risks"), this);
    analyzeVegetationRisksAction_->setToolTip(tr("Analyze vegetation risks around the current measured corridor"));
    focusVegetationRiskAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Risk"), this);
    createIssueFromRiskAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Create Issue"), this);
    createIssuesFromRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Create All Issues"), this);
    clearVegetationRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Risks"), this);
    generateInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Generate Route"), this);
    regenerateInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Regenerate Route"), this);
    clearInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Route"), this);
    toggleRouteEditingAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerAdjust), tr("Edit Route"), this);
    toggleRouteEditingAction_->setCheckable(true);
    toggleRouteEditingAction_->setChecked(routeEditingEnabled_);
    toggleRouteEditingAction_->setToolTip(tr("Enable waypoint edit, delete, and drag operations for the current route"));
    startInspectionRouteRoamAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Start Roam"), this);
    pauseInspectionRouteRoamAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Pause Roam"), this);
    stopInspectionRouteRoamAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Stop Roam"), this);
    focusRouteWaypointAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Route Point"), this);
    importRouteFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Route File"), this);
    saveRouteFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Route File"), this);
    saveRouteFileAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Route File As"), this);
    reloadRouteFileAction_ = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Reload Route File"), this);
    importRouteKmlAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Route KML"), this);
    exportRouteKmlAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Route KML"), this);
    exportRouteDjiKmzAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export DJI KMZ"), this);

    startTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Start Editing"), this);
    finishTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Finish Editing"), this);
    addTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerAdd), tr("Click To Add Tower"), this);
    insertTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerInsert), tr("Insert Before Current"), this);
    moveTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerMove), tr("Move Current Tower"), this);
    editCurrentTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerAdjust), tr("Edit Current Tower"), this);
    focusTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerFocus), tr("Focus Current Tower"), this);
    removeTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerRemove), tr("Remove Current Tower"), this);
    clearTowersAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Tower Markers"), this);
    cancelTowerToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Tower Tool"), this);
    importTowerFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Tower File"), this);
    saveTowerFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Tower File"), this);
    saveTowerFileAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Tower File As"), this);
    reloadTowerFileAction_ = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Reload Tower File"), this);
    showTowerXAction_ = new QAction(tr("Show X"), this);
    showTowerYAction_ = new QAction(tr("Show Y"), this);
    showTowerZAction_ = new QAction(tr("Show Z"), this);
    showTowerXAction_->setCheckable(true);
    showTowerYAction_->setCheckable(true);
    showTowerZAction_->setCheckable(true);
    startIssueMarkAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Mark Issue"), this);
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Issue Tool"), this);
    focusIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Issue"), this);
    removeIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Current Issue"), this);
    clearIssuesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Issues"), this);
    exportIssuesCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Issues CSV"), this);
    exportInspectionReportAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Inspection Report"), this);

    showLogAction_ = new QAction(createRibbonIcon(RibbonGlyph::Log), tr("Log"), this);
    showLogAction_->setCheckable(true);
    showLogAction_->setChecked(false);
    showLogAction_->setToolTip(tr("Show or hide the log panel"));

    languageActionGroup_ = new QActionGroup(this);
    languageActionGroup_->setExclusive(true);

    languageEnglishAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), tr("English"), this);
    languageEnglishAction_->setCheckable(true);
    languageChineseAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), tr("Chinese"), this);
    languageChineseAction_->setCheckable(true);

    languageActionGroup_->addAction(languageEnglishAction_);
    languageActionGroup_->addAction(languageChineseAction_);

    clipModeActionGroup_ = new QActionGroup(this);
    clipModeActionGroup_->setExclusive(true);

    clipModeNoneAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogCancelButton), tr("No Clip"), this);
    clipModeNoneAction_->setCheckable(true);
    clipModeNoneAction_->setChecked(true);
    clipModeNoneAction_->setToolTip(tr("Disable clipping and show all points"));

    clipModeBoxAction_ = new QAction(createRibbonIcon(RibbonGlyph::Bounds), tr("Box Clip"), this);
    clipModeBoxAction_->setCheckable(true);
    clipModeBoxAction_->setToolTip(tr("Pick two point-cloud points to build a clipping box. Preview is shown before the second click."));

    clipModePolygonAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Polygon Clip"), this);
    clipModePolygonAction_->setCheckable(true);
    clipModePolygonAction_->setToolTip(tr("Draw a screen-space polygon in the current view. The finished polygon is frozen into a 3D clip volume."));

    clipModeActionGroup_->addAction(clipModeNoneAction_);
    clipModeActionGroup_->addAction(clipModeBoxAction_);
    clipModeActionGroup_->addAction(clipModePolygonAction_);

    clipBoxAlignmentActionGroup_ = new QActionGroup(this);
    clipBoxAlignmentActionGroup_->setExclusive(true);

    clipBoxWorldAlignedAction_ = new QAction(createRibbonIcon(RibbonGlyph::Top), tr("World Aligned"), this);
    clipBoxWorldAlignedAction_->setCheckable(true);
    clipBoxWorldAlignedAction_->setChecked(true);
    clipBoxWorldAlignedAction_->setToolTip(tr("Build the clip box aligned to the world XYZ axes."));

    clipBoxViewAlignedAction_ = new QAction(createRibbonIcon(RibbonGlyph::Front), tr("View Aligned"), this);
    clipBoxViewAlignedAction_->setCheckable(true);
    clipBoxViewAlignedAction_->setToolTip(tr("Build the clip box aligned to the camera axes captured at the first click."));

    clipBoxAlignmentActionGroup_->addAction(clipBoxWorldAlignedAction_);
    clipBoxAlignmentActionGroup_->addAction(clipBoxViewAlignedAction_);

    clipScopeActionGroup_ = new QActionGroup(this);
    clipScopeActionGroup_->setExclusive(true);

    clipScopeActiveDatasetAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Active Dataset"), this);
    clipScopeActiveDatasetAction_->setCheckable(true);
    clipScopeActiveDatasetAction_->setToolTip(tr("Apply clipping only to the currently selected point-cloud dataset."));

    clipScopeVisibleDatasetsAction_ = new QAction(createRibbonIcon(RibbonGlyph::Bounds), tr("Visible Datasets"), this);
    clipScopeVisibleDatasetsAction_->setCheckable(true);
    clipScopeVisibleDatasetsAction_->setChecked(true);
    clipScopeVisibleDatasetsAction_->setToolTip(tr("Apply clipping to all currently visible point-cloud datasets."));

    clipScopeActionGroup_->addAction(clipScopeActiveDatasetAction_);
    clipScopeActionGroup_->addAction(clipScopeVisibleDatasetsAction_);

    clipToggleInsideAction_ = new QAction(createRibbonIcon(RibbonGlyph::Elevation), tr("Keep Inside"), this);
    clipToggleInsideAction_->setCheckable(true);
    clipToggleInsideAction_->setChecked(true);
    clipToggleInsideAction_->setToolTip(tr("Switch between Keep Inside and Keep Outside for the current clip region."));

    clipApplyExportAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Apply & Export"), this);
    clipApplyExportAction_->setToolTip(tr("Export the currently clipped result as a new LAS file. The exported file is added to the project tree."));
}
