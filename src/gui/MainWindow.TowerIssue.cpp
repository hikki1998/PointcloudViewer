#include "gui/MainWindow.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <set>

#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"

using namespace mainwindow_internal;

void MainWindow::updateTowerPanel()
{
    if (viewer_ == nullptr || towerTableWidget_ == nullptr || towerCountValueLabel_ == nullptr || towerToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<TowerMarker>& towerMarkers = viewer_->towerMarkers();
    towerCountValueLabel_->setText(
        towerMarkers.isEmpty()
            ? tr("No tower markers yet. Use the icon tools above to add one from the point cloud.")
            : tr("%1 tower marker(s)").arg(QLocale().toString(towerMarkers.size())));

    switch (viewer_->towerEditMode()) {
    case TowerEditMode::AddAfterLast:
        towerToolStatusLabel_->setText(tr("Tower tool: click points in the view continuously to add tower markers. Cancel the tool when finished."));
        break;
    case TowerEditMode::InsertBeforeSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to insert a tower marker before current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::MoveSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to move current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::None:
    default:
        towerToolStatusLabel_->setText(
            towerEditingEnabled_
                ? tr("Tower editing active. Select the current tower, then use the toolbar above to insert, move, rename, focus, or remove it.")
                : tr("Tower editing is off. Use the Ribbon to start editing before changing tower markers."));
        break;
    }

    const int selectedRow = viewer_->selectedTowerIndex();
    const QSignalBlocker blocker(towerTableWidget_);
    towerTableWidget_->setEditTriggers(static_cast<QAbstractItemView::EditTriggers>(
        towerEditingEnabled_
            ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed)
            : QAbstractItemView::NoEditTriggers));
    towerTableWidget_->setRowCount(0);
    for (int towerIndex = 0; towerIndex < towerMarkers.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers.at(towerIndex);
        towerTableWidget_->insertRow(towerIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(towerMarker.index));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        towerTableWidget_->setItem(towerIndex, 0, indexItem);

        auto* nameItem = new QTableWidgetItem(towerMarker.name);
        if (towerEditingEnabled_) {
            nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        } else {
            nameItem->setFlags((nameItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        }
        towerTableWidget_->setItem(towerIndex, 1, nameItem);

        const QStringList coordinateTexts = {
            formatCoordinate(towerMarker.point.x),
            formatCoordinate(towerMarker.point.y),
            formatCoordinate(towerMarker.point.z)
        };
        for (int coordinateColumn = 0; coordinateColumn < coordinateTexts.size(); ++coordinateColumn) {
            auto* coordinateItem = new QTableWidgetItem(coordinateTexts.at(coordinateColumn));
            coordinateItem->setFlags((coordinateItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            coordinateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            towerTableWidget_->setItem(towerIndex, coordinateColumn + 2, coordinateItem);
        }
    }

    if (towerTableWidget_->rowCount() > 0) {
        towerTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, towerTableWidget_->rowCount() - 1), 1);
    } else {
        towerTableWidget_->clearSelection();
    }

    updateTowerDetailEditor();
}

QString MainWindow::nextDefaultTowerName() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingNames;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        existingNames.insert(towerMarker.name.trimmed());
    }

    int towerIndex = 1;
    while (true) {
        const QString candidate = tr("Tower %1").arg(QLocale().toString(towerIndex));
        if (existingNames.find(candidate) == existingNames.end()) {
            return candidate;
        }
        ++towerIndex;
    }
}

QString MainWindow::nextDefaultIssueTitle() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingTitles;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        existingTitles.insert(issue.title.trimmed());
    }

    int issueIndex = 1;
    while (true) {
        const QString candidate = tr("Issue %1").arg(QLocale().toString(issueIndex));
        if (existingTitles.find(candidate) == existingTitles.end()) {
            return candidate;
        }
        ++issueIndex;
    }
}

void MainWindow::updateTowerDetailEditor()
{
    if (viewer_ == nullptr || towerCodeEdit_ == nullptr || towerTypeComboBox_ == nullptr) {
        return;
    }

    updatingTowerDetails_ = true;
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const bool hasSelection = selectedTowerIndex >= 0 && selectedTowerIndex < towers.size();

    const TowerRecord towerRecord = hasSelection ? towers.at(selectedTowerIndex) : TowerRecord();
    towerCodeEdit_->setText(towerRecord.code);
    towerLineNameEdit_->setText(towerRecord.lineName);
    towerVoltageLevelEdit_->setText(towerRecord.voltageLevel);
    const int towerTypeOption = towerTypeComboBox_->findData(static_cast<int>(towerRecord.towerType));
    towerTypeComboBox_->setCurrentIndex(towerTypeOption >= 0 ? towerTypeOption : 0);
    towerStructureTypeEdit_->setText(towerRecord.structureType);
    towerInspectionDateEdit_->setText(towerRecord.inspectionDate);
    towerStatusEdit_->setText(towerRecord.status);
    towerNotesEdit_->setPlainText(towerRecord.notes);

    const QList<QWidget*> editors = {
        towerCodeEdit_,
        towerLineNameEdit_,
        towerVoltageLevelEdit_,
        towerTypeComboBox_,
        towerStructureTypeEdit_,
        towerInspectionDateEdit_,
        towerStatusEdit_,
        towerNotesEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection && towerEditingEnabled_);
        }
    }
    updatingTowerDetails_ = false;
}

