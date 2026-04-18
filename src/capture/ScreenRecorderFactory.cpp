#include "capture/ScreenRecorderFactory.h"

#include <memory>
#include <utility>

#include <QtGlobal>

#include "capture/ScreenRecorder.h"
#include "capture/WindowsGraphicsCaptureRecorder.h"

namespace capture
{

namespace
{

class UnsupportedScreenRecorder final : public ScreenRecorder
{
public:
    explicit UnsupportedScreenRecorder(QString reason)
        : reason_(std::move(reason))
    {
    }

    bool isAvailable() const override
    {
        return false;
    }

    QString unavailableReason() const override
    {
        return reason_;
    }

    bool isRecording() const override
    {
        return false;
    }

    ScreenRecordingResult startRecording(const ScreenRecordingStartOptions&) override
    {
        return ScreenRecordingResult::fail(reason_);
    }

    ScreenRecordingResult stopRecording() override
    {
        return ScreenRecordingResult::fail(reason_);
    }

private:
    QString reason_;
};

} // namespace

std::unique_ptr<ScreenRecorder> createScreenRecorder()
{
#if defined(Q_OS_WIN) && defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
    return std::make_unique<WindowsGraphicsCaptureRecorder>();
#else
    return std::make_unique<UnsupportedScreenRecorder>(
        QStringLiteral("Built without Windows capture backend. Enable LAS_VIEWER_ENABLE_WINDOWS_CAPTURE to use embedded recording."));
#endif
}

} // namespace capture
