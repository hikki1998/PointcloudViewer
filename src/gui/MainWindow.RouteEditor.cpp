#include "gui/MainWindow.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPointF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>

#include "crs/CrsTransformService.h"
#include "gui/PointCloudViewer.h"
#include "gui/support/UiHelpers.h"

using lasviewer::crs::CrsTransformService;
using lasviewer::gui::enforceLightDialogButtonStyles;

namespace
{
QString routePartDisplayName(const RoutePartPoint& partPoint)
{
    return partPoint.partName.trimmed().isEmpty()
        ? QCoreApplication::translate("MainWindow", "Part %1").arg(QLocale().toString(partPoint.partIndex))
        : partPoint.partName.trimmed();
}

QString routeWaypointPartSummary(const RouteWaypoint& waypoint, const QHash<int, RoutePartPoint>& partPointByIndex)
{
    QStringList partNames;
    for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
        if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
            const QString partName = routePartDisplayName(partPointByIndex.value(captureTarget.partIndex));
            if (!partNames.contains(partName)) {
                partNames.append(partName);
            }
        } else if (!captureTarget.partName.trimmed().isEmpty() && !partNames.contains(captureTarget.partName.trimmed())) {
            partNames.append(captureTarget.partName.trimmed());
        }
    }

    if (partNames.isEmpty() && waypoint.primaryPartIndex > 0 && partPointByIndex.contains(waypoint.primaryPartIndex)) {
        partNames.append(routePartDisplayName(partPointByIndex.value(waypoint.primaryPartIndex)));
    }

    if (partNames.isEmpty()) {
        return waypoint.isHelperWaypoint
            ? QCoreApplication::translate("MainWindow", "Helper Waypoint")
            : QCoreApplication::translate("MainWindow", "Unlinked");
    }

    return partNames.join(QStringLiteral(", "));
}

double normalizedRouteFocalLengthRatio(double ratio)
{
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        return 1.0;
    }
    return std::clamp(ratio, 0.1, 64.0);
}

QString routeCaptureTargetDisplayName(
    const RouteCaptureTarget& captureTarget,
    const QHash<int, RoutePartPoint>& partPointByIndex,
    int targetSequence)
{
    if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
        return routePartDisplayName(partPointByIndex.value(captureTarget.partIndex));
    }
    if (!captureTarget.partName.trimmed().isEmpty()) {
        return captureTarget.partName.trimmed();
    }
    return QCoreApplication::translate("MainWindow", "Target %1").arg(QLocale().toString(targetSequence));
}
}

