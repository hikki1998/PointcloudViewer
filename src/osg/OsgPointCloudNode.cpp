#include "osg/OsgPointCloudNode.h"

#include <algorithm>

#include <QColor>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/Point>
#include <osg/StateSet>
#include <osg/Array>

#include "pointcloud/PointCloudData.h"

namespace
{
osg::Vec4 toOsgColor(const QColor& color)
{
    return osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f);
}

osg::Vec4 blendColor(const QColor& first, const QColor& second, float factor)
{
    const float clampedFactor = std::clamp(factor, 0.0f, 1.0f);
    const float inverseFactor = 1.0f - clampedFactor;

    return osg::Vec4(
        first.redF() * inverseFactor + second.redF() * clampedFactor,
        first.greenF() * inverseFactor + second.greenF() * clampedFactor,
        first.blueF() * inverseFactor + second.blueF() * clampedFactor,
        1.0f);
}

osg::Vec4 colorForElevation(float normalizedHeight)
{
    const QColor lowColor(40, 110, 230);
    const QColor midColor(56, 201, 166);
    const QColor highColor(244, 146, 66);

    if (normalizedHeight <= 0.5f) {
        return blendColor(lowColor, midColor, normalizedHeight * 2.0f);
    }

    return blendColor(midColor, highColor, (normalizedHeight - 0.5f) * 2.0f);
}

osg::Vec4 pointColor(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions,
    float minZ,
    float heightSpan)
{
    switch (visualizationOptions.colorMode) {
    case PointCloudColorMode::Elevation:
    {
        const float normalizedHeight = heightSpan > 0.0f ? (point.z - minZ) / heightSpan : 0.5f;
        return colorForElevation(normalizedHeight);
    }
    case PointCloudColorMode::SingleColor:
        return toOsgColor(visualizationOptions.singleColor);
    case PointCloudColorMode::Rgb:
    default:
        return osg::Vec4(
            static_cast<float>(point.r) / 255.0f,
            static_cast<float>(point.g) / 255.0f,
            static_cast<float>(point.b) / 255.0f,
            static_cast<float>(point.a) / 255.0f);
    }
}

osg::ref_ptr<osg::Geode> buildPointCloudGeode(
    const PointCloudData& pointCloudData,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    vertices->reserve(pointCloudData.size());
    colors->reserve(pointCloudData.size());

    const float minZ = pointCloudData.minBounds().z;
    const float heightSpan = std::max(0.0f, pointCloudData.maxBounds().z - minZ);

    for (const PointRecord& point : pointCloudData.points()) {
        vertices->push_back(osg::Vec3(point.x, point.y, point.z));
        colors->push_back(pointColor(point, visualizationOptions, minZ, heightSpan));
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    osg::ref_ptr<osg::Point> point = new osg::Point(visualizationOptions.pointSize);
    stateSet->setAttributeAndModes(point.get(), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);

    return geode;
}

osg::ref_ptr<osg::Geode> buildBoundingBoxGeode(const PointCloudData& pointCloudData)
{
    const PointRecord& minBounds = pointCloudData.minBounds();
    const PointRecord& maxBounds = pointCloudData.maxBounds();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    const osg::Vec4 boundsColor(0.96f, 0.77f, 0.28f, 1.0f);

    const osg::Vec3 p000(minBounds.x, minBounds.y, minBounds.z);
    const osg::Vec3 p100(maxBounds.x, minBounds.y, minBounds.z);
    const osg::Vec3 p010(minBounds.x, maxBounds.y, minBounds.z);
    const osg::Vec3 p110(maxBounds.x, maxBounds.y, minBounds.z);
    const osg::Vec3 p001(minBounds.x, minBounds.y, maxBounds.z);
    const osg::Vec3 p101(maxBounds.x, minBounds.y, maxBounds.z);
    const osg::Vec3 p011(minBounds.x, maxBounds.y, maxBounds.z);
    const osg::Vec3 p111(maxBounds.x, maxBounds.y, maxBounds.z);

    const osg::Vec3 edgePairs[] = {
        p000, p100, p100, p110, p110, p010, p010, p000,
        p001, p101, p101, p111, p111, p011, p011, p001,
        p000, p001, p100, p101, p110, p111, p010, p011
    };

    for (const osg::Vec3& vertex : edgePairs) {
        vertices->push_back(vertex);
        colors->push_back(boundsColor);
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);

    return geode;
}

osg::ref_ptr<osg::Geode> buildAxesGeode(const PointCloudData& pointCloudData)
{
    const PointRecord& minBounds = pointCloudData.minBounds();
    const PointRecord& maxBounds = pointCloudData.maxBounds();
    const float maxExtent = std::max({
        maxBounds.x - minBounds.x,
        maxBounds.y - minBounds.y,
        maxBounds.z - minBounds.z,
        1.0f
    });
    const float axisLength = maxExtent * 0.18f;
    const osg::Vec3 origin(minBounds.x, minBounds.y, minBounds.z);

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    const osg::Vec3 axisPairs[] = {
        origin, origin + osg::Vec3(axisLength, 0.0f, 0.0f),
        origin, origin + osg::Vec3(0.0f, axisLength, 0.0f),
        origin, origin + osg::Vec3(0.0f, 0.0f, axisLength)
    };

    const osg::Vec4 axisColors[] = {
        osg::Vec4(0.94f, 0.33f, 0.31f, 1.0f), osg::Vec4(0.94f, 0.33f, 0.31f, 1.0f),
        osg::Vec4(0.18f, 0.74f, 0.43f, 1.0f), osg::Vec4(0.18f, 0.74f, 0.43f, 1.0f),
        osg::Vec4(0.22f, 0.56f, 0.98f, 1.0f), osg::Vec4(0.22f, 0.56f, 0.98f, 1.0f)
    };

    for (const osg::Vec3& vertex : axisPairs) {
        vertices->push_back(vertex);
    }

    for (const osg::Vec4& axisColor : axisColors) {
        colors->push_back(axisColor);
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(3.0f), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);

    return geode;
}
}

osg::ref_ptr<osg::Group> OsgPointCloudNode::build(
    const PointCloudData& pointCloudData,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    osg::ref_ptr<osg::Group> root = new osg::Group();
    root->addChild(buildPointCloudGeode(pointCloudData, visualizationOptions).get());

    if (visualizationOptions.showBoundingBox) {
        root->addChild(buildBoundingBoxGeode(pointCloudData).get());
    }

    if (visualizationOptions.showAxes) {
        root->addChild(buildAxesGeode(pointCloudData).get());
    }

    return root;
}
