#include "capture/WindowsGraphicsCaptureRecorder.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <QFileInfo>
#include <QString>

#if defined(Q_OS_WIN) && defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
#define NOMINMAX
#include <Windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>

#include <wrl/client.h>
#endif

namespace capture
{

#if defined(Q_OS_WIN) && defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)

namespace
{

using Microsoft::WRL::ComPtr;

QString formatHrMessage(const QString& stage, HRESULT hr)
{
    return QStringLiteral("%1 (hr=0x%2)")
        .arg(stage)
        .arg(static_cast<unsigned long>(hr), 8, 16, QLatin1Char('0'));
}

class ScopedCom
{
public:
    ScopedCom()
    {
        hr_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }

    ~ScopedCom()
    {
        if (SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE) {
            CoUninitialize();
        }
    }

    bool ok() const
    {
        return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE;
    }

    HRESULT hr() const
    {
        return hr_;
    }

private:
    HRESULT hr_ = E_FAIL;
};

class ScopedMf
{
public:
    ScopedMf()
    {
        hr_ = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    }

    ~ScopedMf()
    {
        if (SUCCEEDED(hr_)) {
            MFShutdown();
        }
    }

    bool ok() const
    {
        return SUCCEEDED(hr_);
    }

    HRESULT hr() const
    {
        return hr_;
    }

private:
    HRESULT hr_ = E_FAIL;
};

struct GdiCaptureContext
{
    HWND hwnd = nullptr;
    HDC screenDc = nullptr;
    HDC memDc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldObject = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    BYTE* pixels = nullptr;
    int sourceX = 0;
    int sourceY = 0;

    ~GdiCaptureContext()
    {
        if (memDc != nullptr && oldObject != nullptr) {
            SelectObject(memDc, oldObject);
        }
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (memDc != nullptr) {
            DeleteDC(memDc);
        }
        if (screenDc != nullptr) {
            ReleaseDC(nullptr, screenDc);
        }
    }
};

bool initializeCaptureContext(HWND hwnd, GdiCaptureContext& ctx, QString& error)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        error = QStringLiteral("Native window handle is invalid.");
        return false;
    }

    RECT clientRect {};
    if (!GetClientRect(hwnd, &clientRect)) {
        error = QStringLiteral("Failed to query client rect.");
        return false;
    }

    POINT topLeft { clientRect.left, clientRect.top };
    if (!ClientToScreen(hwnd, &topLeft)) {
        error = QStringLiteral("Failed to convert client coordinates.");
        return false;
    }

    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    if (width <= 1 || height <= 1) {
        error = QStringLiteral("Window size is too small for recording.");
        return false;
    }

    if ((width % 2) != 0) {
        --width;
    }
    if ((height % 2) != 0) {
        --height;
    }
    if (width <= 1 || height <= 1) {
        error = QStringLiteral("Window size cannot satisfy encoder alignment.");
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        error = QStringLiteral("Failed to acquire screen DC.");
        return false;
    }

    HDC memDc = CreateCompatibleDC(screenDc);
    if (memDc == nullptr) {
        ReleaseDC(nullptr, screenDc);
        error = QStringLiteral("Failed to create memory DC.");
        return false;
    }

    BITMAPINFO bmi {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr || bits == nullptr) {
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        error = QStringLiteral("Failed to create DIB section for capture.");
        return false;
    }

    HGDIOBJ oldObject = SelectObject(memDc, bitmap);
    if (oldObject == nullptr || oldObject == HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        error = QStringLiteral("Failed to select capture bitmap into memory DC.");
        return false;
    }

    ctx.hwnd = hwnd;
    ctx.screenDc = screenDc;
    ctx.memDc = memDc;
    ctx.bitmap = bitmap;
    ctx.oldObject = oldObject;
    ctx.width = width;
    ctx.height = height;
    ctx.stride = width * 4;
    ctx.pixels = static_cast<BYTE*>(bits);
    ctx.sourceX = topLeft.x;
    ctx.sourceY = topLeft.y;
    return true;
}

bool captureFrame(const GdiCaptureContext& ctx, QString& error)
{
    if (!BitBlt(
            ctx.memDc,
            0,
            0,
            ctx.width,
            ctx.height,
            ctx.screenDc,
            ctx.sourceX,
            ctx.sourceY,
            SRCCOPY | CAPTUREBLT)) {
        error = QStringLiteral("Failed to copy window frame from screen.");
        return false;
    }
    return true;
}

