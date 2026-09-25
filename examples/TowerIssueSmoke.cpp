#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QOpenGLWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QSlider>
#include <QMouseEvent>
#include <QMenu>
#include <QSpinBox>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTranslator>
#include <QTimer>
#include <QLineEdit>

#include <osgGA/TrackballManipulator>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

#include "crs/CrsAuthorityService.h"
#include "domain/InspectionData.h"
#include "domain/TowerFileInterop.h"
#include "gui/ApplicationLogDock.h"
#include "gui/IssueController.h"
#define private public
#include "gui/MainWindow.h"
#include "gui/PointCloudViewer.h"
#undef private
#include "gui/MeasurementAnalysisController.h"
#include "gui/NavigationSettingsWidget.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/RouteController.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerController.h"
#include "gui/VisualizationPanelController.h"
#include "logging/ApplicationLogger.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

#include "QtnRibbonBackstageView.h"
#include "QtnRibbonBar.h"
#include "QtnRibbonSystemPopupBar.h"

#include "SmokeTestSupport.h"

bool runTowerControllerSmoke(const QStringList&)
{
    QAction startTowerEditAction(QStringLiteral("Start Edit"), nullptr);
    QAction finishTowerEditAction(QStringLiteral("Finish Edit"), nullptr);
    QAction addTowerAction(QStringLiteral("Add Tower"), nullptr);
    QAction insertTowerAction(QStringLiteral("Insert Tower"), nullptr);
    QAction moveTowerAction(QStringLiteral("Move Tower"), nullptr);
    QAction editCurrentTowerAction(QStringLiteral("Edit Current"), nullptr);
    QAction focusTowerAction(QStringLiteral("Focus Tower"), nullptr);
    QAction removeTowerAction(QStringLiteral("Remove Tower"), nullptr);
    QAction clearTowersAction(QStringLiteral("Clear Towers"), nullptr);
    QAction cancelTowerToolAction(QStringLiteral("Cancel Tool"), nullptr);
    QAction importTowerFileAction(QStringLiteral("Import"), nullptr);
    QAction saveTowerFileAction(QStringLiteral("Save"), nullptr);
    QAction saveTowerFileAsAction(QStringLiteral("Save As"), nullptr);
    QAction reloadTowerFileAction(QStringLiteral("Reload"), nullptr);
    QAction showTowerXAction(QStringLiteral("Show X"), nullptr);
    QAction showTowerYAction(QStringLiteral("Show Y"), nullptr);
    QAction showTowerZAction(QStringLiteral("Show Z"), nullptr);
    showTowerXAction.setCheckable(true);
    showTowerYAction.setCheckable(true);
    showTowerZAction.setCheckable(true);

    QTableWidget towerTableWidget(2, 5);
    towerTableWidget.setItem(0, 1, new QTableWidgetItem(QStringLiteral("T-001")));
    towerTableWidget.setItem(1, 1, new QTableWidgetItem(QStringLiteral("T-002")));

    QLineEdit towerCodeEdit;
    QLineEdit towerLineNameEdit;
    QLineEdit towerVoltageLevelEdit;
    QComboBox towerTypeComboBox;
    towerTypeComboBox.addItem(QStringLiteral("Unknown"), 0);
    towerTypeComboBox.addItem(QStringLiteral("Tangent"), 1);
    QLineEdit towerStructureTypeEdit;
    QLineEdit towerInspectionDateEdit;
    QLineEdit towerStatusEdit;
    QPlainTextEdit towerNotesEdit;

    int startEditCount = 0;
    int finishEditCount = 0;
    int addTowerCount = 0;
    int insertTowerCount = 0;
    int moveTowerCount = 0;
    int editCurrentCount = 0;
    int focusTowerCount = 0;
    int removeTowerCount = 0;
    int clearTowersCount = 0;
    int cancelToolCount = 0;
    int importTowerFileCount = 0;
    int saveTowerFileCount = 0;
    int saveTowerFileAsCount = 0;
    int reloadTowerFileCount = 0;
    int showColumnToggleCount = 0;
    int selectionChangedCount = 0;
    int latestSelectedRow = -1;
    int towerNameEditedCount = 0;
    int latestEditedRow = -1;
    QString latestEditedName;
    int commitTowerDetailsCount = 0;

    TowerController controller(
        &startTowerEditAction,
        &finishTowerEditAction,
        &addTowerAction,
        &insertTowerAction,
        &moveTowerAction,
        &editCurrentTowerAction,
        &focusTowerAction,
        &removeTowerAction,
        &clearTowersAction,
        &cancelTowerToolAction,
        &importTowerFileAction,
        &saveTowerFileAction,
        &saveTowerFileAsAction,
        &reloadTowerFileAction,
        &showTowerXAction,
        &showTowerYAction,
        &showTowerZAction,
        &towerTableWidget,
        &towerCodeEdit,
        &towerLineNameEdit,
        &towerVoltageLevelEdit,
        &towerTypeComboBox,
        &towerStructureTypeEdit,
        &towerInspectionDateEdit,
        &towerStatusEdit,
        &towerNotesEdit,
        [&startEditCount]() { ++startEditCount; },
        [&finishEditCount]() { ++finishEditCount; },
        [&addTowerCount]() { ++addTowerCount; },
        [&insertTowerCount]() { ++insertTowerCount; },
        [&moveTowerCount]() { ++moveTowerCount; },
        [&editCurrentCount]() { ++editCurrentCount; },
        [&focusTowerCount]() { ++focusTowerCount; },
        [&removeTowerCount]() { ++removeTowerCount; },
        [&clearTowersCount]() { ++clearTowersCount; },
        [&cancelToolCount]() { ++cancelToolCount; },
        [&importTowerFileCount]() { ++importTowerFileCount; },
        [&saveTowerFileCount]() { ++saveTowerFileCount; },
        [&saveTowerFileAsCount]() { ++saveTowerFileAsCount; },
        [&reloadTowerFileCount]() { ++reloadTowerFileCount; },
        [&showColumnToggleCount](bool) { ++showColumnToggleCount; },
        [&showColumnToggleCount](bool) { ++showColumnToggleCount; },
        [&showColumnToggleCount](bool) { ++showColumnToggleCount; },
        [&selectionChangedCount, &latestSelectedRow](int currentRow) {
            ++selectionChangedCount;
            latestSelectedRow = currentRow;
        },
        [&towerNameEditedCount, &latestEditedRow, &latestEditedName](int row, const QString& name) {
            ++towerNameEditedCount;
            latestEditedRow = row;
            latestEditedName = name;
        },
        [&commitTowerDetailsCount]() {
            ++commitTowerDetailsCount;
        });
    Q_UNUSED(controller);

    startTowerEditAction.trigger();
    finishTowerEditAction.trigger();
    addTowerAction.trigger();
    insertTowerAction.trigger();
    moveTowerAction.trigger();
    editCurrentTowerAction.trigger();
    focusTowerAction.trigger();
    removeTowerAction.trigger();
    clearTowersAction.trigger();
    cancelTowerToolAction.trigger();
    importTowerFileAction.trigger();
    saveTowerFileAction.trigger();
    saveTowerFileAsAction.trigger();
    reloadTowerFileAction.trigger();

    if (!verify(startEditCount == 1 && finishEditCount == 1, "Tower controller should forward start/finish edit actions")) {
        return false;
    }
    if (!verify(addTowerCount == 1 && insertTowerCount == 1 && moveTowerCount == 1, "Tower controller should forward tower edit mode actions")) {
        return false;
    }
    if (!verify(editCurrentCount == 1 && focusTowerCount == 1 && removeTowerCount == 1, "Tower controller should forward current tower operations")) {
        return false;
    }
    if (!verify(clearTowersCount == 1 && cancelToolCount == 1, "Tower controller should forward clear/cancel actions")) {
        return false;
    }
    if (!verify(
            importTowerFileCount == 1
                && saveTowerFileCount == 1
                && saveTowerFileAsCount == 1
                && reloadTowerFileCount == 1,
            "Tower controller should forward tower file actions")) {
        return false;
    }

    showTowerXAction.setChecked(true);
    showTowerYAction.setChecked(true);
    showTowerZAction.setChecked(true);
    showTowerXAction.setChecked(false);
    showTowerYAction.setChecked(false);
    showTowerZAction.setChecked(false);
    if (!verify(towerTableWidget.isColumnHidden(2), "Tower controller should toggle X column visibility")) {
        return false;
    }
    if (!verify(towerTableWidget.isColumnHidden(3), "Tower controller should toggle Y column visibility")) {
        return false;
    }
    if (!verify(towerTableWidget.isColumnHidden(4), "Tower controller should toggle Z column visibility")) {
        return false;
    }
    if (!verify(showColumnToggleCount == 6, "Tower controller should invoke visibility callbacks for each toggle transition")) {
        return false;
    }

    towerTableWidget.setCurrentCell(1, 1);
    if (!verify(selectionChangedCount > 0 && latestSelectedRow == 1, "Tower controller should forward table selection changes")) {
        return false;
    }

    if (QTableWidgetItem* item = towerTableWidget.item(1, 1)) {
        item->setText(QStringLiteral("T-002-EDITED"));
    }
    if (!verify(
            towerNameEditedCount > 0 && latestEditedRow == 1 && latestEditedName == QStringLiteral("T-002-EDITED"),
            "Tower controller should forward tower name edits")) {
        return false;
    }

    const int commitBaseline = commitTowerDetailsCount;
    towerTypeComboBox.setCurrentIndex(1);
    towerNotesEdit.setPlainText(QStringLiteral("updated"));
    if (!verify(commitTowerDetailsCount >= commitBaseline + 2, "Tower controller should forward detail field edits")) {
        return false;
    }

    std::cout << "[PASS] Tower controller smoke test completed." << std::endl;
    return true;
}

