#include "gui/ProfileClassificationController.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>

#include <utility>

#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationWidget.h"

namespace
{
QString trMainWindow(const char* sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

ProfileClassificationController::ProfileClassificationController(
    ProfileClassificationWidget* panel,
    PointCloudViewer* viewer,
    QAction* profileClassificationAction,
    QAction* saveProfileClassificationEditsAction,
    QAction* undoProfileClassificationAction,
    QAction* redoProfileClassificationAction,
    QAction* clearProfileClassificationEditsAction,
    ClassificationNameResolver classificationNameResolver,
    QObject* parent)
    : QObject(parent)
    , panel_(panel)
    , viewer_(viewer)
    , profileClassificationAction_(profileClassificationAction)
    , saveProfileClassificationEditsAction_(saveProfileClassificationEditsAction)
    , undoProfileClassificationAction_(undoProfileClassificationAction)
    , redoProfileClassificationAction_(redoProfileClassificationAction)
    , clearProfileClassificationEditsAction_(clearProfileClassificationEditsAction)
    , classificationNameResolver_(std::move(classificationNameResolver))
{
    if (panel_ == nullptr || viewer_ == nullptr) {
        return;
    }

    if (profileClassificationAction_ != nullptr) {
        connect(profileClassificationAction_, &QAction::toggled, viewer_, &PointCloudViewer::setProfileClassificationModeEnabled);
    }

    const auto requestSave = [this]() {
        if (viewer_ == nullptr || viewer_->classificationEditedPointCount() <= 0) {
            return;
        }
        emit saveRequested();
    };
    if (saveProfileClassificationEditsAction_ != nullptr) {
        connect(saveProfileClassificationEditsAction_, &QAction::triggered, this, requestSave);
    }
    if (panel_->saveButton() != nullptr) {
        connect(panel_->saveButton(), &QPushButton::clicked, this, requestSave);
    }

    if (undoProfileClassificationAction_ != nullptr) {
        connect(undoProfileClassificationAction_, &QAction::triggered, viewer_, &PointCloudViewer::undoClassificationEdit);
    }
    if (redoProfileClassificationAction_ != nullptr) {
        connect(redoProfileClassificationAction_, &QAction::triggered, viewer_, &PointCloudViewer::redoClassificationEdit);
    }
    if (clearProfileClassificationEditsAction_ != nullptr) {
        connect(clearProfileClassificationEditsAction_, &QAction::triggered, viewer_, &PointCloudViewer::clearClassificationEdits);
    }
    if (panel_->undoButton() != nullptr) {
        connect(panel_->undoButton(), &QPushButton::clicked, viewer_, &PointCloudViewer::undoClassificationEdit);
    }
    if (panel_->redoButton() != nullptr) {
        connect(panel_->redoButton(), &QPushButton::clicked, viewer_, &PointCloudViewer::redoClassificationEdit);
    }
    if (panel_->clearEditsButton() != nullptr) {
        connect(panel_->clearEditsButton(), &QPushButton::clicked, viewer_, &PointCloudViewer::clearClassificationEdits);
    }

    if (panel_->toggleButton() != nullptr) {
        connect(panel_->toggleButton(), &QPushButton::clicked, this, [this]() {
            if (viewer_ == nullptr) {
                return;
            }

            viewer_->setProfileClassificationModeEnabled(!viewer_->profileClassificationModeEnabled());
        });
    }

    if (panel_->selectAllButton() != nullptr) {
        connect(panel_->selectAllButton(), &QPushButton::clicked, this, [this]() {
            QListWidget* sourceList = panel_ != nullptr ? panel_->sourceListWidget() : nullptr;
            if (sourceList == nullptr) {
                return;
            }

            for (int row = 0; row < sourceList->count(); ++row) {
                if (QListWidgetItem* item = sourceList->item(row)) {
                    item->setCheckState(Qt::Checked);
                }
            }
        });
    }

    if (panel_->clearSelectionButton() != nullptr) {
        connect(panel_->clearSelectionButton(), &QPushButton::clicked, this, [this]() {
            QListWidget* sourceList = panel_ != nullptr ? panel_->sourceListWidget() : nullptr;
            if (sourceList == nullptr) {
                return;
            }

            for (int row = 0; row < sourceList->count(); ++row) {
                if (QListWidgetItem* item = sourceList->item(row)) {
                    item->setCheckState(Qt::Unchecked);
                }
            }
        });
    }

    if (panel_->sourceListWidget() != nullptr) {
        connect(panel_->sourceListWidget(), &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
            QListWidget* sourceList = panel_ != nullptr ? panel_->sourceListWidget() : nullptr;
            if (viewer_ == nullptr || sourceList == nullptr) {
                return;
            }

            QSet<int> selectedSourceClasses;
            for (int row = 0; row < sourceList->count(); ++row) {
                QListWidgetItem* item = sourceList->item(row);
                if (item != nullptr && item->checkState() == Qt::Checked) {
                    selectedSourceClasses.insert(item->data(Qt::UserRole).toInt());
                }
            }
            viewer_->setProfileClassificationSourceClasses(selectedSourceClasses);
        });
    }

    if (panel_->targetListWidget() != nullptr) {
        connect(panel_->targetListWidget(), &QListWidget::currentRowChanged, this, [this](int currentRow) {
            QListWidget* targetList = panel_ != nullptr ? panel_->targetListWidget() : nullptr;
            if (viewer_ == nullptr || targetList == nullptr || currentRow < 0) {
                return;
            }

            QListWidgetItem* item = targetList->item(currentRow);
            if (item != nullptr) {
                viewer_->setProfileClassificationTargetClass(item->data(Qt::UserRole).toInt());
            }
        });
    }

    if (panel_->modeComboBox() != nullptr) {
        connect(panel_->modeComboBox(), qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
            QComboBox* modeCombo = panel_ != nullptr ? panel_->modeComboBox() : nullptr;
            if (viewer_ == nullptr || modeCombo == nullptr) {
                return;
            }

            const ProfileClassificationSelectionMode mode = static_cast<ProfileClassificationSelectionMode>(
                modeCombo->currentData().toInt());
            viewer_->setProfileClassificationSelectionMode(mode);
        });
    }