bool configureSinkWriter(
    const QString& outputFilePath,
    int width,
    int height,
    int frameRate,
    ComPtr<IMFSinkWriter>& writer,
    DWORD& streamIndex,
    QString& error)
{
    const std::wstring outputPath = outputFilePath.toStdWString();
    HRESULT hr = MFCreateSinkWriterFromURL(outputPath.c_str(), nullptr, nullptr, writer.GetAddressOf());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("MFCreateSinkWriterFromURL failed"), hr);
        return false;
    }

    ComPtr<IMFMediaType> outputType;
    hr = MFCreateMediaType(outputType.GetAddressOf());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("MFCreateMediaType(output) failed"), hr);
        return false;
    }

    hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetGUID output major type failed"), hr);
        return false;
    }

    hr = outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetGUID output subtype failed"), hr);
        return false;
    }

    hr = outputType->SetUINT32(MF_MT_AVG_BITRATE, 8 * 1000 * 1000);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetUINT32 output bitrate failed"), hr);
        return false;
    }

    hr = outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetUINT32 output interlace failed"), hr);
        return false;
    }

    hr = MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("Set output frame size failed"), hr);
        return false;
    }

    hr = MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, static_cast<UINT32>(frameRate), 1);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("Set output frame rate failed"), hr);
        return false;
    }

    hr = MFSetAttributeRatio(outputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("Set output pixel aspect ratio failed"), hr);
        return false;
    }

    hr = writer->AddStream(outputType.Get(), &streamIndex);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("AddStream failed"), hr);
        return false;
    }

    ComPtr<IMFMediaType> inputType;
    hr = MFCreateMediaType(inputType.GetAddressOf());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("MFCreateMediaType(input) failed"), hr);
        return false;
    }

    hr = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetGUID input major type failed"), hr);
        return false;
    }

    hr = inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetGUID input subtype failed"), hr);
        return false;
    }

    hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetUINT32 input interlace failed"), hr);
        return false;
    }

    hr = MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("Set input frame size failed"), hr);
        return false;
    }

    hr = MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, static_cast<UINT32>(frameRate), 1);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("Set input frame rate failed"), hr);
        return false;
    }

    hr = MFSetAttributeRatio(inputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("Set input pixel aspect ratio failed"), hr);
        return false;
    }

    hr = writer->SetInputMediaType(streamIndex, inputType.Get(), nullptr);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetInputMediaType failed"), hr);
        return false;
    }

    hr = writer->BeginWriting();
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("BeginWriting failed"), hr);
        return false;
    }

    return true;
}

bool writeCapturedFrame(
    IMFSinkWriter* writer,
    DWORD streamIndex,
    const GdiCaptureContext& ctx,
    LONGLONG sampleTime,
    LONGLONG sampleDuration,
    QString& error)
{
    if (writer == nullptr) {
        error = QStringLiteral("Sink writer is null.");
        return false;
    }

    const DWORD frameBytes = static_cast<DWORD>(ctx.stride * ctx.height);

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(frameBytes, buffer.GetAddressOf());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("MFCreateMemoryBuffer failed"), hr);
        return false;
    }

    BYTE* destination = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;
    hr = buffer->Lock(&destination, &maxLength, &currentLength);
    if (FAILED(hr) || destination == nullptr) {
        error = formatHrMessage(QStringLiteral("IMFMediaBuffer::Lock failed"), hr);
        return false;
    }

    for (int row = 0; row < ctx.height; ++row) {
        const BYTE* sourceRow = ctx.pixels + static_cast<size_t>(ctx.height - 1 - row) * static_cast<size_t>(ctx.stride);
        BYTE* destinationRow = destination + static_cast<size_t>(row) * static_cast<size_t>(ctx.stride);
        memcpy(destinationRow, sourceRow, static_cast<size_t>(ctx.stride));
    }
    buffer->Unlock();
    hr = buffer->SetCurrentLength(frameBytes);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetCurrentLength failed"), hr);
        return false;
    }

    ComPtr<IMFSample> sample;
    hr = MFCreateSample(sample.GetAddressOf());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("MFCreateSample failed"), hr);
        return false;
    }

    hr = sample->AddBuffer(buffer.Get());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("IMFSample::AddBuffer failed"), hr);
        return false;
    }

    hr = sample->SetSampleTime(sampleTime);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetSampleTime failed"), hr);
        return false;
    }

    hr = sample->SetSampleDuration(sampleDuration);
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("SetSampleDuration failed"), hr);
        return false;
    }

    hr = writer->WriteSample(streamIndex, sample.Get());
    if (FAILED(hr)) {
        error = formatHrMessage(QStringLiteral("WriteSample failed"), hr);
        return false;
    }

    return true;
}

} // namespace

#endif

struct WindowsGraphicsCaptureRecorder::Impl
{
    std::thread worker;
    std::atomic_bool stopRequested { false };
    std::atomic_bool running { false };
    QString lastMessage;
};

WindowsGraphicsCaptureRecorder::WindowsGraphicsCaptureRecorder()
    : impl_(std::make_unique<Impl>())
{
}

WindowsGraphicsCaptureRecorder::~WindowsGraphicsCaptureRecorder()
{
    if (isRecording()) {
        stopRecording();
    }
}

bool WindowsGraphicsCaptureRecorder::isAvailable() const
{
#if defined(Q_OS_WIN) && defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
    return true;
#else
    return false;
#endif
}

QString WindowsGraphicsCaptureRecorder::unavailableReason() const
{
#if defined(Q_OS_WIN) && defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
    return QString();
#else
    return QStringLiteral("Windows capture backend scaffold is present but implementation is not finished yet.");
#endif
}

