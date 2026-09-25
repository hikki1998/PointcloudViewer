#include "gui/MainWindow.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QPixmap>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

#include "capture/ScreenRecorderFactory.h"
#include "gui/support/UiHelpers.h"

namespace
{

QString makeUniqueOutputPath(const QString& candidatePath)
{
    if (!QFileInfo::exists(candidatePath)) {
        return candidatePath;
    }

    const QFileInfo candidateInfo(candidatePath);
    const QString extension = candidateInfo.suffix();
    const QString extensionPart = extension.isEmpty() ? QString() : QStringLiteral(".") + extension;
    const QString baseName = candidateInfo.completeBaseName().isEmpty()
        ? QStringLiteral("recording")
        : candidateInfo.completeBaseName();
    const QDir parentDir = candidateInfo.absoluteDir();

    int suffixIndex = 1;
    QString uniquePath;
    do {
        uniquePath = parentDir.filePath(
            QStringLiteral("%1_%2%3")
                .arg(baseName)
                .arg(suffixIndex)
                .arg(extensionPart));
        ++suffixIndex;
    } while (QFileInfo::exists(uniquePath));

    return uniquePath;
}

}

QString MainWindow::defaultCaptureSaveDirectory() const
{
    QString baseDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (baseDirectory.isEmpty()) {
        baseDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    if (baseDirectory.isEmpty()) {
        baseDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (baseDirectory.isEmpty()) {
        baseDirectory = QDir::homePath();
    }

    return QDir::toNativeSeparators(QDir(baseDirectory).filePath(QStringLiteral("LASViewerCaptures")));
}

QString MainWindow::createTemporaryRecordingOutputPath(const QString& preferredFileName) const
{
    QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.trimmed().isEmpty()) {
        tempRoot = QDir::tempPath();
    }

    QDir tempDir(QDir::fromNativeSeparators(tempRoot));
    if (!tempDir.mkpath(QStringLiteral("."))) {
        return QString();
    }

    if (!tempDir.cd(QStringLiteral("LASViewerRecordingTemp"))) {
        if (!tempDir.mkdir(QStringLiteral("LASViewerRecordingTemp"))) {
            return QString();
        }
        if (!tempDir.cd(QStringLiteral("LASViewerRecordingTemp"))) {
            return QString();
        }
    }

    QString baseName = QFileInfo(preferredFileName).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("recording");
    }

    const QString tempFileName = QStringLiteral("%1_%2.mp4")
        .arg(baseName)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    return tempDir.filePath(tempFileName);
}

