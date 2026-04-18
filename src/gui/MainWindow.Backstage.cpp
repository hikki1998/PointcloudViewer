#include "gui/MainWindow.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequence>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "QtnRibbonBackstageView.h"
#include "QtnRibbonBar.h"
#include "QtnRibbonSystemPopupBar.h"
#include "gui/BackstageAboutWidget.h"
#include "gui/BackstageApplicationSettingsWidget.h"
#include "gui/BackstageOpenActionsWidget.h"
#include "gui/BackstageOpenProjectWidget.h"
#include "gui/BackstagePageHeaderWidget.h"
#include "gui/BackstageProjectPropertiesWidget.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/support/RibbonIconFactory.h"
#include "gui/support/SettingsKeys.h"
#include "gui/support/UiHelpers.h"

using namespace mainwindow_internal;
using lasviewer::gui::RibbonGlyph;
using lasviewer::gui::createRibbonIcon;
using lasviewer::gui::showLightStyledMessageBox;
using lasviewer::gui::showStyledExistingDirectoryDialog;
using lasviewer::gui::showStyledOpenFileNameDialog;
namespace settingskeys = lasviewer::gui::settingskeys;

namespace
{
void normalizeBackstageNavigationButton(Qtitan::RibbonBackstageView* backstageView, QAction* action)
{
    if (backstageView == nullptr || action == nullptr) {
        return;
    }

    const QList<Qtitan::RibbonBackstageButton*> buttons = backstageView->findChildren<Qtitan::RibbonBackstageButton*>();
    for (Qtitan::RibbonBackstageButton* button : buttons) {
        if (button == nullptr || button->defaultAction() != action) {
            continue;
        }

        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(20, 20));
        button->setMinimumHeight(38);
        button->setTabStyle(true);
        button->setFlatStyle(false);
        button->updateGeometry();
        button->update();
    }
}
}
void MainWindow::createBackstageView()
{
    if (ribbonBar_ == nullptr || backstageSystemButton_ == nullptr) {
        return;
    }

    backstageView_ = new Qtitan::RibbonBackstageView(ribbonBar_);
    backstageView_->setObjectName(QStringLiteral("mainBackstageView"));

    const auto initializeBackstagePage = [](QWidget* page) {
        if (page == nullptr) {
            return;
        }
        page->setStyleSheet(backstagePageStyleSheet());
    };
    QFont backstageNavigationFont = font();
    if (backstageNavigationFont.pointSizeF() < 11.0) {
        backstageNavigationFont.setPointSizeF(11.0);
    }
    backstageNavigationFont.setWeight(QFont::Medium);
    const auto configureBackstageNavigationAction = [&backstageNavigationFont](QAction* action, const QIcon& icon) {
        if (action == nullptr) {
            return;
        }
        action->setIcon(icon);
        action->setFont(backstageNavigationFont);
    };

    backstageOpenPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageOpenPage_->setObjectName(QStringLiteral("backstageOpenPage"));
    backstageOpenPage_->setWindowTitle(tr("Open"));
    initializeBackstagePage(backstageOpenPage_);
    {
        auto* layout = new QVBoxLayout(backstageOpenPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* openHeaderWidget = new BackstagePageHeaderWidget(backstageOpenPage_);
        openHeaderWidget->setTitleText(tr("Open"));
        openHeaderWidget->setSubtitleText(tr("Open point clouds and projects, or continue from a recent engineering file."));
        backstageOpenTitleLabel_ = openHeaderWidget->titleLabel();
        backstageOpenSubtitleLabel_ = openHeaderWidget->subtitleLabel();
        layout->addWidget(openHeaderWidget);

        auto* actionsCard = createBackstageCard(backstageOpenPage_);
        auto* actionsCardLayout = new QVBoxLayout(actionsCard);
        actionsCardLayout->setContentsMargins(0, 0, 0, 0);

        auto* openActionsWidget = new BackstageOpenActionsWidget(actionsCard);
        if (QVBoxLayout* actionsLayout = openActionsWidget->actionsLayout()) {
            actionsLayout->addWidget(createBackstageActionButton(openAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(addPointCloudAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(openProjectAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(saveProjectAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(saveProjectAsAction_, openActionsWidget));
            actionsLayout->addStretch(1);
        }
        actionsCardLayout->addWidget(openActionsWidget);
        layout->addWidget(actionsCard, 1);
    }
    backstageOpenPageAction_ = backstageView_->addPage(backstageOpenPage_);
    configureBackstageNavigationAction(backstageOpenPageAction_, createRibbonIcon(RibbonGlyph::Open));

    backstageOpenProjectPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageOpenProjectPage_->setObjectName(QStringLiteral("backstageOpenProjectPage"));
    backstageOpenProjectPage_->setWindowTitle(tr("Open Project"));
    initializeBackstagePage(backstageOpenProjectPage_);
    {
        auto* layout = new QVBoxLayout(backstageOpenProjectPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* openProjectHeaderWidget = new BackstagePageHeaderWidget(backstageOpenProjectPage_);
        openProjectHeaderWidget->setTitleText(tr("Open Project"));
        openProjectHeaderWidget->setSubtitleText(tr("Select a recent project or browse to a project file."));
        backstageOpenProjectTitleLabel_ = openProjectHeaderWidget->titleLabel();
        backstageOpenProjectSubtitleLabel_ = openProjectHeaderWidget->subtitleLabel();
        layout->addWidget(openProjectHeaderWidget);

        auto* contentCard = createBackstageCard(backstageOpenProjectPage_);
        auto* contentLayout = new QVBoxLayout(contentCard);
        contentLayout->setContentsMargins(20, 20, 20, 20);
        contentLayout->setSpacing(14);

        backstageOpenProjectWidget_ = new BackstageOpenProjectWidget(contentCard);
        backstageRecentProjectsListWidget_ = backstageOpenProjectWidget_->recentProjectsListWidget();
        backstageProjectPathLineEdit_ = backstageOpenProjectWidget_->projectPathLineEdit();
        backstageProjectBrowseButton_ = backstageOpenProjectWidget_->browseButton();
        backstageProjectOpenButton_ = backstageOpenProjectWidget_->openButton();
        contentLayout->addWidget(backstageOpenProjectWidget_);

        layout->addWidget(contentCard, 1);
    }
    backstageOpenProjectPageAction_ = backstageView_->addPage(backstageOpenProjectPage_);
    configureBackstageNavigationAction(backstageOpenProjectPageAction_, createRibbonIcon(RibbonGlyph::Open));

    backstageSaveAction_ = backstageView_->addAction(createRibbonIcon(RibbonGlyph::Save), saveProjectAction_->text());
    configureBackstageNavigationAction(backstageSaveAction_, createRibbonIcon(RibbonGlyph::Save));
    backstageSaveAsAction_ = backstageView_->addAction(createRibbonIcon(RibbonGlyph::Save), saveProjectAsAction_->text());
    configureBackstageNavigationAction(backstageSaveAsAction_, createRibbonIcon(RibbonGlyph::Save));

    backstageProjectPropertiesPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageProjectPropertiesPage_->setObjectName(QStringLiteral("backstageProjectPropertiesPage"));
    backstageProjectPropertiesPage_->setWindowTitle(tr("Project Management"));
    initializeBackstagePage(backstageProjectPropertiesPage_);
    {
        auto* layout = new QVBoxLayout(backstageProjectPropertiesPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* projectPropertiesHeaderWidget = new BackstagePageHeaderWidget(backstageProjectPropertiesPage_);
        projectPropertiesHeaderWidget->setTitleText(tr("Project Management"));
        projectPropertiesHeaderWidget->setSubtitleText(tr("Review the active project file, datasets, and coordinate system configuration."));
        backstageProjectPropertiesTitleLabel_ = projectPropertiesHeaderWidget->titleLabel();
        backstageProjectPropertiesSubtitleLabel_ = projectPropertiesHeaderWidget->subtitleLabel();
        layout->addWidget(projectPropertiesHeaderWidget);

        auto* summaryCard = createBackstageCard(backstageProjectPropertiesPage_);
        auto* summaryLayout = new QVBoxLayout(summaryCard);
        summaryLayout->setContentsMargins(20, 20, 20, 20);
        summaryLayout->setSpacing(0);

        backstageProjectPropertiesWidget_ = new BackstageProjectPropertiesWidget(summaryCard);
        backstageProjectFileValueLabel_ = backstageProjectPropertiesWidget_->projectFileValueLabel();
        backstageProjectDatasetCountValueLabel_ = backstageProjectPropertiesWidget_->datasetCountValueLabel();
        backstageProjectCoordinateSystemsValueLabel_ = backstageProjectPropertiesWidget_->coordinateSystemsValueLabel();
        backstageEditProjectPropertiesButton_ = backstageProjectPropertiesWidget_->editCoordinateSystemsButton();
        summaryLayout->addWidget(backstageProjectPropertiesWidget_);
        layout->addWidget(summaryCard);
        layout->addStretch(1);
    }
    backstageProjectPropertiesPageAction_ = backstageView_->addPage(backstageProjectPropertiesPage_);
    configureBackstageNavigationAction(backstageProjectPropertiesPageAction_, projectCoordinateSystemsAction_->icon());

    backstageApplicationSettingsPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageApplicationSettingsPage_->setObjectName(QStringLiteral("backstageApplicationSettingsPage"));
    backstageApplicationSettingsPage_->setWindowTitle(tr("Application Settings"));
    initializeBackstagePage(backstageApplicationSettingsPage_);
    {
        auto* layout = new QVBoxLayout(backstageApplicationSettingsPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* applicationSettingsHeaderWidget = new BackstagePageHeaderWidget(backstageApplicationSettingsPage_);
        applicationSettingsHeaderWidget->setTitleText(tr("Application Settings"));
        applicationSettingsHeaderWidget->setSubtitleText(tr("Adjust the office theme, interface language, and workspace panels."));
        backstageApplicationSettingsTitleLabel_ = applicationSettingsHeaderWidget->titleLabel();
        backstageApplicationSettingsSubtitleLabel_ = applicationSettingsHeaderWidget->subtitleLabel();
        layout->addWidget(applicationSettingsHeaderWidget);

        backstageApplicationSettingsWidget_ = new BackstageApplicationSettingsWidget(backstageApplicationSettingsPage_);
        if (QHBoxLayout* themeLayout = backstageApplicationSettingsWidget_->themeButtonLayout()) {
            themeLayout->addWidget(createBackstageActionButton(themeColorfulAction_, backstageApplicationSettingsWidget_));
            themeLayout->addWidget(createBackstageActionButton(themeWhiteAction_, backstageApplicationSettingsWidget_));
            themeLayout->addWidget(createBackstageActionButton(themeDarkGrayAction_, backstageApplicationSettingsWidget_));
            themeLayout->addStretch(1);
        }
        if (QHBoxLayout* languageLayout = backstageApplicationSettingsWidget_->languageButtonLayout()) {
            languageLayout->addWidget(createBackstageActionButton(languageEnglishAction_, backstageApplicationSettingsWidget_));
            languageLayout->addWidget(createBackstageActionButton(languageChineseAction_, backstageApplicationSettingsWidget_));
            languageLayout->addStretch(1);
        }
        backstageShowLogCheckBox_ = backstageApplicationSettingsWidget_->showLogCheckBox();
        backstageCaptureSaveDirectoryLineEdit_ = backstageApplicationSettingsWidget_->captureSaveDirectoryLineEdit();
        backstageCaptureBrowseButton_ = backstageApplicationSettingsWidget_->captureBrowseButton();
        backstageCaptureAutoSaveCheckBox_ = backstageApplicationSettingsWidget_->captureAutoSaveCheckBox();
        backstageCaptureShortcutHintLabel_ = backstageApplicationSettingsWidget_->captureShortcutHintLabel();
        layout->addWidget(backstageApplicationSettingsWidget_);
        layout->addStretch(1);
    }
    backstageApplicationSettingsPageAction_ = backstageView_->addPage(backstageApplicationSettingsPage_);
    configureBackstageNavigationAction(backstageApplicationSettingsPageAction_, createRibbonIcon(RibbonGlyph::Settings));

    backstageAboutPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageAboutPage_->setObjectName(QStringLiteral("backstageAboutPage"));
    backstageAboutPage_->setWindowTitle(tr("About"));
    initializeBackstagePage(backstageAboutPage_);
    {
        auto* layout = new QVBoxLayout(backstageAboutPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* aboutHeaderWidget = new BackstagePageHeaderWidget(backstageAboutPage_);
        aboutHeaderWidget->setTitleText(tr("About"));
        aboutHeaderWidget->setSubtitleText(tr("Build information and the key runtime components used by this application."));
        backstageAboutTitleLabel_ = aboutHeaderWidget->titleLabel();
        backstageAboutSubtitleLabel_ = aboutHeaderWidget->subtitleLabel();
        layout->addWidget(aboutHeaderWidget);

        auto* aboutCard = createBackstageCard(backstageAboutPage_);
        auto* aboutLayout = new QVBoxLayout(aboutCard);
        aboutLayout->setContentsMargins(20, 20, 20, 20);
        backstageAboutWidget_ = new BackstageAboutWidget(aboutCard);
        backstageAboutBodyLabel_ = backstageAboutWidget_->bodyLabel();
        aboutLayout->addWidget(backstageAboutWidget_);
        layout->addWidget(aboutCard);
        layout->addStretch(1);
    }
    backstageAboutPageAction_ = backstageView_->addPage(backstageAboutPage_);
    configureBackstageNavigationAction(backstageAboutPageAction_, createRibbonIcon(RibbonGlyph::About));

    backstageView_->addSeparator();
    backstageExitAction_ = backstageView_->addAction(exitAction_->icon(), exitAction_->text());
    configureBackstageNavigationAction(backstageExitAction_, createRibbonIcon(RibbonGlyph::Exit));

    backstageSystemButton_->setBackstage(backstageView_);

    connect(backstageSaveAction_, &QAction::triggered, this, [this]() { saveProject(); });
    connect(backstageSaveAsAction_, &QAction::triggered, this, [this]() { saveProjectAs(); });
    connect(backstageExitAction_, &QAction::triggered, this, [this]() {
        hideBackstageView();
        close();
    });
    connect(backstageProjectBrowseButton_, &QPushButton::clicked, this, [this]() {
        const QString selectedPath = showStyledOpenFileNameDialog(
            this,
            tr("Open Project"),
            backstageProjectPathLineEdit_ != nullptr ? backstageProjectPathLineEdit_->text().trimmed() : QString(),
            tr("LiDAR Power Projects (*.json *.lpproj);;JSON Files (*.json);;All Files (*.*)"));
        if (!selectedPath.isEmpty() && backstageProjectPathLineEdit_ != nullptr) {
            backstageProjectPathLineEdit_->setText(normalizedProjectFilePath(selectedPath));
        }
    });
    connect(backstageProjectOpenButton_, &QPushButton::clicked, this, [this]() { openProjectFromBackstage(); });
    connect(backstageProjectPathLineEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (backstageProjectOpenButton_ != nullptr) {
            backstageProjectOpenButton_->setEnabled(!normalizedProjectFilePath(text).isEmpty());
        }
    });
    connect(backstageRecentProjectsListWidget_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (current == nullptr || backstageProjectPathLineEdit_ == nullptr) {
            return;
        }
        const QString selectedPath = current->data(Qt::UserRole).toString();
        if (!selectedPath.isEmpty()) {
            backstageProjectPathLineEdit_->setText(selectedPath);
        }
    });
    connect(backstageRecentProjectsListWidget_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item == nullptr || item->data(Qt::UserRole).toString().isEmpty()) {
            return;
        }
        openProjectFromBackstage();
    });
    connect(backstageEditProjectPropertiesButton_, &QPushButton::clicked, this, [this]() { openProjectCoordinateSystems(); });
    connect(backstageShowLogCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != checked) {
            showLogAction_->setChecked(checked);
        }
    });
    connect(backstageCaptureBrowseButton_, &QPushButton::clicked, this, [this]() {
        const QString selectedDirectory = showStyledExistingDirectoryDialog(
            this,
            tr("Select Capture Save Folder"),
            backstageCaptureSaveDirectoryLineEdit_ != nullptr
                ? backstageCaptureSaveDirectoryLineEdit_->text().trimmed()
                : captureSaveDirectory_);
        if (selectedDirectory.isEmpty()) {
            return;
        }

        captureSaveDirectory_ = QDir::toNativeSeparators(QDir::cleanPath(selectedDirectory));
        if (backstageCaptureSaveDirectoryLineEdit_ != nullptr) {
            const QSignalBlocker blocker(backstageCaptureSaveDirectoryLineEdit_);
            backstageCaptureSaveDirectoryLineEdit_->setText(captureSaveDirectory_);
        }
        persistWindowSettings();
    });
    connect(backstageCaptureSaveDirectoryLineEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (backstageCaptureSaveDirectoryLineEdit_ == nullptr) {
            return;
        }

        const QString trimmedPath = backstageCaptureSaveDirectoryLineEdit_->text().trimmed();
        captureSaveDirectory_ = trimmedPath.isEmpty()
            ? defaultCaptureSaveDirectory()
            : QDir::toNativeSeparators(QDir::cleanPath(trimmedPath));
        const QSignalBlocker blocker(backstageCaptureSaveDirectoryLineEdit_);
        backstageCaptureSaveDirectoryLineEdit_->setText(captureSaveDirectory_);
        persistWindowSettings();
    });
    connect(backstageCaptureAutoSaveCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        captureSkipSaveDialog_ = checked;
        persistWindowSettings();
    });
    connect(showLogAction_, &QAction::toggled, this, [this](bool checked) {
        if (backstageShowLogCheckBox_ != nullptr && backstageShowLogCheckBox_->isChecked() != checked) {
            const QSignalBlocker blocker(backstageShowLogCheckBox_);
            backstageShowLogCheckBox_->setChecked(checked);
        }
    });
    connect(backstageView_, &Qtitan::RibbonBackstageView::aboutToShow, this, [this]() {
        normalizeBackstageNavigationButton(backstageView_, backstageSaveAction_);
        normalizeBackstageNavigationButton(backstageView_, backstageSaveAsAction_);
        normalizeBackstageNavigationButton(backstageView_, backstageExitAction_);
        refreshBackstageRecentProjects();
        refreshBackstageProjectPropertiesPage();
        refreshBackstageApplicationSettingsPage();
        refreshBackstageAboutPage();
    });
}

void MainWindow::openBackstagePage(QWidget* page)
{
    if (backstageView_ == nullptr || page == nullptr) {
        return;
    }

    if (page == backstageOpenProjectPage_) {
        refreshBackstageRecentProjects();
    } else if (page == backstageProjectPropertiesPage_) {
        refreshBackstageProjectPropertiesPage();
    } else if (page == backstageApplicationSettingsPage_) {
        refreshBackstageApplicationSettingsPage();
    } else if (page == backstageAboutPage_) {
        refreshBackstageAboutPage();
    }

    backstageView_->setActivePage(page);
    backstageView_->open();
}

void MainWindow::hideBackstageView()
{
    if (backstageView_ != nullptr && backstageView_->isVisible()) {
        backstageView_->hide();
    }
}

void MainWindow::refreshBackstageRecentProjects()
{
    if (backstageRecentProjectsListWidget_ == nullptr || backstageProjectPathLineEdit_ == nullptr) {
        return;
    }

    QSettings settings;
    const QString lastOpenedProject = normalizedProjectFilePath(
        settings.value(settingskeys::kProjectLastOpenedProject).toString());
    const QStringList recentProjects = normalizedRecentProjectFiles(
        settings.value(settingskeys::kProjectRecentProjects).toStringList(),
        lastOpenedProject);
    settings.setValue(settingskeys::kProjectRecentProjects, recentProjects);

    const QSignalBlocker listBlocker(backstageRecentProjectsListWidget_);
    backstageRecentProjectsListWidget_->clear();

    for (const QString& recentProject : recentProjects) {
        const QString displayName = QFileInfo(recentProject).fileName();
        auto* item = new QListWidgetItem(
            displayName.isEmpty() ? QDir::toNativeSeparators(recentProject) : displayName,
            backstageRecentProjectsListWidget_);
        item->setData(Qt::UserRole, recentProject);
        item->setToolTip(QDir::toNativeSeparators(recentProject));
    }

    if (backstageRecentProjectsListWidget_->count() == 0) {
        auto* item = new QListWidgetItem(tr("No recent projects"), backstageRecentProjectsListWidget_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
    }

    if (!recentProjects.isEmpty()) {
        const int defaultRecentIndex = lastOpenedProject.isEmpty()
            ? 0
            : indexOfProjectFilePath(recentProjects, lastOpenedProject);
        backstageRecentProjectsListWidget_->setCurrentRow(defaultRecentIndex >= 0 ? defaultRecentIndex : 0);
        backstageProjectPathLineEdit_->setText(
            defaultRecentIndex >= 0 ? recentProjects.at(defaultRecentIndex) : recentProjects.constFirst());
    } else if (!currentProjectFilePath_.isEmpty()) {
        backstageProjectPathLineEdit_->setText(currentProjectFilePath_);
    } else {
        backstageProjectPathLineEdit_->clear();
    }

    if (backstageProjectOpenButton_ != nullptr) {
        backstageProjectOpenButton_->setEnabled(!normalizedProjectFilePath(backstageProjectPathLineEdit_->text()).isEmpty());
    }
}

void MainWindow::refreshBackstageProjectPropertiesPage()
{
    if (backstageProjectFileValueLabel_ == nullptr
        || backstageProjectDatasetCountValueLabel_ == nullptr
        || backstageProjectCoordinateSystemsValueLabel_ == nullptr) {
        return;
    }

    const QString projectPath = currentProjectFilePath_.trimmed().isEmpty()
        ? tr("Unsaved project")
        : QDir::toNativeSeparators(currentProjectFilePath_);

    backstageProjectFileValueLabel_->setText(projectPath);
    backstageProjectDatasetCountValueLabel_->setText(
        viewer_ != nullptr
            ? QLocale().toString(viewer_->currentFilePaths().size())
            : QStringLiteral("0"));
    backstageProjectCoordinateSystemsValueLabel_->setText(
        formatProjectCoordinateSystemsSummary(projectCoordinateSystems_));
}

void MainWindow::refreshBackstageApplicationSettingsPage()
{
    if (backstageShowLogCheckBox_ != nullptr && showLogAction_ != nullptr) {
        const QSignalBlocker blocker(backstageShowLogCheckBox_);
        backstageShowLogCheckBox_->setChecked(showLogAction_->isChecked());
    }
    if (backstageCaptureSaveDirectoryLineEdit_ != nullptr) {
        const QSignalBlocker blocker(backstageCaptureSaveDirectoryLineEdit_);
        backstageCaptureSaveDirectoryLineEdit_->setText(
            captureSaveDirectory_.trimmed().isEmpty()
                ? defaultCaptureSaveDirectory()
                : captureSaveDirectory_);
    }
    if (backstageCaptureAutoSaveCheckBox_ != nullptr) {
        const QSignalBlocker blocker(backstageCaptureAutoSaveCheckBox_);
        backstageCaptureAutoSaveCheckBox_->setChecked(captureSkipSaveDialog_);
    }
    if (backstageCaptureShortcutHintLabel_ != nullptr) {
        const QString screenshotShortcut = captureScreenshotAction_ != nullptr
            ? captureScreenshotAction_->shortcut().toString(QKeySequence::NativeText)
            : QString();
        const QString recordingShortcut = toggleScreenRecordingAction_ != nullptr
            ? toggleScreenRecordingAction_->shortcut().toString(QKeySequence::NativeText)
            : QString();
        backstageCaptureShortcutHintLabel_->setText(
            tr("Screenshot: %1 | Recording: %2")
                .arg(screenshotShortcut.isEmpty() ? tr("None") : screenshotShortcut)
                .arg(recordingShortcut.isEmpty() ? tr("None") : recordingShortcut));
    }
}

void MainWindow::refreshBackstageAboutPage()
{
    if (backstageAboutBodyLabel_ == nullptr) {
        return;
    }

    backstageAboutBodyLabel_->setText(tr(
        "Version: %1\n"
        "Frameworks: Qt %2, OpenSceneGraph, Qtitan Ribbon\n"
        "Point cloud stack: LASlib / LASzip, optional PROJ / GDAL support\n"
        "Repository: LAS Point Cloud Viewer for transmission line inspection workflows.")
            .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
}

void MainWindow::openProjectFromBackstage()
{
    if (backstageProjectPathLineEdit_ == nullptr) {
        return;
    }

    const QString projectPath = normalizedProjectFilePath(backstageProjectPathLineEdit_->text());
    if (projectPath.isEmpty()) {
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Open Project"),
            tr("Select an existing project file."),
            QMessageBox::Ok);
        return;
    }
    if (!QFileInfo::exists(projectPath)) {
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Open Project"),
            tr("Project file does not exist."),
            QMessageBox::Ok);
        return;
    }
    const QString suffix = QFileInfo(projectPath).suffix().toLower();
    if (suffix != QStringLiteral("json") && suffix != QStringLiteral("lpproj")) {
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Open Project"),
            tr("Choose a .json or .lpproj project file."),
            QMessageBox::Ok);
        return;
    }

    hideBackstageView();
    loadProjectFile(projectPath);
}