    connect(viewer_, &PointCloudViewer::profileClassificationModeChanged, this, [this](bool enabled) {
        if (profileClassificationAction_ != nullptr) {
            const QSignalBlocker blocker(profileClassificationAction_);
            profileClassificationAction_->setChecked(enabled);
        }
        emit modeChanged(enabled);
        refreshPanel(classificationEditsDirty_);
        emit stateChanged();
    });
    connect(viewer_, &PointCloudViewer::classificationEditsChanged, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        classificationEditsDirty_ = viewer_->classificationEditedPointCount() > 0;
        emit editsDirtyChanged(classificationEditsDirty_);
        refreshPanel(classificationEditsDirty_);
        emit stateChanged();
    });
    connect(viewer_, &PointCloudViewer::profileClassificationStateChanged, this, [this]() {
        refreshPanel(classificationEditsDirty_);
        emit stateChanged();
    });

    classificationEditsDirty_ = viewer_->classificationEditedPointCount() > 0;
    refreshPanel(classificationEditsDirty_);
}

void ProfileClassificationController::initializeClassificationItems(const QList<int>& classificationCodes)
{
    if (panel_ == nullptr) {
        return;
    }

    QListWidget* sourceList = panel_->sourceListWidget();
    QListWidget* targetList = panel_->targetListWidget();
    if (sourceList == nullptr || targetList == nullptr) {
        return;
    }

    const QSignalBlocker sourceBlocker(sourceList);
    const QSignalBlocker targetBlocker(targetList);
    sourceList->clear();
    targetList->clear();

    for (int classificationCode : classificationCodes) {
        auto* sourceItem = new QListWidgetItem(sourceList);
        sourceItem->setData(Qt::UserRole, classificationCode);
        sourceItem->setFlags(sourceItem->flags() | Qt::ItemIsUserCheckable);
        sourceItem->setCheckState(Qt::Unchecked);

        auto* targetItem = new QListWidgetItem(targetList);
        targetItem->setData(Qt::UserRole, classificationCode);
    }

    refreshPanel(classificationEditsDirty_);
}

void ProfileClassificationController::retranslateUi()
{
    if (panel_ != nullptr) {
        panel_->retranslateUi();
    }
    refreshPanel(classificationEditsDirty_);
}