bool runIssueControllerSmoke(const QStringList&)
{
    QAction startIssueMarkAction(QStringLiteral("Mark Issue"), nullptr);
    QAction cancelIssueToolAction(QStringLiteral("Cancel"), nullptr);
    QAction focusIssueAction(QStringLiteral("Focus"), nullptr);
    QAction removeIssueAction(QStringLiteral("Remove"), nullptr);
    QAction clearIssuesAction(QStringLiteral("Clear"), nullptr);
    QAction exportIssuesCsvAction(QStringLiteral("Export CSV"), nullptr);
    QAction exportInspectionReportAction(QStringLiteral("Export Report"), nullptr);

    QTableWidget issueTableWidget(2, 6);
    issueTableWidget.setItem(0, 1, new QTableWidgetItem(QStringLiteral("Issue 1")));
    issueTableWidget.setItem(1, 1, new QTableWidgetItem(QStringLiteral("Issue 2")));

    QLineEdit issueTitleEdit;
    QComboBox issueCategoryComboBox;
    issueCategoryComboBox.setEditable(true);
    issueCategoryComboBox.addItem(QStringLiteral("Other"));
    issueCategoryComboBox.addItem(QStringLiteral("Vegetation"));
    QComboBox issueSeverityComboBox;
    issueSeverityComboBox.addItem(QStringLiteral("Info"));
    issueSeverityComboBox.addItem(QStringLiteral("Major"));
    QComboBox issueStatusComboBox;
    issueStatusComboBox.addItem(QStringLiteral("Open"));
    issueStatusComboBox.addItem(QStringLiteral("Resolved"));
    QComboBox issueRelatedTowerComboBox;
    issueRelatedTowerComboBox.addItem(QStringLiteral("None"), -1);
    issueRelatedTowerComboBox.addItem(QStringLiteral("T-001"), 0);
    QLineEdit issueImagePathEdit;
    QPlainTextEdit issueDescriptionEdit;

    int beginIssueMarkingCount = 0;
    int cancelIssueToolCount = 0;
    int focusSelectedIssueCount = 0;
    int removeSelectedIssueCount = 0;
    int clearAllIssuesCount = 0;
    int exportIssuesCsvCount = 0;
    int exportInspectionReportCount = 0;
    int issueSelectionChangedCount = 0;
    int latestIssueSelection = -1;
    int commitIssueDetailsCount = 0;

    IssueController controller(
        &startIssueMarkAction,
        &cancelIssueToolAction,
        &focusIssueAction,
        &removeIssueAction,
        &clearIssuesAction,
        &exportIssuesCsvAction,
        &exportInspectionReportAction,
        &issueTableWidget,
        &issueTitleEdit,
        &issueCategoryComboBox,
        &issueSeverityComboBox,
        &issueStatusComboBox,
        &issueRelatedTowerComboBox,
        &issueImagePathEdit,
        &issueDescriptionEdit,
        [&beginIssueMarkingCount]() { ++beginIssueMarkingCount; },
        [&cancelIssueToolCount]() { ++cancelIssueToolCount; },
        [&focusSelectedIssueCount]() { ++focusSelectedIssueCount; },
        [&removeSelectedIssueCount]() { ++removeSelectedIssueCount; },
        [&clearAllIssuesCount]() { ++clearAllIssuesCount; },
        [&exportIssuesCsvCount]() { ++exportIssuesCsvCount; },
        [&exportInspectionReportCount]() { ++exportInspectionReportCount; },
        [&issueSelectionChangedCount, &latestIssueSelection](int row) {
            ++issueSelectionChangedCount;
            latestIssueSelection = row;
        },
        [&commitIssueDetailsCount]() { ++commitIssueDetailsCount; });
    Q_UNUSED(controller);

    startIssueMarkAction.trigger();
    cancelIssueToolAction.trigger();
    focusIssueAction.trigger();
    removeIssueAction.trigger();
    clearIssuesAction.trigger();
    exportIssuesCsvAction.trigger();
    exportInspectionReportAction.trigger();

    if (!verify(beginIssueMarkingCount == 1, "Issue controller should forward start issue marking action")) {
        return false;
    }
    if (!verify(cancelIssueToolCount == 1, "Issue controller should forward cancel issue tool action")) {
        return false;
    }
    if (!verify(focusSelectedIssueCount == 1, "Issue controller should forward focus issue action")) {
        return false;
    }
    if (!verify(removeSelectedIssueCount == 1 && clearAllIssuesCount == 1, "Issue controller should forward remove/clear issue actions")) {
        return false;
    }
    if (!verify(exportIssuesCsvCount == 1 && exportInspectionReportCount == 1, "Issue controller should forward issue export actions")) {
        return false;
    }

    issueTableWidget.setCurrentCell(1, 1);
    if (!verify(issueSelectionChangedCount > 0 && latestIssueSelection == 1, "Issue controller should forward issue table selection")) {
        return false;
    }

    const int commitBaseline = commitIssueDetailsCount;
    issueTitleEdit.setText(QStringLiteral("Updated Issue"));
    issueTitleEdit.editingFinished();
    issueCategoryComboBox.setEditText(QStringLiteral("Vegetation"));
    issueSeverityComboBox.setCurrentIndex(1);
    issueStatusComboBox.setCurrentIndex(1);
    issueRelatedTowerComboBox.setCurrentIndex(1);
    issueImagePathEdit.setText(QStringLiteral("images/issue.jpg"));
    issueImagePathEdit.editingFinished();
    issueDescriptionEdit.setPlainText(QStringLiteral("updated notes"));
    if (!verify(commitIssueDetailsCount >= commitBaseline + 7, "Issue controller should forward detail edits")) {
        return false;
    }

    std::cout << "[PASS] Issue controller smoke test completed." << std::endl;
    return true;
}

