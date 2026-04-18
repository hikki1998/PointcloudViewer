#pragma once

#include <QString>
#include <QtGlobal>

namespace capture
{

struct ScreenRecordingStartOptions
{
    QString outputFilePath;
    int frameRate = 30;
    quintptr nativeWindowHandle = 0;
};

struct ScreenRecordingResult
{
    bool success = false;
    QString message;

    static ScreenRecordingResult ok(const QString& msg = QString())
    {
        return ScreenRecordingResult{true, msg};
    }

    static ScreenRecordingResult fail(const QString& msg)
    {
        return ScreenRecordingResult{false, msg};
    }
};

} // namespace capture
