#pragma once

#include <QString>

#include "capture/ScreenRecordingTypes.h"

namespace capture
{

class ScreenRecorder
{
public:
    virtual ~ScreenRecorder() = default;

    virtual bool isAvailable() const = 0;
    virtual QString unavailableReason() const = 0;
    virtual bool isRecording() const = 0;

    virtual ScreenRecordingResult startRecording(const ScreenRecordingStartOptions& options) = 0;
    virtual ScreenRecordingResult stopRecording() = 0;
};

} // namespace capture