void ProfileClassificationController::refreshPanel(bool classificationEditsDirty)
{
    classificationEditsDirty_ = classificationEditsDirty;
    if (panel_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const ProfileClassificationSelectionMode selectionMode = viewer_->profileClassificationSelectionMode();
    const bool polygonMode = selectionMode == ProfileClassificationSelectionMode::Polygon;
    const bool hasPointCloud = viewer_->hasPointCloud();
    const bool sceneReady = hasPointCloud;
    const bool toolBusy = viewer_->profileClassificationTaskActive();

    panel_->setTitle(trMainWindow("3D Profile Classification"));
    if (panel_->toggleButton() != nullptr) {
        panel_->toggleButton()->setText(
            viewer_->profileClassificationModeEnabled()
                ? trMainWindow("Exit Tool")
                : trMainWindow("Start Tool"));
        panel_->toggleButton()->setEnabled(sceneReady && !toolBusy);
    }
    if (panel_->selectAllButton() != nullptr) {
        panel_->selectAllButton()->setText(trMainWindow("Select All"));
        panel_->selectAllButton()->setEnabled(sceneReady && !toolBusy);
    }
    if (panel_->clearSelectionButton() != nullptr) {
        panel_->clearSelectionButton()->setText(trMainWindow("Clear Sources"));
        panel_->clearSelectionButton()->setEnabled(sceneReady && !toolBusy);
    }
    if (panel_->undoButton() != nullptr) {
        panel_->undoButton()->setText(trMainWindow("Undo"));
        panel_->undoButton()->setEnabled(sceneReady && viewer_->canUndoClassificationEdits() && !toolBusy);
    }
    if (panel_->redoButton() != nullptr) {
        panel_->redoButton()->setText(trMainWindow("Redo"));
        panel_->redoButton()->setEnabled(sceneReady && viewer_->canRedoClassificationEdits() && !toolBusy);
    }
    if (panel_->clearEditsButton() != nullptr) {
        panel_->clearEditsButton()->setText(trMainWindow("Clear Edits"));
        panel_->clearEditsButton()->setEnabled(sceneReady && viewer_->classificationEditedPointCount() > 0 && !toolBusy);
    }
    if (panel_->saveButton() != nullptr) {
        panel_->saveButton()->setText(trMainWindow("Save Result"));
        panel_->saveButton()->setEnabled(hasPointCloud && viewer_->classificationEditedPointCount() > 0 && !toolBusy);
    }

    panel_->setEnabled(hasPointCloud);

    if (QComboBox* modeCombo = panel_->modeComboBox()) {
        const QSignalBlocker blocker(modeCombo);
        const int modeIndex = modeCombo->findData(static_cast<int>(selectionMode));
        modeCombo->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        modeCombo->setEnabled(sceneReady && !toolBusy);
    }

    if (QListWidget* sourceList = panel_->sourceListWidget()) {
        const QSignalBlocker blocker(sourceList);
        for (int row = 0; row < sourceList->count(); ++row) {
            QListWidgetItem* item = sourceList->item(row);
            if (item == nullptr) {
                continue;
            }

            const int classificationCode = item->data(Qt::UserRole).toInt();
            const QString classificationName = classificationNameResolver_
                ? classificationNameResolver_(classificationCode)
                : QLocale().toString(classificationCode);
            item->setText(QStringLiteral("%1 - %2")
                .arg(QLocale().toString(classificationCode))
                .arg(classificationName));
            item->setCheckState(viewer_->profileClassificationSourceClasses().contains(classificationCode)
                ? Qt::Checked
                : Qt::Unchecked);
        }
        sourceList->setEnabled(sceneReady && !toolBusy);
    }

    if (QListWidget* targetList = panel_->targetListWidget()) {
        const QSignalBlocker blocker(targetList);
        int targetRow = -1;
        for (int row = 0; row < targetList->count(); ++row) {
            QListWidgetItem* item = targetList->item(row);
            if (item == nullptr) {
                continue;
            }

            const int classificationCode = item->data(Qt::UserRole).toInt();
            const QString classificationName = classificationNameResolver_
                ? classificationNameResolver_(classificationCode)
                : QLocale().toString(classificationCode);
            item->setText(QStringLiteral("%1 - %2")
                .arg(QLocale().toString(classificationCode))
                .arg(classificationName));
            if (classificationCode == viewer_->profileClassificationTargetClass()) {
                targetRow = row;
            }
        }
        targetList->setCurrentRow(targetRow >= 0 ? targetRow : 0);
        targetList->setEnabled(sceneReady && !toolBusy);
    }

    if (QLabel* statusLabel = panel_->statusLabel()) {
        if (!hasPointCloud) {
            statusLabel->setText(trMainWindow("Load a point cloud and switch to a stable scene before using profile classification."));
        } else if (toolBusy) {
            statusLabel->setText(
                polygonMode
                    ? trMainWindow("Profile classification is processing the current polygon selection.")
                    : trMainWindow("Profile classification is processing the current rectangular selection."));
        } else {
            statusLabel->setText(
                trMainWindow("Mode %1 | Source classes %2 | Target class %3 | Edited points %4 | Save state %5")
                    .arg(polygonMode ? trMainWindow("Polygon") : trMainWindow("Rectangle"))
                    .arg(QLocale().toString(viewer_->profileClassificationSourceClasses().size()))
                    .arg(QLocale().toString(viewer_->profileClassificationTargetClass()))
                    .arg(QLocale().toString(viewer_->classificationEditedPointCount()))
                    .arg(classificationEditsDirty_ ? trMainWindow("unsaved") : trMainWindow("saved")));
        }
    }
}