bool WindowsGraphicsCaptureRecorder::isRecording() const
{
    return recording_.load();
}

ScreenRecordingResult WindowsGraphicsCaptureRecorder::startRecording(const ScreenRecordingStartOptions& options)
{
#if !defined(Q_OS_WIN) || !defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
    (void)options;
    return ScreenRecordingResult::fail(unavailableReason());
#else
    if (recording_.load()) {
        return ScreenRecordingResult::fail(
            QStringLiteral("Recorder is already running."));
    }

    if (options.outputFilePath.trimmed().isEmpty()) {
        return ScreenRecordingResult::fail(
            QStringLiteral("Output file path is empty."));
    }

    if (options.nativeWindowHandle == 0) {
        return ScreenRecordingResult::fail(
            QStringLiteral("Native window handle is missing."));
    }

    if (impl_ == nullptr) {
        return ScreenRecordingResult::fail(
            QStringLiteral("Recorder implementation state is unavailable."));
    }

    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }

    const int frameRate = std::max(1, options.frameRate);
    impl_->stopRequested.store(false);
    impl_->running.store(false);
    impl_->lastMessage.clear();

    std::promise<ScreenRecordingResult> initPromise;
    std::future<ScreenRecordingResult> initFuture = initPromise.get_future();

    const QString outputPath = options.outputFilePath;
    const HWND hwnd = reinterpret_cast<HWND>(options.nativeWindowHandle);
    impl_->worker = std::thread(
        [this, outputPath, frameRate, hwnd](std::promise<ScreenRecordingResult> promise) mutable {
            ScopedCom com;
            if (!com.ok()) {
                const QString msg = formatHrMessage(QStringLiteral("CoInitializeEx failed"), com.hr());
                promise.set_value(ScreenRecordingResult::fail(msg));
                return;
            }

            ScopedMf mf;
            if (!mf.ok()) {
                const QString msg = formatHrMessage(QStringLiteral("MFStartup failed"), mf.hr());
                promise.set_value(ScreenRecordingResult::fail(msg));
                return;
            }

            GdiCaptureContext capture;
            QString error;
            if (!initializeCaptureContext(hwnd, capture, error)) {
                promise.set_value(ScreenRecordingResult::fail(error));
                return;
            }

            ComPtr<IMFSinkWriter> writer;
            DWORD streamIndex = 0;
            if (!configureSinkWriter(outputPath, capture.width, capture.height, frameRate, writer, streamIndex, error)) {
                promise.set_value(ScreenRecordingResult::fail(error));
                return;
            }

            impl_->running.store(true);
            recording_.store(true);
            promise.set_value(ScreenRecordingResult::ok());

            const LONGLONG frameDuration = 10'000'000LL / frameRate;
            LONGLONG sampleTime = 0;
            auto nextTick = std::chrono::steady_clock::now();

            while (!impl_->stopRequested.load()) {
                nextTick += std::chrono::milliseconds(1000 / frameRate);

                if (!captureFrame(capture, error)) {
                    impl_->lastMessage = error;
                    break;
                }

                if (!writeCapturedFrame(writer.Get(), streamIndex, capture, sampleTime, frameDuration, error)) {
                    impl_->lastMessage = error;
                    break;
                }

                sampleTime += frameDuration;
                std::this_thread::sleep_until(nextTick);
            }

            const HRESULT finalizeHr = writer->Finalize();
            if (FAILED(finalizeHr) && impl_->lastMessage.trimmed().isEmpty()) {
                impl_->lastMessage = formatHrMessage(QStringLiteral("IMFSinkWriter::Finalize failed"), finalizeHr);
            }

            if (impl_->lastMessage.trimmed().isEmpty()) {
                impl_->lastMessage = QStringLiteral("Embedded recording completed.");
            }

            impl_->running.store(false);
            recording_.store(false);
        },
        std::move(initPromise));

    const ScreenRecordingResult initResult = initFuture.get();
    if (!initResult.success) {
        if (impl_->worker.joinable()) {
            impl_->worker.join();
        }
        recording_.store(false);
        return initResult;
    }

    return ScreenRecordingResult::ok();
#endif
}

ScreenRecordingResult WindowsGraphicsCaptureRecorder::stopRecording()
{
#if !defined(Q_OS_WIN) || !defined(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
    return ScreenRecordingResult::fail(unavailableReason());
#else
    if (!recording_.load()) {
        return ScreenRecordingResult::fail(
            QStringLiteral("Recorder is not running."));
    }

    if (impl_ != nullptr) {
        impl_->stopRequested.store(true);
        if (impl_->worker.joinable()) {
            impl_->worker.join();
        }

        if (!impl_->lastMessage.trimmed().isEmpty()
            && !impl_->lastMessage.startsWith(QStringLiteral("Embedded recording completed."))) {
            recording_.store(false);
            return ScreenRecordingResult::fail(impl_->lastMessage);
        }
    }

    recording_.store(false);
    return ScreenRecordingResult::ok();
#endif
}

} // namespace capture