bool MainWindow::editRouteWaypoint(int waypointIndex)
{
    if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
        return false;
    }

    if (!ensureRouteEditingEnabled(true)) {
        return false;
    }

    if (QDialog* existingDialog = findChild<QDialog*>(QStringLiteral("routeWaypointEditDialog"), Qt::FindDirectChildrenOnly);
        existingDialog != nullptr) {
        existingDialog->close();
    }

    struct RouteWaypointEditState
    {
        RouteWaypoint originalWaypoint;
        int selectedTargetIndex = -1;
        int originalTargetIndex = -1;
        bool hasCaptureTargets = false;
    };

    RouteWaypoint& waypoint = currentPowerlineRoute_.waypoints[waypointIndex];
    const RouteWaypoint originalWaypoint = waypoint;
    const bool hasCaptureTargets = !waypoint.captureTargets.isEmpty();
    const int originalTargetIndex = hasCaptureTargets
        ? std::clamp(selectedRouteWaypointTargetIndex_, 0, waypoint.captureTargets.size() - 1)
        : -1;
    const auto state = std::make_shared<RouteWaypointEditState>();
    state->originalWaypoint = originalWaypoint;
    state->selectedTargetIndex = originalTargetIndex;
    state->originalTargetIndex = originalTargetIndex;
    state->hasCaptureTargets = hasCaptureTargets;
    const RouteCaptureTarget selectedTarget =
        (hasCaptureTargets && state->selectedTargetIndex >= 0 && state->selectedTargetIndex < originalWaypoint.captureTargets.size())
            ? originalWaypoint.captureTargets.at(state->selectedTargetIndex)
            : RouteCaptureTarget();

    QHash<int, RoutePartPoint> partPointByIndex;
    for (const RoutePartPoint& partPoint : currentPowerlineRoute_.partPoints) {
        if (partPoint.partIndex > 0) {
            partPointByIndex.insert(partPoint.partIndex, partPoint);
        }
    }

    auto* dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("routeWaypointEditDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(tr("Edit Route Waypoint"));
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->resize(560, 500);
    dialog->setStyleSheet(QStringLiteral(
        "QDialog {"
        "background-color: #f3f7fb;"
        "}"
        "QFrame#routeEditCard {"
        "background-color: rgba(255, 255, 255, 0.97);"
        "border: 1px solid rgba(148, 163, 184, 0.22);"
        "border-radius: 16px;"
        "}"
        "QLabel#routeEditTitle {"
        "color: #0f172a;"
        "font-size: 20px;"
        "font-weight: 700;"
        "}"
        "QLabel#routeEditSubtitle {"
        "color: #475569;"
        "font-size: 13px;"
        "line-height: 1.4em;"
        "}"
        "QLabel#routeEditHint {"
        "color: #92400e;"
        "background-color: #fffbeb;"
        "border: 1px solid #fcd34d;"
        "border-radius: 10px;"
        "padding: 8px 10px;"
        "}"
        "QLabel {"
        "color: #0f172a;"
        "}"
        "QDoubleSpinBox {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 8px;"
        "padding: 6px 10px;"
        "min-height: 28px;"
        "}"
        "QDoubleSpinBox:disabled {"
        "background-color: #f8fafc;"
        "color: #94a3b8;"
        "}"
        "QTableWidget {"
        "background-color: #ffffff;"
        "alternate-background-color: #f8fafc;"
        "gridline-color: #e2e8f0;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "border: 1px solid #dbe3ee;"
        "border-radius: 10px;"
        "}"
        "QHeaderView::section {"
        "background-color: #e2e8f0;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "padding: 4px 8px;"
        "font-weight: 600;"
        "}"
        "QDialogButtonBox QPushButton {"
        "min-width: 96px;"
        "padding: 8px 18px;"
        "border-radius: 10px;"
        "border: 1px solid #cbd5e1;"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "font-weight: 600;"
        "}"
        "QDialogButtonBox QPushButton:hover {"
        "background-color: #eff6ff;"
        "border-color: #93c5fd;"
        "}"));

    auto* rootLayout = new QVBoxLayout(dialog);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(16);

    auto* headerCard = new QFrame(dialog);
    headerCard->setObjectName(QStringLiteral("routeEditCard"));
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(22, 18, 22, 18);
    headerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(
        tr("Waypoint #%1").arg(QLocale().toString(waypointIndex + 1)),
        headerCard);
    titleLabel->setObjectName(QStringLiteral("routeEditTitle"));
    headerLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(
        tr("Linked part: %1").arg(routeWaypointPartSummary(waypoint, partPointByIndex)),
        headerCard);
    subtitleLabel->setObjectName(QStringLiteral("routeEditSubtitle"));
    subtitleLabel->setWordWrap(true);
    headerLayout->addWidget(subtitleLabel);

    auto* targetCountLabel = new QLabel(
        tr("Linked targets: %1").arg(QLocale().toString(waypoint.captureTargets.size())),
        headerCard);
    targetCountLabel->setObjectName(QStringLiteral("routeEditSubtitle"));
    targetCountLabel->setWordWrap(true);
    headerLayout->addWidget(targetCountLabel);

    if (waypoint.captureTargets.isEmpty()) {
        auto* hintLabel = new QLabel(
            tr("This waypoint has no linked capture target. Camera yaw and camera pitch are read-only for this edit."),
            headerCard);
        hintLabel->setObjectName(QStringLiteral("routeEditHint"));
        hintLabel->setWordWrap(true);
        headerLayout->addWidget(hintLabel);
    }

    rootLayout->addWidget(headerCard);

    QTableWidget* targetTableWidget = nullptr;
    if (hasCaptureTargets) {
        auto* targetsCard = new QFrame(dialog);
        targetsCard->setObjectName(QStringLiteral("routeEditCard"));
        auto* targetsLayout = new QVBoxLayout(targetsCard);
        targetsLayout->setContentsMargins(22, 16, 22, 16);
        targetsLayout->setSpacing(8);

        auto* targetsTitleLabel = new QLabel(tr("Capture Targets"), targetsCard);
        targetsTitleLabel->setObjectName(QStringLiteral("routeEditSubtitle"));
        targetsLayout->addWidget(targetsTitleLabel);

        targetTableWidget = new QTableWidget(targetsCard);
        targetTableWidget->setColumnCount(5);
        targetTableWidget->setAlternatingRowColors(true);
        targetTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        targetTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        targetTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        targetTableWidget->verticalHeader()->setVisible(false);
        targetTableWidget->setHorizontalHeaderLabels({
            tr("#"),
            tr("Part"),
            tr("Focal Ratio"),
            tr("Camera Yaw"),
            tr("Camera Pitch")
        });
        targetTableWidget->horizontalHeader()->setStretchLastSection(false);
        targetTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        targetTableWidget->setColumnWidth(0, 54);
        targetTableWidget->setColumnWidth(1, 210);
        targetTableWidget->setColumnWidth(2, 108);
        targetTableWidget->setColumnWidth(3, 112);
        targetTableWidget->setColumnWidth(4, 112);

        targetTableWidget->setRowCount(waypoint.captureTargets.size());
        auto createReadOnlyItem = [](const QString& text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            return item;
        };

        for (int targetIndex = 0; targetIndex < waypoint.captureTargets.size(); ++targetIndex) {
            const RouteCaptureTarget& captureTarget = waypoint.captureTargets.at(targetIndex);
            const QString targetName = routeCaptureTargetDisplayName(captureTarget, partPointByIndex, targetIndex + 1);
            targetTableWidget->setItem(
                targetIndex,
                0,
                createReadOnlyItem(QLocale().toString(targetIndex + 1), Qt::AlignCenter));
            targetTableWidget->setItem(
                targetIndex,
                1,
                createReadOnlyItem(targetName));
            targetTableWidget->setItem(
                targetIndex,
                2,
                createReadOnlyItem(QLocale().toString(captureTarget.focalLengthRatio, 'f', 2), Qt::AlignRight | Qt::AlignVCenter));
            targetTableWidget->setItem(
                targetIndex,
                3,
                createReadOnlyItem(QLocale().toString(captureTarget.cameraYawDeg, 'f', 2), Qt::AlignRight | Qt::AlignVCenter));
            targetTableWidget->setItem(
                targetIndex,
                4,
                createReadOnlyItem(QLocale().toString(captureTarget.cameraPitchDeg, 'f', 2), Qt::AlignRight | Qt::AlignVCenter));
        }

        targetTableWidget->setCurrentCell(std::max(0, state->selectedTargetIndex), 1);
        targetsLayout->addWidget(targetTableWidget, 1);
        rootLayout->addWidget(targetsCard, 1);
    }

    auto* formCard = new QFrame(dialog);
    formCard->setObjectName(QStringLiteral("routeEditCard"));
    auto* formLayout = new QFormLayout(formCard);
    formLayout->setContentsMargins(22, 18, 22, 18);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto configureCoordinateSpin = [](QDoubleSpinBox* spinBox, double value) {
        spinBox->setRange(-1000000000.0, 1000000000.0);
        spinBox->setDecimals(3);
        spinBox->setSingleStep(0.5);
        spinBox->setValue(value);
    };
    auto configureAngleSpin = [](QDoubleSpinBox* spinBox, double minimum, double maximum, double value) {
        spinBox->setRange(minimum, maximum);
        spinBox->setDecimals(2);
        spinBox->setSingleStep(1.0);
        spinBox->setValue(value);
    };
    auto configureFocalRatioSpin = [](QDoubleSpinBox* spinBox, double value) {
        spinBox->setRange(0.1, 64.0);
        spinBox->setDecimals(2);
        spinBox->setSingleStep(0.1);
        spinBox->setValue(normalizedRouteFocalLengthRatio(value));
    };

    auto* xSpinBox = new QDoubleSpinBox(formCard);
    auto* ySpinBox = new QDoubleSpinBox(formCard);
    auto* zSpinBox = new QDoubleSpinBox(formCard);
    auto* aircraftYawSpinBox = new QDoubleSpinBox(formCard);
    auto* gimbalPitchSpinBox = new QDoubleSpinBox(formCard);
    auto* focalLengthRatioSpinBox = new QDoubleSpinBox(formCard);
    auto* cameraYawSpinBox = new QDoubleSpinBox(formCard);
    auto* cameraPitchSpinBox = new QDoubleSpinBox(formCard);

    configureCoordinateSpin(xSpinBox, waypoint.localPoint.x);
    configureCoordinateSpin(ySpinBox, waypoint.localPoint.y);
    configureCoordinateSpin(zSpinBox, waypoint.localPoint.z);
    configureAngleSpin(aircraftYawSpinBox, -360.0, 360.0, waypoint.aircraftYawDeg);
    configureAngleSpin(gimbalPitchSpinBox, -180.0, 180.0, waypoint.gimbalPitchDeg);
    configureFocalRatioSpin(focalLengthRatioSpinBox, selectedTarget.focalLengthRatio);
    configureAngleSpin(cameraYawSpinBox, -360.0, 360.0, selectedTarget.cameraYawDeg);
    configureAngleSpin(cameraPitchSpinBox, -180.0, 180.0, selectedTarget.cameraPitchDeg);

    focalLengthRatioSpinBox->setEnabled(hasCaptureTargets);
    cameraYawSpinBox->setEnabled(hasCaptureTargets);
    cameraPitchSpinBox->setEnabled(hasCaptureTargets);

    formLayout->addRow(tr("X"), xSpinBox);
    formLayout->addRow(tr("Y"), ySpinBox);
    formLayout->addRow(tr("Z"), zSpinBox);
    formLayout->addRow(tr("Aircraft Yaw"), aircraftYawSpinBox);
    formLayout->addRow(tr("Gimbal Pitch"), gimbalPitchSpinBox);
    formLayout->addRow(tr("Focal Ratio"), focalLengthRatioSpinBox);
    formLayout->addRow(tr("Camera Yaw"), cameraYawSpinBox);
    formLayout->addRow(tr("Camera Pitch"), cameraPitchSpinBox);
    rootLayout->addWidget(formCard, 1);

    const auto refreshTargetTableRow = [this, targetTableWidget, waypointIndex](int row) {
        if (targetTableWidget == nullptr
            || waypointIndex < 0
            || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        const RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints.at(waypointIndex);
        if (row < 0 || row >= currentWaypoint.captureTargets.size()) {
            return;
        }

        const RouteCaptureTarget& captureTarget = currentWaypoint.captureTargets.at(row);
        if (QTableWidgetItem* focalItem = targetTableWidget->item(row, 2); focalItem != nullptr) {
            focalItem->setText(QLocale().toString(captureTarget.focalLengthRatio, 'f', 2));
        }
        if (QTableWidgetItem* yawItem = targetTableWidget->item(row, 3); yawItem != nullptr) {
            yawItem->setText(QLocale().toString(captureTarget.cameraYawDeg, 'f', 2));
        }
        if (QTableWidgetItem* pitchItem = targetTableWidget->item(row, 4); pitchItem != nullptr) {
            pitchItem->setText(QLocale().toString(captureTarget.cameraPitchDeg, 'f', 2));
        }
    };

    const auto applyCurrentEditorValues = [this,
                                           waypointIndex,
                                           state,
                                           xSpinBox,
                                           ySpinBox,
                                           zSpinBox,
                                           aircraftYawSpinBox,
                                           gimbalPitchSpinBox,
                                           focalLengthRatioSpinBox,
                                           cameraYawSpinBox,
                                           cameraPitchSpinBox,
                                           refreshTargetTableRow]() {
        if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints[waypointIndex];
        currentWaypoint.localPoint.x = xSpinBox->value();
        currentWaypoint.localPoint.y = ySpinBox->value();
        currentWaypoint.localPoint.z = zSpinBox->value();
        currentWaypoint.dh = zSpinBox->value();
        currentWaypoint.height = zSpinBox->value();
        currentWaypoint.aircraftYawDeg = aircraftYawSpinBox->value();
        currentWaypoint.gimbalPitchDeg = gimbalPitchSpinBox->value();

        if (state->hasCaptureTargets) {
            for (RouteCaptureTarget& captureTarget : currentWaypoint.captureTargets) {
                captureTarget.aircraftYawDeg = currentWaypoint.aircraftYawDeg;
                captureTarget.gimbalPitchDeg = currentWaypoint.gimbalPitchDeg;
            }

            if (state->selectedTargetIndex >= 0 && state->selectedTargetIndex < currentWaypoint.captureTargets.size()) {
                RouteCaptureTarget& activeTarget = currentWaypoint.captureTargets[state->selectedTargetIndex];
                activeTarget.focalLengthRatio = normalizedRouteFocalLengthRatio(focalLengthRatioSpinBox->value());
                activeTarget.cameraYawDeg = cameraYawSpinBox->value();
                activeTarget.cameraPitchDeg = cameraPitchSpinBox->value();
                refreshTargetTableRow(state->selectedTargetIndex);
            }
        }

        selectedRouteWaypointIndex_ = waypointIndex;
        selectedRouteWaypointTargetIndex_ = state->selectedTargetIndex;
        applyCurrentRouteToViewer();
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
        }
        updateRoutePlanningPanel();
    };

    const auto syncSelectedTargetFromTable = [this,
                                              waypointIndex,
                                              state,
                                              targetTableWidget,
                                              focalLengthRatioSpinBox,
                                              cameraYawSpinBox,
                                              cameraPitchSpinBox]() {
        if (!state->hasCaptureTargets
            || targetTableWidget == nullptr
            || waypointIndex < 0
            || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        const RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints.at(waypointIndex);
        int row = targetTableWidget->currentRow();
        if (row < 0 || row >= currentWaypoint.captureTargets.size()) {
            row = 0;
        }

        state->selectedTargetIndex = row;
        const RouteCaptureTarget& captureTarget = currentWaypoint.captureTargets.at(state->selectedTargetIndex);
        const QSignalBlocker focalRatioBlocker(focalLengthRatioSpinBox);
        const QSignalBlocker yawBlocker(cameraYawSpinBox);
        const QSignalBlocker pitchBlocker(cameraPitchSpinBox);
        focalLengthRatioSpinBox->setValue(normalizedRouteFocalLengthRatio(captureTarget.focalLengthRatio));
        cameraYawSpinBox->setValue(captureTarget.cameraYawDeg);
        cameraPitchSpinBox->setValue(captureTarget.cameraPitchDeg);

        selectedRouteWaypointTargetIndex_ = state->selectedTargetIndex;
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
        }
    };

    if (targetTableWidget != nullptr) {
        connect(targetTableWidget, &QTableWidget::currentCellChanged, dialog, [syncSelectedTargetFromTable](int, int, int, int) {
            syncSelectedTargetFromTable();
        });
        syncSelectedTargetFromTable();
    }

    const auto connectRealtimePreview = [this, dialog, applyCurrentEditorValues](QDoubleSpinBox* spinBox) {
        QObject::connect(
            spinBox,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            dialog,
            [this, applyCurrentEditorValues](double) {
            applyCurrentEditorValues();
        });
    };

    connectRealtimePreview(xSpinBox);
    connectRealtimePreview(ySpinBox);
    connectRealtimePreview(zSpinBox);
    connectRealtimePreview(aircraftYawSpinBox);
    connectRealtimePreview(gimbalPitchSpinBox);
    connectRealtimePreview(focalLengthRatioSpinBox);
    connectRealtimePreview(cameraYawSpinBox);
    connectRealtimePreview(cameraPitchSpinBox);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
    QPushButton* resetButton = buttonBox->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, dialog, [this,
                                                          waypointIndex,
                                                          state,
                                                          targetTableWidget,
                                                          xSpinBox,
                                                          ySpinBox,
                                                          zSpinBox,
                                                          aircraftYawSpinBox,
                                                          gimbalPitchSpinBox,
                                                          focalLengthRatioSpinBox,
                                                          cameraYawSpinBox,
                                                          cameraPitchSpinBox,
                                                          refreshTargetTableRow,
                                                          applyCurrentEditorValues]() {
        if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints[waypointIndex];
        currentWaypoint = state->originalWaypoint;
        state->selectedTargetIndex = state->originalTargetIndex;

        if (targetTableWidget != nullptr && state->selectedTargetIndex >= 0) {
            const QSignalBlocker tableBlocker(targetTableWidget);
            targetTableWidget->setCurrentCell(state->selectedTargetIndex, 1);
        }

        const QSignalBlocker xBlocker(xSpinBox);
        const QSignalBlocker yBlocker(ySpinBox);
        const QSignalBlocker zBlocker(zSpinBox);
        const QSignalBlocker aircraftYawBlocker(aircraftYawSpinBox);
        const QSignalBlocker gimbalPitchBlocker(gimbalPitchSpinBox);
        const QSignalBlocker focalRatioBlocker(focalLengthRatioSpinBox);
        const QSignalBlocker cameraYawBlocker(cameraYawSpinBox);
        const QSignalBlocker cameraPitchBlocker(cameraPitchSpinBox);

        xSpinBox->setValue(state->originalWaypoint.localPoint.x);
        ySpinBox->setValue(state->originalWaypoint.localPoint.y);
        zSpinBox->setValue(state->originalWaypoint.localPoint.z);
        aircraftYawSpinBox->setValue(state->originalWaypoint.aircraftYawDeg);
        gimbalPitchSpinBox->setValue(state->originalWaypoint.gimbalPitchDeg);
        if (state->hasCaptureTargets
            && state->selectedTargetIndex >= 0
            && state->selectedTargetIndex < currentWaypoint.captureTargets.size()) {
            const RouteCaptureTarget& captureTarget = currentWaypoint.captureTargets.at(state->selectedTargetIndex);
            focalLengthRatioSpinBox->setValue(normalizedRouteFocalLengthRatio(captureTarget.focalLengthRatio));
            cameraYawSpinBox->setValue(captureTarget.cameraYawDeg);
            cameraPitchSpinBox->setValue(captureTarget.cameraPitchDeg);
        } else {
            focalLengthRatioSpinBox->setValue(1.0);
            cameraYawSpinBox->setValue(0.0);
            cameraPitchSpinBox->setValue(0.0);
        }

        if (targetTableWidget != nullptr) {
            for (int row = 0; row < currentWaypoint.captureTargets.size(); ++row) {
                refreshTargetTableRow(row);
            }
        }

        applyCurrentEditorValues();
    });
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    rootLayout->addWidget(buttonBox);
    enforceLightDialogButtonStyles(dialog);

    connect(dialog, &QDialog::finished, this, [this, waypointIndex, state, applyCurrentEditorValues](int result) {
        if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints[waypointIndex];
        if (result != QDialog::Accepted) {
            currentWaypoint = state->originalWaypoint;
            selectedRouteWaypointIndex_ = waypointIndex;
            selectedRouteWaypointTargetIndex_ = state->originalTargetIndex;
            applyCurrentRouteToViewer();
            if (viewer_ != nullptr) {
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
            }
            updateRoutePlanningPanel();
            return;
        }

        applyCurrentEditorValues();

        QString geographicWarning;
        if (projectCoordinateSystems_.pointCloudCrs.code > 0 && projectCoordinateSystems_.geographicCrs.code > 0) {
            QPointF geographicPoint;
            QString errorMessage;
            if (CrsTransformService::transformPoint(
                    projectCoordinateSystems_.pointCloudCrs,
                    projectCoordinateSystems_.geographicCrs,
                    QPointF(currentWaypoint.localPoint.x, currentWaypoint.localPoint.y),
                    &geographicPoint,
                    &errorMessage)) {
                currentWaypoint.longitude = geographicPoint.x();
                currentWaypoint.latitude = geographicPoint.y();
            } else {
                geographicWarning = errorMessage.isEmpty()
                    ? tr("Waypoint local position was updated, but geographic coordinates could not be synchronized.")
                    : errorMessage;
            }
        }

        selectedRouteWaypointIndex_ = waypointIndex;
        selectedRouteWaypointTargetIndex_ = state->selectedTargetIndex;
        applyCurrentRouteToViewer();
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
        }
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        if (viewer_ != nullptr) {
            viewer_->focusOnPoint(currentWaypoint.localPoint, 0.2);
        }

        if (!geographicWarning.isEmpty()) {
            showUserMessage(LogLevel::Warning, geographicWarning, 4500);
        } else {
            showUserMessage(
                LogLevel::Info,
                tr("Updated route waypoint #%1.").arg(QLocale().toString(waypointIndex + 1)),
                3000);
        }
    });

    dialog->open();
    dialog->raise();
    dialog->activateWindow();
    return true;
}