bool runTowerFileInteropSmoke(const QStringList&)
{
    QList<TowerRecord> expectedTowers;
    TowerRecord tower0;
    tower0.index = 0;
    tower0.name = QStringLiteral("#001");
    tower0.point.x = 100.5f;
    tower0.point.y = 200.25f;
    tower0.point.z = 300.125f;
    tower0.towerType = TowerType::Unknown;
    expectedTowers.append(tower0);

    TowerRecord tower1;
    tower1.index = 1;
    tower1.name = QStringLiteral("#002");
    tower1.point.x = 110.5f;
    tower1.point.y = 210.25f;
    tower1.point.z = 310.125f;
    tower1.towerType = TowerType::Tangent;
    expectedTowers.append(tower1);

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString towerPath = QDir(tempDir.path()).filePath(QStringLiteral("tower.LiTower"));
    QString errorMessage;
    if (!exportTowerLiTowerFile(towerPath, expectedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(towerPath), "Exported tower file should exist")) {
        return false;
    }

    QFile file(towerPath);
    if (!verify(file.open(QIODevice::ReadOnly | QIODevice::Text), "Exported tower file should be readable")) {
        return false;
    }
    const QString firstLine = QString::fromUtf8(file.readLine()).trimmed();
    file.close();
    if (!verify(firstLine == QStringLiteral("Index,X,Y,Z,Type,Name"), "Tower file header should match LiTower format")) {
        return false;
    }

    QList<TowerRecord> importedTowers;
    if (!importTowerLiTowerFile(towerPath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedTowers.size() == expectedTowers.size(), "Imported tower count mismatch")) {
        return false;
    }

    for (int index = 0; index < expectedTowers.size(); ++index) {
        const TowerRecord& expected = expectedTowers.at(index);
        const TowerRecord& actual = importedTowers.at(index);
        if (!verify(actual.index == expected.index, "Tower index mismatch")) {
            return false;
        }
        if (!verify(actual.name == expected.name, "Tower name mismatch")) {
            return false;
        }
        if (!verify(actual.towerType == expected.towerType, "Tower type mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.x - expected.point.x) < 1e-4f, "Tower X mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.y - expected.point.y) < 1e-4f, "Tower Y mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.z - expected.point.z) < 1e-4f, "Tower Z mismatch")) {
            return false;
        }
    }

    std::cout << "[PASS] Tower file interop smoke test completed." << std::endl;
    return true;
}