void MainWindow::updateIssuePanel()
{
    if (viewer_ == nullptr || issueTableWidget_ == nullptr || issueCountValueLabel_ == nullptr || issueToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    issueCountValueLabel_->setText(
        issues.isEmpty()
            ? tr("No inspection issues yet. Use the toolbar above to mark one from the point cloud.")
            : tr("%1 inspection issue(s)").arg(QLocale().toString(issues.size())));
    issueToolStatusLabel_->setText(
        viewer_->issueEditMode() == IssueEditMode::Add
            ? tr("Issue tool: click points in the view continuously to add inspection issues. Right-click to cancel the tool.")
            : tr("Select an issue to edit its business details, focus the scene, or export reports."));

    const int selectedRow = viewer_->selectedIssueIndex();
    const QSignalBlocker blocker(issueTableWidget_);
    issueTableWidget_->setRowCount(0);
    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        issueTableWidget_->insertRow(issueIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(issueIndex + 1));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        issueTableWidget_->setItem(issueIndex, 0, indexItem);

        auto* titleItem = new QTableWidgetItem(issue.title);
        titleItem->setFlags((titleItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        issueTableWidget_->setItem(issueIndex, 1, titleItem);

        const QStringList rowValues = {
            issueSeverityDisplayName(issue.severity),
            issueStatusDisplayName(issue.status),
            issue.relatedTowerName,
            issue.category
        };
        for (int valueIndex = 0; valueIndex < rowValues.size(); ++valueIndex) {
            auto* valueItem = new QTableWidgetItem(rowValues.at(valueIndex));
            valueItem->setFlags((valueItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            issueTableWidget_->setItem(issueIndex, valueIndex + 2, valueItem);
        }
    }

    if (issueTableWidget_->rowCount() > 0 && selectedRow >= 0) {
        issueTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, issueTableWidget_->rowCount() - 1), 1);
    } else {
        issueTableWidget_->clearSelection();
    }

    updateIssueDetailEditor();
}

void MainWindow::updateIssueDetailEditor()
{
    if (viewer_ == nullptr || issueTitleEdit_ == nullptr) {
        return;
    }

    updatingIssueDetails_ = true;
    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    const bool hasSelection = selectedIssueIndex >= 0 && selectedIssueIndex < issues.size();
    const InspectionIssue issue = hasSelection ? issues.at(selectedIssueIndex) : InspectionIssue();

    issueTitleEdit_->setText(issue.title);
    issueCategoryComboBox_->setEditText(issue.category);
    issueSeverityComboBox_->setCurrentIndex(static_cast<int>(issue.severity));
    issueStatusComboBox_->setCurrentIndex(static_cast<int>(issue.status));
    issueImagePathEdit_->setText(issue.imagePath);
    issueDescriptionEdit_->setPlainText(issue.description);
    issueLocationValueLabel_->setText(
        hasSelection ? formatTriplet(issue.point.x, issue.point.y, issue.point.z) : tr("N/A"));
    issueCreatedAtValueLabel_->setText(hasSelection ? issue.createdAt : tr("N/A"));

    issueRelatedTowerComboBox_->clear();
    issueRelatedTowerComboBox_->addItem(tr("None"), -1);
    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        issueRelatedTowerComboBox_->addItem(towers.at(towerIndex).name, towerIndex);
    }
    const int relatedTowerOption = issueRelatedTowerComboBox_->findData(issue.relatedTowerIndex);
    issueRelatedTowerComboBox_->setCurrentIndex(relatedTowerOption >= 0 ? relatedTowerOption : 0);

    const QList<QWidget*> editors = {
        issueTitleEdit_,
        issueCategoryComboBox_,
        issueSeverityComboBox_,
        issueStatusComboBox_,
        issueRelatedTowerComboBox_,
        issueImagePathEdit_,
        issueDescriptionEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection);
        }
    }
    updatingIssueDetails_ = false;
}

void MainWindow::setTowerEditingEnabled(bool enabled)
{
    if (towerEditingEnabled_ == enabled) {
        return;
    }

    towerEditingEnabled_ = enabled;
    if (!towerEditingEnabled_ && viewer_ != nullptr) {
        viewer_->cancelTowerEditMode();
    }

    updateActionState();
    updateTowerPanel();
}
