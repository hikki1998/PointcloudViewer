#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QThread>

#include <iostream>

#include "gui/PointCloudViewer.h"

namespace
{
void pumpEvents(int durationMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(25);
    }
}

bool hasVisiblePixels(const QImage& image, int* nonBackgroundPixelCount)
{
    if (image.isNull()) {
        if (nonBackgroundPixelCount != nullptr) {
            *nonBackgroundPixelCount = 0;
        }
        return false;
    }

    const QRgb background = image.pixel(0, 0);
    int count = 0;

    for (int y = 0; y < image.height(); ++y) {
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = scanLine[x];
            const int redDelta = std::abs(qRed(pixel) - qRed(background));
            const int greenDelta = std::abs(qGreen(pixel) - qGreen(background));
            const int blueDelta = std::abs(qBlue(pixel) - qBlue(background));

            if (redDelta > 2 || greenDelta > 2 || blueDelta > 2) {
                ++count;
            }
        }
    }

    if (nonBackgroundPixelCount != nullptr) {
        *nonBackgroundPixelCount = count;
    }

    return count > 100;
}

bool runSmokeTest(const QString& filePath)
{
    PointCloudViewer viewer;
    viewer.resize(1024, 768);
    viewer.show();

    pumpEvents(500);

    QString errorMessage;
    if (!viewer.loadPointCloud(filePath, &errorMessage)) {
        std::cerr << "Load failed for " << filePath.toStdString() << ": "
                  << errorMessage.toStdString() << std::endl;
        return false;
    }

    pumpEvents(1000);

    QOpenGLWidget* glWidget = viewer.findChild<QOpenGLWidget*>();
    if (glWidget == nullptr) {
        std::cerr << "No QOpenGLWidget found for " << filePath.toStdString() << std::endl;
        return false;
    }

    const QImage frame = glWidget->grabFramebuffer();
    if (frame.isNull()) {
        std::cerr << "grabFramebuffer() returned a null image for "
                  << filePath.toStdString() << std::endl;
        return false;
    }

    int nonBackgroundPixelCount = 0;
    const bool visiblePixels = hasVisiblePixels(frame, &nonBackgroundPixelCount);

    std::cout << "Loaded " << filePath.toStdString()
              << " framebuffer=" << frame.width() << "x" << frame.height()
              << " nonBackgroundPixels=" << nonBackgroundPixelCount << std::endl;

    if (!visiblePixels) {
        std::cerr << "Rendered framebuffer appears empty for "
                  << filePath.toStdString() << std::endl;
        return false;
    }

    return true;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setVersion(2, 1);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Usage: LASViewerSmokeTest <file1> [file2 ...]" << std::endl;
        return 2;
    }

    bool allPassed = true;
    for (int index = 1; index < argc; ++index) {
        const QString filePath = QString::fromLocal8Bit(argv[index]);
        if (!runSmokeTest(filePath)) {
            allPassed = false;
        }
    }

    return allPassed ? 0 : 1;
}