capture::ScreenRecordingResult MainWindow::finalizeRecordingOutputFile(const QString& temporaryFilePath, bool interactiveStop)
{
    const QFileInfo temporaryInfo(temporaryFilePath);
    if (temporaryFilePath.trimmed().isEmpty() || !temporaryInfo.exists()) {
        return capture::ScreenRecordingResult::fail(
            tr("No recording file was produced."));
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultFileName = temporaryInfo.fileName().trimmed().isEmpty()
        ? QStringLiteral("recording_%1.mp4").arg(timestamp)
        : temporaryInfo.fileName();

    QString finalOutputPath;
    if (interactiveStop && !captureSkipSaveDialog_) {
        finalOutputPath = resolveCaptureOutputPath(
            tr("Save Recording"),
            defaultFileName,
            tr("MP4 Video (*.mp4)"),
            QStringLiteral("mp4"));
        if (finalOutputPath.isEmpty()) {
            QFile::remove(temporaryFilePath);
            return capture::ScreenRecordingResult::fail(QString());
        }
    } else {
        QString outputDirectoryPath = captureSaveDirectory_.trimmed();
        if (outputDirectoryPath.isEmpty()) {
            outputDirectoryPath = defaultCaptureSaveDirectory();
        }

        QDir outputDirectory(QDir::fromNativeSeparators(outputDirectoryPath));
        if (!outputDirectory.mkpath(QStringLiteral("."))) {
            return capture::ScreenRecordingResult::fail(
                tr("Unable to create the recording output folder."));
        }

        finalOutputPath = makeUniqueOutputPath(outputDirectory.filePath(defaultFileName));
    }

    finalOutputPath = QDir::toNativeSeparators(finalOutputPath);
    const QFileInfo finalInfo(finalOutputPath);
    if (!QDir(finalInfo.absolutePath()).mkpath(QStringLiteral("."))) {
        return capture::ScreenRecordingResult::fail(
            tr("Unable to create the recording output folder."));
    }

    bool moved = false;
    if (QDir::toNativeSeparators(temporaryFilePath) == finalOutputPath) {
        moved = true;
    } else {
        if (QFileInfo::exists(finalOutputPath)) {
            QFile::remove(finalOutputPath);
        }
        moved = QFile::rename(temporaryFilePath, finalOutputPath);
        if (!moved) {
            moved = QFile::copy(temporaryFilePath, finalOutputPath);
            if (moved) {
                QFile::remove(temporaryFilePath);
            }
        }
    }

    if (!moved) {
        return capture::ScreenRecordingResult::fail(
            tr("Failed to save recording: %1").arg(finalOutputPath));
    }

    captureSaveDirectory_ = QDir::toNativeSeparators(finalInfo.absolutePath());
    persistWindowSettings();
    recordingOutputFilePath_ = finalOutputPath;
    return capture::ScreenRecordingResult::ok(finalOutputPath);
}

QString MainWindow::resolveCaptureOutputPath(
    const QString& dialogTitle,
    const QString& defaultFileName,
    const QString& filter,
    const QString& requiredSuffix)
{
    QString saveDirectory = captureSaveDirectory_.trimmed();
    if (saveDirectory.isEmpty()) {
        saveDirectory = defaultCaptureSaveDirectory();
    }
    saveDirectory = QDir::toNativeSeparators(QDir::cleanPath(saveDirectory));

    if (captureSkipSaveDialog_) {
        QDir outputDirectory(QDir::fromNativeSeparators(saveDirectory));
        if (!outputDirectory.mkpath(QStringLiteral("."))) {
            showUserMessage(LogLevel::Error, tr("Unable to create the capture folder."), 4500);
            return QString();
        }
        return outputDirectory.filePath(defaultFileName);
    }

    const QString initialPath = QDir(QDir::fromNativeSeparators(saveDirectory)).filePath(defaultFileName);
    QString selectedPath = lasviewer::gui::showStyledSaveFileNameDialog(
        this,
        dialogTitle,
        QDir::toNativeSeparators(initialPath),
        filter);
    if (selectedPath.isEmpty()) {
        return QString();
    }

    if (!requiredSuffix.trimmed().isEmpty() && QFileInfo(selectedPath).suffix().trimmed().isEmpty()) {
        selectedPath += QStringLiteral(".") + requiredSuffix.trimmed();
    }

    captureSaveDirectory_ = QDir::toNativeSeparators(QFileInfo(selectedPath).absolutePath());
    persistWindowSettings();
    return selectedPath;
}

void MainWindow::captureMainWindowScreenshot()
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultFileName = QStringLiteral("screenshot_%1.png").arg(timestamp);
    const QString outputPath = resolveCaptureOutputPath(
        tr("Save Screenshot"),
        defaultFileName,
        tr("PNG Images (*.png)"),
        QStringLiteral("png"));
    if (outputPath.isEmpty()) {
        return;
    }

    const QFileInfo outputInfo(outputPath);
    if (!QDir(outputInfo.absolutePath()).mkpath(QStringLiteral("."))) {
        showUserMessage(LogLevel::Error, tr("Unable to create the screenshot output folder."), 4500);
        return;
    }

    const QPixmap screenshot = grab();
    if (screenshot.isNull()) {
        showUserMessage(LogLevel::Error, tr("Screenshot failed. The window image is empty."), 4500);
        return;
    }
    if (!screenshot.save(outputPath, "PNG")) {
        showUserMessage(LogLevel::Error, tr("Failed to save screenshot: %1").arg(QDir::toNativeSeparators(outputPath)), 5000);
        return;
    }

    showUserMessage(LogLevel::Info, tr("Screenshot saved: %1").arg(outputInfo.fileName()), 3200);
}

