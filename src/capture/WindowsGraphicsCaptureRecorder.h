#pragma once

#include <atomic>
#include <memory>

#include <QString>

#include "capture/ScreenRecorder.h"

namespace capture
{

class WindowsGraphicsCaptureRecorder final : public ScreenRecorder
{
public:
    WindowsGraphicsCaptureRecorder();
    ~WindowsGraphicsCaptureRecorder() override;

    bool isAvailable() const override;
    QString unavailableReason() const override;
    bool isRecording() const override;

    ScreenRecordingResult startRecording(const ScreenRecordingStartOptions& options) override;
    ScreenRecordingResult stopRecording() override;

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
    std::atomic_bool recording_ { false };
};

} // namespace capture
