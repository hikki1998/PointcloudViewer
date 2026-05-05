#include "osg/OsgPointCloudNode.h"

#include <algorithm>
#include <cmath>

#include <QColor>

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osg/Point>
#include <osg/Program>
#include <osg/Shader>
#include <osg/StateSet>
#include <osg/Uniform>

#include "domain/ClassificationEditStore.h"
#include "pointcloud/PointCloudData.h"

namespace
{
#if !defined(GL_POINT_SPRITE)
#define GL_POINT_SPRITE 0x8861
#endif

constexpr unsigned int kDatasetIdAttributeLocation = 6;

osg::Vec4 toOsgColor(const QColor& color)
{
    return osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f);
}

osg::Vec4ub toOsgColorBytes(const QColor& color)
{
    return osg::Vec4ub(
        static_cast<unsigned char>(color.red()),
        static_cast<unsigned char>(color.green()),
        static_cast<unsigned char>(color.blue()),
        255);
}

float clampUnit(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

osg::Program* buildPointCloudProgram()
{
    static const char* kVertexShaderSource = R"(
        #version 120

        varying vec4 vPointColor;
        varying float vViewDepth;
        varying vec3 vWorldPos;
        varying float vClipDatasetId;

        uniform vec3 uSceneOrigin;
        attribute float aDatasetId;

        void main()
        {
            vec4 viewPosition = gl_ModelViewMatrix * gl_Vertex;
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            vPointColor = gl_Color;
            vViewDepth = max(0.0, -viewPosition.z);
            vWorldPos = gl_Vertex.xyz + uSceneOrigin;
            vClipDatasetId = aDatasetId;
        }
    )";

    static const char* kFragmentShaderSource = R"(
        #version 120

        uniform float uPointOpacity;
        uniform float uDepthCueStrength;
        uniform float uEdlStrength;
        uniform float uUseRoundSplats;
        uniform vec3 uBackgroundColor;

        uniform float uClipEnabled;
        uniform int uClipMode;
        uniform int uClipScope;
        uniform float uClipActiveDatasetId;
        uniform int uClipPlaneCount;
        uniform vec4 uClipPlanes[48];

        varying vec4 vPointColor;
        varying float vViewDepth;
        varying vec3 vWorldPos;
        varying float vClipDatasetId;

        bool pointInsideClipVolume(vec3 p)
        {
            for (int planeIndex = 0; planeIndex < 48; ++planeIndex) {
                if (planeIndex >= uClipPlaneCount) {
                    break;
                }

                vec4 plane = uClipPlanes[planeIndex];
                if (dot(plane.xyz, p) + plane.w < 0.0) {
                    return false;
                }
            }

            return true;
        }

        void main()
        {
            bool clipAppliesToPoint = uClipScope != 0 || abs(vClipDatasetId - uClipActiveDatasetId) < 0.5;
            if (uClipEnabled > 0.5 && clipAppliesToPoint) {
                bool insideClip = pointInsideClipVolume(vWorldPos);
                if (uClipMode == 0) {
                    if (!insideClip) discard;
                } else {
                    if (insideClip) discard;
                }
            }

            vec2 centeredCoord = gl_PointCoord * 2.0 - vec2(1.0, 1.0);
            float radialDistance = length(centeredCoord);
            if (uUseRoundSplats > 0.5 && radialDistance > 1.0) {
                discard;
            }

            float dome = clamp(1.0 - dot(centeredCoord, centeredCoord), 0.0, 1.0);
            float splatAlpha = uUseRoundSplats > 0.5 ? smoothstep(0.0, 0.22, dome) : 1.0;
            float rim = smoothstep(0.15, 1.0, radialDistance);
            float edlShade = 1.0 - clamp(uEdlStrength, 0.0, 1.0) * rim * 0.65;

            float depthCue = exp(-clamp(uDepthCueStrength, 0.0, 1.0) * vViewDepth * 0.0035);
            depthCue = clamp(depthCue, 0.28, 1.0);

            vec3 shadedColor = vPointColor.rgb * edlShade;
            shadedColor = mix(uBackgroundColor, shadedColor, depthCue);

            float alpha = clamp(vPointColor.a * clamp(uPointOpacity, 0.0, 1.0) * splatAlpha, 0.0, 1.0);
            if (alpha <= 0.01) {
                discard;
            }

            gl_FragColor = vec4(shadedColor, alpha);
        }
    )";

    osg::ref_ptr<osg::Program> program = new osg::Program();
    program->addBindAttribLocation("aDatasetId", kDatasetIdAttributeLocation);
    program->addShader(new osg::Shader(osg::Shader::VERTEX, kVertexShaderSource));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, kFragmentShaderSource));
    return program.release();
}