void MainWindow::toggleScreenRecording()
{
    const bool processRecordingActive =
        recordingProcess_ != nullptr && recordingProcess_->state() != QProcess::NotRunning;
    const bool embeddedRecordingActive =
        screenRecorder_ != nullptr && screenRecorder_->isRecording();
    if (processRecordingActive || embeddedRecordingActive) {
        stopScreenRecording(true);
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultFileName = QStringLiteral("recording_%1.mp4").arg(timestamp);
    const QString temporaryOutputPath = createTemporaryRecordingOutputPath(defaultFileName);
    if (temporaryOutputPath.isEmpty()) {
        showUserMessage(LogLevel::Error, tr("Unable to create temporary recording file."), 4500);
        return;
    }

    if (screenRecorder_ != nullptr && screenRecorder_->isAvailable()) {
        capture::ScreenRecordingStartOptions options;
        options.outputFilePath = temporaryOutputPath;
        options.frameRate = 30;
        options.nativeWindowHandle = static_cast<quintptr>(winId());

        const capture::ScreenRecordingResult result = screenRecorder_->startRecording(options);
        if (!result.success) {
            const QString diagnostic = result.message.trimmed().isEmpty()
                ? tr("No ffmpeg diagnostic output was captured.")
                : result.message;
            showUserMessage(
                LogLevel::Error,
                tr("Recording failed. %1").arg(diagnostic),
                6500);
            return;
        }

        recordingOutputFilePath_ = temporaryOutputPath;
        showUserMessage(
            LogLevel::Info,
            tr("Recording started. Use %1 or the ribbon button to stop.")
                .arg(toggleScreenRecordingAction_ != nullptr
                    ? toggleScreenRecordingAction_->shortcut().toString(QKeySequence::NativeText)
                    : tr("Stop Recording")),
            4200);
        updateActionState();
        return;
    }

    const QString recorderUnavailableReason =
        screenRecorder_ != nullptr ? screenRecorder_->unavailableReason().trimmed() : QString();
    const QString ffmpegExecutable = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpegExecutable.isEmpty()) {
        const QString message = recorderUnavailableReason.isEmpty()
            ? tr("Recording requires ffmpeg. Add ffmpeg to PATH or place ffmpeg.exe beside the application.")
            : tr("Embedded recording is unavailable: %1. Recording requires ffmpeg. Add ffmpeg to PATH, or enable LAS_VIEWER_ENABLE_WINDOWS_CAPTURE in your build.")
                .arg(recorderUnavailableReason);
        showUserMessage(
            LogLevel::Error,
            message,
            5500);
        return;
    }

    recordingStopRequested_ = false;
    suppressRecordingStopMessage_ = false;
    recordingOutputFilePath_ = temporaryOutputPath;
    recordingProcess_ = new QProcess(this);
    recordingProcess_->setProgram(ffmpegExecutable);
    recordingProcess_->setProcessChannelMode(QProcess::MergedChannels);
    recordingProcess_->setArguments({
        QStringLiteral("-y"),
        QStringLiteral("-f"),
        QStringLiteral("gdigrab"),
        QStringLiteral("-framerate"),
        QStringLiteral("30"),
        QStringLiteral("-i"),
        QStringLiteral("title=%1").arg(windowTitle()),
        QStringLiteral("-vcodec"),
        QStringLiteral("libx264"),
        QStringLiteral("-preset"),
        QStringLiteral("veryfast"),
        QStringLiteral("-pix_fmt"),
        QStringLiteral("yuv420p"),
        temporaryOutputPath
    });

    connect(
        recordingProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            QProcess* finishedProcess = recordingProcess_;
            QString processOutput;
            if (finishedProcess != nullptr) {
                processOutput = QString::fromLocal8Bit(finishedProcess->readAllStandardOutput()).trimmed();
                finishedProcess->deleteLater();
            }

            recordingProcess_ = nullptr;
            const bool stoppedByUser = recordingStopRequested_;
            recordingStopRequested_ = false;
            const bool interactiveStop = !suppressRecordingStopMessage_;

            const bool success = exitStatus == QProcess::NormalExit
                && (exitCode == 0 || stoppedByUser)
                && QFileInfo::exists(recordingOutputFilePath_);
            if (success) {
                const capture::ScreenRecordingResult saveResult =
                    finalizeRecordingOutputFile(recordingOutputFilePath_, interactiveStop);
                if (interactiveStop) {
                    if (saveResult.success) {
                        showUserMessage(
                            LogLevel::Info,
                            tr("Recording saved: %1").arg(QFileInfo(recordingOutputFilePath_).fileName()),
                            3800);
                    } else if (!saveResult.message.trimmed().isEmpty()) {
                        showUserMessage(
                            LogLevel::Error,
                            tr("Recording failed. %1").arg(saveResult.message),
                            6500);
                    }
                }
            } else if (interactiveStop) {
                const QString diagnostic = processOutput.isEmpty()
                    ? tr("No ffmpeg diagnostic output was captured.")
                    : processOutput;
                showUserMessage(
                    LogLevel::Error,
                    tr("Recording failed. %1").arg(diagnostic),
                    6500);
            }

            suppressRecordingStopMessage_ = false;
            updateActionState();
        });

    recordingProcess_->start();
    if (!recordingProcess_->waitForStarted(3000)) {
        const QString diagnostic = QString::fromLocal8Bit(recordingProcess_->readAllStandardOutput()).trimmed();
        recordingProcess_->deleteLater();
        recordingProcess_ = nullptr;
        QFile::remove(temporaryOutputPath);
        showUserMessage(
            LogLevel::Error,
            diagnostic.isEmpty()
                ? tr("Failed to start recording process.")
                : tr("Failed to start recording process. %1").arg(diagnostic),
            6000);
        return;
    }

    showUserMessage(
        LogLevel::Info,
        tr("Recording started. Use %1 or the ribbon button to stop.")
            .arg(toggleScreenRecordingAction_ != nullptr
                ? toggleScreenRecordingAction_->shortcut().toString(QKeySequence::NativeText)
                : tr("Stop Recording")),
        4200);
    updateActionState();
}

