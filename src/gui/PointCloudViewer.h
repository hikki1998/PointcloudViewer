#pragma once

#include <QString>
#include <QWidget>
#include <QGridLayout>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudData.h"

class QLabel;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace osg
{
class Group;
class Node;
}

class OsgWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OsgWidget(QWidget* parent = nullptr);
    ~OsgWidget() override;

    osgViewer::Viewer* getViewer() { return viewer_.get(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    osgGA::EventQueue* eventQueue() const;
    void updateViewport(int width, int height);
    static float toDevicePixels(float value, float devicePixelRatio);

    osg::ref_ptr<osgViewer::Viewer> viewer_;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> graphicsWindow_;
    bool initialized_ = false;
};

class PointCloudViewer final : public QWidget
{
    Q_OBJECT

public:
    explicit PointCloudViewer(QWidget* parent = nullptr);
    ~PointCloudViewer() override;

    bool loadPointCloud(const QString& filePath, QString* errorMessage = nullptr);
    void clearPointCloud();

    bool hasPointCloud() const;
    QString currentFilePath() const;
    const PointCloudData* pointCloudData() const;
    const PointCloudVisualizationOptions& visualizationOptions() const;

public slots:
    void setPointSize(int pointSize);
    void setColorMode(int colorModeIndex);
    void setColorMode(PointCloudColorMode colorMode);
    void setSingleColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setShowAxes(bool showAxes);
    void setShowBoundingBox(bool showBoundingBox);
    void resetView();
    void setViewPreset(PointCloudViewPreset viewPreset);

signals:
    void pointCloudLoaded();
    void pointCloudCleared();
    void visualizationOptionsChanged();

private:
    void createStatusPanel();
    void rebuildScene();
    void updateFooter();
    void updateMessage(const QString& title, const QString& detail);
    void applyClearColor();
    void applyViewPreset(PointCloudViewPreset viewPreset);

    QGridLayout* layout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    OsgWidget* osgWidget_ = nullptr;
    QWidget* statusPanel_ = nullptr;

    PointCloudData currentPointCloud_;
    QString currentFilePath_;
    PointCloudVisualizationOptions visualizationOptions_;

    osg::ref_ptr<osg::Group> rootGroup_;
    osg::ref_ptr<osg::Node> pointCloudNode_;
};