void applyPointCloudShaderState(
    osg::StateSet* stateSet,
    const PointCloudVisualizationOptions& visualizationOptions,
    const osg::Vec3d& sceneOrigin)
{
    if (stateSet == nullptr) {
        return;
    }

    stateSet->setAttributeAndModes(buildPointCloudProgram(), osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(
        new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    stateSet->addUniform(new osg::Uniform("uPointOpacity", clampUnit(visualizationOptions.pointOpacity)));
    stateSet->addUniform(new osg::Uniform("uDepthCueStrength", clampUnit(visualizationOptions.depthCueStrength)));
    stateSet->addUniform(new osg::Uniform("uEdlStrength", clampUnit(visualizationOptions.edlStrength)));
    stateSet->addUniform(new osg::Uniform("uUseRoundSplats", visualizationOptions.useRoundSplats ? 1.0f : 0.0f));

    osg::Vec4 backgroundColor = toOsgColor(visualizationOptions.backgroundColor);
    stateSet->addUniform(new osg::Uniform(
        "uBackgroundColor",
        osg::Vec3(backgroundColor.r(), backgroundColor.g(), backgroundColor.b())));
    stateSet->addUniform(new osg::Uniform(
        "uSceneOrigin",
        osg::Vec3(
            static_cast<float>(sceneOrigin.x()),
            static_cast<float>(sceneOrigin.y()),
            static_cast<float>(sceneOrigin.z()))));

    const ClipRegion& clip = visualizationOptions.clipRegion;
    const int clipPlaneCount = clip.isActive()
        ? std::min(static_cast<int>(clip.planes.size()), kMaxClipPlaneCount)
        : 0;
    stateSet->addUniform(new osg::Uniform("uClipEnabled", clipPlaneCount > 0 ? 1.0f : 0.0f));
    stateSet->addUniform(new osg::Uniform("uClipMode", clip.keepInside ? 0 : 1));
    stateSet->addUniform(new osg::Uniform("uClipScope", static_cast<int>(clip.scope)));
    stateSet->addUniform(new osg::Uniform("uClipActiveDatasetId", static_cast<float>(clip.activeDatasetId)));
    stateSet->addUniform(new osg::Uniform("uClipPlaneCount", clipPlaneCount));

    osg::ref_ptr<osg::Uniform> clipPlanesUniform =
        new osg::Uniform(osg::Uniform::FLOAT_VEC4, "uClipPlanes", kMaxClipPlaneCount);
    for (int planeIndex = 0; planeIndex < clipPlaneCount; ++planeIndex) {
        const ClipPlane& plane = clip.planes.at(static_cast<std::size_t>(planeIndex));
        clipPlanesUniform->setElement(
            planeIndex,
            osg::Vec4(
            static_cast<float>(plane.normal.x()),
            static_cast<float>(plane.normal.y()),
            static_cast<float>(plane.normal.z()),
            static_cast<float>(plane.distance)));
    }
    stateSet->addUniform(clipPlanesUniform.get());

    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setMode(GL_POINT_SPRITE, osg::StateAttribute::ON);
    stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
}

QColor blendColor(const QColor& first, const QColor& second, float factor)
{
    const float clampedFactor = std::clamp(factor, 0.0f, 1.0f);
    const float inverseFactor = 1.0f - clampedFactor;

    return QColor(
        static_cast<int>(std::lround(first.red() * inverseFactor + second.red() * clampedFactor)),
        static_cast<int>(std::lround(first.green() * inverseFactor + second.green() * clampedFactor)),
        static_cast<int>(std::lround(first.blue() * inverseFactor + second.blue() * clampedFactor)));
}

QColor colorForElevation(float normalizedHeight)
{
    const QColor lowColor(40, 110, 230);
    const QColor midColor(56, 201, 166);
    const QColor highColor(244, 146, 66);

    if (normalizedHeight <= 0.5f) {
        return blendColor(lowColor, midColor, normalizedHeight * 2.0f);
    }

    return blendColor(midColor, highColor, (normalizedHeight - 0.5f) * 2.0f);
}

QColor colorForClassification(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    if (!point.hasClassification) {
        return visualizationOptions.classificationFallbackColor;
    }

    int classification = static_cast<int>(point.classification);
    if (visualizationOptions.classificationEditStore != nullptr && point.sourceDatasetId >= 0) {
        const auto datasetPathIt = visualizationOptions.classificationDatasetPathsById.constFind(point.sourceDatasetId);
        if (datasetPathIt != visualizationOptions.classificationDatasetPathsById.constEnd()) {
            classification = visualizationOptions.classificationEditStore->effectiveClassification(
                datasetPathIt.value(),
                point.sourcePointIndex,
                classification);
        }
    }
    const auto colorIt = visualizationOptions.classificationColors.constFind(classification);
    if (colorIt != visualizationOptions.classificationColors.constEnd()) {
        return colorIt.value();
    }

    return visualizationOptions.classificationFallbackColor;
}

bool pointVisibleForClassification(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    const bool fallbackVisible = visualizationOptions.classificationVisibility.value(-1, true);
    if (!point.hasClassification) {
        return fallbackVisible;
    }

    int classification = static_cast<int>(point.classification);
    if (visualizationOptions.classificationEditStore != nullptr && point.sourceDatasetId >= 0) {
        const auto datasetPathIt = visualizationOptions.classificationDatasetPathsById.constFind(point.sourceDatasetId);
        if (datasetPathIt != visualizationOptions.classificationDatasetPathsById.constEnd()) {
            classification = visualizationOptions.classificationEditStore->effectiveClassification(
                datasetPathIt.value(),
                point.sourcePointIndex,
                classification);
        }
    }

    return visualizationOptions.classificationVisibility.value(
        classification,
        fallbackVisible);
}

osg::Vec4ub pointColor(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions,
    double minZ,
    double heightSpan)
{
    switch (visualizationOptions.colorMode) {
    case PointCloudColorMode::Elevation:
    {
        const double normalizedHeight = heightSpan > 0.0 ? (point.z - minZ) / heightSpan : 0.5;
        return toOsgColorBytes(colorForElevation(static_cast<float>(std::clamp(normalizedHeight, 0.0, 1.0))));
    }
    case PointCloudColorMode::SingleColor:
        return toOsgColorBytes(visualizationOptions.singleColor);
    case PointCloudColorMode::Classification:
        return toOsgColorBytes(colorForClassification(point, visualizationOptions));
    case PointCloudColorMode::Rgb:
    default:
        return osg::Vec4ub(point.r, point.g, point.b, point.a);
    }
}

osg::ref_ptr<osg::Node> buildPointCloudNode(
    const PointCloudData& pointCloudData,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    const PointRecord& minBounds = pointCloudData.minBounds();
    const PointRecord& maxBounds = pointCloudData.maxBounds();
    const osg::Vec3d sceneOrigin(
        (minBounds.x + maxBounds.x) * 0.5,
        (minBounds.y + maxBounds.y) * 0.5,
        (minBounds.z + maxBounds.z) * 0.5);

    const std::size_t pointCount = pointCloudData.size();
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4ubArray> colors = new osg::Vec4ubArray();
    osg::ref_ptr<osg::FloatArray> datasetIds = new osg::FloatArray();
    vertices->reserve(pointCount);
    colors->reserve(pointCount);
    datasetIds->reserve(pointCount);

    const double minZ = minBounds.z;
    const double heightSpan = std::max(0.0, maxBounds.z - minZ);

    const std::vector<PointRecord>& points = pointCloudData.points();
    for (std::size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        const PointRecord& point = points[pointIndex];
        if (!pointVisibleForClassification(point, visualizationOptions)) {
            continue;
        }

        vertices->push_back(osg::Vec3(
            static_cast<float>(point.x - sceneOrigin.x()),
            static_cast<float>(point.y - sceneOrigin.y()),
            static_cast<float>(point.z - sceneOrigin.z())));
        colors->push_back(pointColor(point, visualizationOptions, minZ, heightSpan));
        datasetIds->push_back(static_cast<float>(point.sourceDatasetId));
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->setVertexAttribArray(kDatasetIdAttributeLocation, datasetIds.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    osg::ref_ptr<osg::Point> point = new osg::Point(visualizationOptions.pointSize);
    stateSet->setAttributeAndModes(point.get(), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    applyPointCloudShaderState(stateSet, visualizationOptions, sceneOrigin);

    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform();
    transform->setMatrix(osg::Matrixd::translate(sceneOrigin));
    transform->addChild(geode.get());
    return transform;
}

osg::ref_ptr<osg::Geode> buildBoundingBoxGeode(const PointRecord& minBounds, const PointRecord& maxBounds)
{
    osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    const osg::Vec4 boundsColor(0.96f, 0.77f, 0.28f, 1.0f);

    const osg::Vec3d p000(minBounds.x, minBounds.y, minBounds.z);
    const osg::Vec3d p100(maxBounds.x, minBounds.y, minBounds.z);
    const osg::Vec3d p010(minBounds.x, maxBounds.y, minBounds.z);
    const osg::Vec3d p110(maxBounds.x, maxBounds.y, minBounds.z);
    const osg::Vec3d p001(minBounds.x, minBounds.y, maxBounds.z);
    const osg::Vec3d p101(maxBounds.x, minBounds.y, maxBounds.z);
    const osg::Vec3d p011(minBounds.x, maxBounds.y, maxBounds.z);
    const osg::Vec3d p111(maxBounds.x, maxBounds.y, maxBounds.z);

    const osg::Vec3d edgePairs[] = {
        p000, p100, p100, p110, p110, p010, p010, p000,
        p001, p101, p101, p111, p111, p011, p011, p001,
        p000, p001, p100, p101, p110, p111, p010, p011
    };

    for (const osg::Vec3d& vertex : edgePairs) {
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

osg::ref_ptr<osg::Geode> buildAxesGeode(const PointRecord& minBounds, const PointRecord& maxBounds)
{
    const double maxExtent = std::max({
        maxBounds.x - minBounds.x,
        maxBounds.y - minBounds.y,
        maxBounds.z - minBounds.z,
        1.0
    });
    const double axisLength = maxExtent * 0.18;
    const osg::Vec3d origin(minBounds.x, minBounds.y, minBounds.z);

    osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    const osg::Vec3d axisPairs[] = {
        origin, origin + osg::Vec3d(axisLength, 0.0, 0.0),
        origin, origin + osg::Vec3d(0.0, axisLength, 0.0),
        origin, origin + osg::Vec3d(0.0, 0.0, axisLength)
    };

    const osg::Vec4 axisColors[] = {
        osg::Vec4(0.94f, 0.33f, 0.31f, 1.0f), osg::Vec4(0.94f, 0.33f, 0.31f, 1.0f),
        osg::Vec4(0.18f, 0.74f, 0.43f, 1.0f), osg::Vec4(0.18f, 0.74f, 0.43f, 1.0f),
        osg::Vec4(0.22f, 0.56f, 0.98f, 1.0f), osg::Vec4(0.22f, 0.56f, 0.98f, 1.0f)
    };

    for (const osg::Vec3d& vertex : axisPairs) {
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

OsgPointCloudNode::OsgPointCloudNode()
{
    root_ = new osg::Group();
    tileGroup_ = new osg::Group();
    root_->addChild(tileGroup_.get());
}

osg::ref_ptr<osg::Group> OsgPointCloudNode::build(
    const PointCloudData& pointCloudData,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    osg::ref_ptr<osg::Group> root = new osg::Group();
    root->addChild(buildPointCloudNode(pointCloudData, visualizationOptions).get());

    if (visualizationOptions.showBoundingBox) {
        root->addChild(buildBoundingBoxGeode(pointCloudData.minBounds(), pointCloudData.maxBounds()).get());
    }

    if (visualizationOptions.showAxes) {
        root->addChild(buildAxesGeode(pointCloudData.minBounds(), pointCloudData.maxBounds()).get());
    }

    return root;
}

osg::Group* OsgPointCloudNode::root() const
{
    return root_.get();
}

void OsgPointCloudNode::clear()
{
    tileEntries_.clear();
    if (tileGroup_.valid()) {
        tileGroup_->removeChildren(0, tileGroup_->getNumChildren());
    }
    hasSceneBounds_ = false;
    sceneMinBounds_ = PointRecord();
    sceneMaxBounds_ = PointRecord();
    rebuildAuxiliaryNodes();
}

void OsgPointCloudNode::setVisualizationOptions(const PointCloudVisualizationOptions& visualizationOptions)
{
    visualizationOptions_ = visualizationOptions;
    rebuildAllTiles();
    rebuildAuxiliaryNodes();
}

void OsgPointCloudNode::setSceneBounds(const PointRecord& minBounds, const PointRecord& maxBounds, bool hasData)
{
    sceneMinBounds_ = minBounds;
    sceneMaxBounds_ = maxBounds;
    hasSceneBounds_ = hasData;
    rebuildAuxiliaryNodes();
}

void OsgPointCloudNode::setPreviewTiles(const PointCloudTileSet& tileSet)
{
    tileEntries_.clear();
    if (tileGroup_.valid()) {
        tileGroup_->removeChildren(0, tileGroup_->getNumChildren());
    }

    setSceneBounds(tileSet.minBounds, tileSet.maxBounds, !tileSet.empty());

    for (const PointCloudTileData& tile : tileSet.tiles) {
        TileNodeEntry entry;
        entry.id = tile.id;
        entry.minBounds = tile.minBounds;
        entry.maxBounds = tile.maxBounds;
        entry.previewPointCloud = tile.pointCloud;
        entry.group = new osg::Group();
        tileGroup_->addChild(entry.group.get());
        tileEntries_.insert(entry.id, entry);
    }

    rebuildAllTiles();
}

void OsgPointCloudNode::setFullTileData(const PointCloudTileId& tileId, const std::shared_ptr<PointCloudData>& pointCloud)
{
    TileNodeEntry* tileEntry = nullptr;
    auto it = tileEntries_.find(tileId);
    if (it == tileEntries_.end()) {
        TileNodeEntry entry;
        entry.id = tileId;
        entry.group = new osg::Group();
        if (pointCloud != nullptr && !pointCloud->empty()) {
            entry.minBounds = pointCloud->minBounds();
            entry.maxBounds = pointCloud->maxBounds();
        }
        it = tileEntries_.insert(tileId, entry);
        tileGroup_->addChild(it->group.get());
    }

    tileEntry = &it.value();
    tileEntry->fullPointCloud = pointCloud;
    if (pointCloud != nullptr && !pointCloud->empty()) {
        tileEntry->minBounds = pointCloud->minBounds();
        tileEntry->maxBounds = pointCloud->maxBounds();
    }
    tileEntry->fullNode = nullptr;
    updateTileNode(*tileEntry);
}

bool OsgPointCloudNode::setTilePromoted(const PointCloudTileId& tileId, bool promoted)
{
    auto it = tileEntries_.find(tileId);
    if (it == tileEntries_.end()) {
        return false;
    }

    if (it->promoted == promoted) {
        return true;
    }

    it->promoted = promoted;
    updateTileNode(it.value());
    return true;
}

QList<PointCloudTileId> OsgPointCloudNode::tileIds() const
{
    return tileEntries_.keys();
}

void OsgPointCloudNode::rebuildAllTiles()
{
    for (auto it = tileEntries_.begin(); it != tileEntries_.end(); ++it) {
        it->previewNode = nullptr;
        it->fullNode = nullptr;
        updateTileNode(it.value());
    }
}

void OsgPointCloudNode::rebuildAuxiliaryNodes()
{
    if (!root_.valid()) {
        return;
    }

    if (boundsNode_.valid()) {
        root_->removeChild(boundsNode_.get());
        boundsNode_ = nullptr;
    }

    if (axesNode_.valid()) {
        root_->removeChild(axesNode_.get());
        axesNode_ = nullptr;
    }

    if (!hasSceneBounds_) {
        return;
    }

    if (visualizationOptions_.showBoundingBox) {
        boundsNode_ = buildBoundingBoxGeode(sceneMinBounds_, sceneMaxBounds_);
        if (boundsNode_.valid()) {
            root_->addChild(boundsNode_.get());
        }
    }

    if (visualizationOptions_.showAxes) {
        axesNode_ = buildAxesGeode(sceneMinBounds_, sceneMaxBounds_);
        if (axesNode_.valid()) {
            root_->addChild(axesNode_.get());
        }
    }
}

void OsgPointCloudNode::updateTileNode(TileNodeEntry& tileEntry)
{
    if (!tileEntry.group.valid()) {
        tileEntry.group = new osg::Group();
    }

    tileEntry.group->removeChildren(0, tileEntry.group->getNumChildren());

    osg::ref_ptr<osg::Node> activeNode;
    if (tileEntry.promoted && tileEntry.fullPointCloud != nullptr && !tileEntry.fullPointCloud->empty()) {
        if (!tileEntry.fullNode.valid()) {
            tileEntry.fullNode = buildTileNode(tileEntry.fullPointCloud);
        }
        activeNode = tileEntry.fullNode;
    } else if (tileEntry.previewPointCloud != nullptr && !tileEntry.previewPointCloud->empty()) {
        if (!tileEntry.previewNode.valid()) {
            tileEntry.previewNode = buildTileNode(tileEntry.previewPointCloud);
        }
        activeNode = tileEntry.previewNode;
    } else if (tileEntry.fullPointCloud != nullptr && !tileEntry.fullPointCloud->empty()) {
        if (!tileEntry.fullNode.valid()) {
            tileEntry.fullNode = buildTileNode(tileEntry.fullPointCloud);
        }
        activeNode = tileEntry.fullNode;
    }

    if (activeNode.valid()) {
        tileEntry.group->addChild(activeNode.get());
    }
}

osg::ref_ptr<osg::Node> OsgPointCloudNode::buildTileNode(const std::shared_ptr<PointCloudData>& pointCloud) const
{
    if (pointCloud == nullptr || pointCloud->empty()) {
        return nullptr;
    }

    return buildPointCloudNode(*pointCloud, visualizationOptions_);
}