void MainWindow::stopScreenRecording(bool notifyUser)
{
    if (screenRecorder_ != nullptr && screenRecorder_->isRecording()) {
        const capture::ScreenRecordingResult result = screenRecorder_->stopRecording();
        if (result.success) {
            const capture::ScreenRecordingResult saveResult =
                finalizeRecordingOutputFile(recordingOutputFilePath_, notifyUser);
            if (notifyUser) {
                if (saveResult.success) {
                    showUserMessage(
                        LogLevel::Info,
                        tr("Recording saved: %1").arg(QFileInfo(recordingOutputFilePath_).fileName()),
                        3800);
                } else if (!saveResult.message.trimmed().isEmpty()) {
                    showUserMessage(
                        LogLevel::Error,
                        tr("Recording failed. %1").arg(saveResult.message),
                        6500);
                }
            }
        } else if (notifyUser) {
            const QString diagnostic = result.message.trimmed().isEmpty()
                ? tr("No ffmpeg diagnostic output was captured.")
                : result.message;
                showUserMessage(
                    LogLevel::Error,
                    tr("Recording failed. %1").arg(diagnostic),
                    6500);
        }
        updateActionState();
        return;
    }

    if (recordingProcess_ == nullptr || recordingProcess_->state() == QProcess::NotRunning) {
        return;
    }

    recordingStopRequested_ = true;
    suppressRecordingStopMessage_ = !notifyUser;
    recordingProcess_->write("q\n");
    recordingProcess_->waitForBytesWritten(300);
    if (!recordingProcess_->waitForFinished(2800)) {
        recordingProcess_->kill();
    }
}