bool runTowerProjectLinkSmoke(const QStringList&)
{
    const QString sourceTowerFilePath = QFileInfo(
        QDir::current().absoluteFilePath(QStringLiteral("templates/tower.LiTower"))).absoluteFilePath();
    if (!verify(QFileInfo::exists(sourceTowerFilePath), "templates/tower.LiTower should exist")) {
        return false;
    }

    QList<TowerRecord> importedTowers;
    QString errorMessage;
    if (!importTowerLiTowerFile(sourceTowerFilePath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(importedTowers.size() >= 4, "Expected at least 4 towers from template")) {
        return false;
    }
    if (!verify(importedTowers.first().index == 44, "Template first index should be 44 before editing")) {
        return false;
    }

    normalizeTowerIndices(&importedTowers);
    if (!verify(importedTowers.first().index == 0, "After edit normalization, first index should be 0")) {
        return false;
    }
    if (!verify(importedTowers.last().index == importedTowers.size() - 1, "After edit normalization, last index should be N-1")) {
        return false;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString projectFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_project.lpproj"));
    const QString linkedTowerFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_linked.LiTower"));
    if (!exportTowerLiTowerFile(linkedTowerFilePath, importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile initial: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    QJsonArray towersArray;
    for (const TowerRecord& towerRecord : importedTowers) {
        towersArray.append(towerRecordToJson(towerRecord));
    }

    QJsonArray pointCloudFilesArray;
    pointCloudFilesArray.append(QStringLiteral("./test_data/ezhou_powerline_sample.las"));
    QJsonObject towerFileObject {
        { QStringLiteral("format"), QStringLiteral("LiTower") },
        { QStringLiteral("relativePath"), QStringLiteral("./tower_linked.LiTower") }
    };

    QJsonObject projectObject {
        { QStringLiteral("version"), 8 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("towerFile"), towerFileObject },
        { QStringLiteral("towerMarkers"), towersArray }
    };

    QFile projectFile(projectFilePath);
    if (!verify(projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Project file should be writable")) {
        return false;
    }
    projectFile.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    projectFile.close();

    QFile projectFileRead(projectFilePath);
    if (!verify(projectFileRead.open(QIODevice::ReadOnly), "Project file should be readable")) {
        return false;
    }
    const QJsonDocument loadedDocument = QJsonDocument::fromJson(projectFileRead.readAll());
    projectFileRead.close();
    if (!verify(loadedDocument.isObject(), "Loaded project JSON must be an object")) {
        return false;
    }

    const QJsonObject loadedProject = loadedDocument.object();
    const QString loadedRelativeTowerPath = loadedProject.value(QStringLiteral("towerFile")).toObject().value(QStringLiteral("relativePath")).toString();
    const QString resolvedTowerPath = resolveProjectPath(projectFilePath, loadedRelativeTowerPath);
    if (!verify(QFileInfo::exists(resolvedTowerPath), "Resolved linked tower file should exist")) {
        return false;
    }

    QList<TowerRecord> loadedTowerRecords;
    const QJsonArray loadedTowersArray = loadedProject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : loadedTowersArray) {
        loadedTowerRecords.append(towerRecordFromJson(towerValue.toObject()));
    }
    if (!verify(loadedTowerRecords.size() == importedTowers.size(), "Loaded tower record count mismatch")) {
        return false;
    }
    if (!verify(loadedTowerRecords.first().index == 0, "Loaded project first index should be 0")) {
        return false;
    }

    if (!exportTowerLiTowerFile(resolvedTowerPath, loadedTowerRecords, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile sync: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    QFile linkedTowerFile(resolvedTowerPath);
    if (!verify(linkedTowerFile.open(QIODevice::ReadOnly | QIODevice::Text), "Linked tower file should be readable")) {
        return false;
    }
    QTextStream stream(&linkedTowerFile);
    stream.setCodec("UTF-8");
    const QString header = stream.readLine().trimmed();
    const QString firstRow = stream.readLine().trimmed();
    linkedTowerFile.close();

    if (!verify(header == QStringLiteral("Index,X,Y,Z,Type,Name"), "Linked tower header should match LiTower format")) {
        return false;
    }
    if (!verify(firstRow.startsWith(QStringLiteral("0,")), "First linked tower row should start with index 0")) {
        return false;
    }

    std::cout << "[PASS] Tower project link smoke test completed." << std::endl;
    return true;
}
