#include "gaussian/GaussianRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

#include <QOpenGLContext>

namespace
{
constexpr std::size_t kDepthBinCount = 65536;

const char* kVertexShader = R"(
#version 430 compatibility

struct SplatRecord {
    vec4 positionAlpha;
    vec4 covarianceA;
    vec4 covarianceB;
    vec4 color;
};

layout(std430, binding = 0) readonly buffer SplatBuffer { SplatRecord splats[]; };
layout(std430, binding = 1) readonly buffer IndexBuffer { uint sortedIndices[]; };

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uViewport;

out vec2 vGaussianCoord;
out vec4 vColor;

void main()
{
    const vec2 corners[4] = vec2[4](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    uint splatIndex = sortedIndices[gl_InstanceID];
    SplatRecord splat = splats[splatIndex];
    vec4 viewPosition = uView * vec4(splat.positionAlpha.xyz, 1.0);
    float depth = -viewPosition.z;
    if (depth <= 0.01 || splat.positionAlpha.w <= 0.003) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vGaussianCoord = vec2(10.0);
        vColor = vec4(0.0);
        return;
    }

    mat3 covarianceWorld = mat3(
        vec3(splat.covarianceA.x, splat.covarianceA.y, splat.covarianceA.z),
        vec3(splat.covarianceA.y, splat.covarianceB.x, splat.covarianceB.y),
        vec3(splat.covarianceA.z, splat.covarianceB.y, splat.covarianceB.z));
    mat3 rotation = mat3(uView);
    mat3 covarianceView = rotation * covarianceWorld * transpose(rotation);

    float focalX = 0.5 * uViewport.x * uProjection[0][0];
    float focalY = 0.5 * uViewport.y * uProjection[1][1];
    vec3 jacobianX = vec3(focalX / depth, 0.0, focalX * viewPosition.x / (depth * depth));
    vec3 jacobianY = vec3(0.0, focalY / depth, focalY * viewPosition.y / (depth * depth));
    float covXX = dot(jacobianX, covarianceView * jacobianX) + 0.3;
    float covXY = dot(jacobianX, covarianceView * jacobianY);
    float covYY = dot(jacobianY, covarianceView * jacobianY) + 0.3;

    float trace = covXX + covYY;
    float radius = sqrt(max(0.0, 0.25 * trace * trace - (covXX * covYY - covXY * covXY)));
    float lambdaMajor = max(0.1, 0.5 * trace + radius);
    float lambdaMinor = max(0.1, 0.5 * trace - radius);
    vec2 majorAxis = abs(covXY) > 1e-6
        ? normalize(vec2(lambdaMajor - covYY, covXY))
        : (covXX >= covYY ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
    vec2 minorAxis = vec2(-majorAxis.y, majorAxis.x);

    vec2 corner = corners[gl_VertexID];
    vec2 pixelOffset = 3.0 * (
        corner.x * sqrt(lambdaMajor) * majorAxis
        + corner.y * sqrt(lambdaMinor) * minorAxis);
    vec4 clipPosition = uProjection * viewPosition;
    clipPosition.xy += (2.0 * pixelOffset / uViewport) * clipPosition.w;
    gl_Position = clipPosition;
    vGaussianCoord = corner * 3.0;
    vColor = vec4(splat.color.rgb, splat.positionAlpha.w);
}
)";

const char* kFragmentShader = R"(
#version 430 compatibility

in vec2 vGaussianCoord;
in vec4 vColor;
out vec4 outColor;

void main()
{
    float power = -0.5 * dot(vGaussianCoord, vGaussianCoord);
    float alpha = min(0.99, vColor.a * exp(power));
    if (alpha < (1.0 / 255.0)) {
        discard;
    }
    outColor = vec4(vColor.rgb, alpha);
}
)";

bool matrixNearlyEqual(const QMatrix4x4& first, const QMatrix4x4& second)
{
    const float* a = first.constData();
    const float* b = second.constData();
    for (int index = 0; index < 16; ++index) {
        if (std::abs(a[index] - b[index]) > 1e-4f) {
            return false;
        }
    }
    return true;
}
}

GaussianRenderer::GaussianRenderer()
{
    sortThread_ = std::thread(&GaussianRenderer::sortLoop, this);
}

GaussianRenderer::~GaussianRenderer()
{
    {
        std::lock_guard<std::mutex> lock(sortMutex_);
        stopSortThread_ = true;
    }
    sortCondition_.notify_one();
    if (sortThread_.joinable()) {
        sortThread_.join();
    }
    destroyGlResources();
}

bool GaussianRenderer::initialize(QString* errorMessage)
{
    if (initialized_) {
        return true;
    }
    QOpenGLContext* context = QOpenGLContext::currentContext();
    if (context == nullptr || context->format().majorVersion() < 4
        || (context->format().majorVersion() == 4 && context->format().minorVersion() < 3)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Gaussian rendering requires OpenGL 4.3 or newer.");
        }
        return false;
    }
    if (!initializeOpenGLFunctions() || !buildProgram(errorMessage)) {
        return false;
    }
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &splatBuffer_);
    glGenBuffers(1, &indexBuffer_);
    initialized_ = true;
    return true;
}

bool GaussianRenderer::setModel(std::shared_ptr<const GaussianModel> model, QString* errorMessage)
{
    if (!initialized_ && !initialize(errorMessage)) {
        return false;
    }
    if (model == nullptr || model->empty()) {
        clear();
        return true;
    }

    GLint64 maxBlockSize = 0;
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxBlockSize);
    const std::size_t modelBytes = model->splats.size() * sizeof(GaussianGpuRecord);
    if (modelBytes > static_cast<std::size_t>(maxBlockSize)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Gaussian model requires %1 MiB in one GPU buffer, but the driver limit is %2 MiB.")
                .arg(modelBytes / (1024 * 1024))
                .arg(maxBlockSize / (1024 * 1024));
        }
        return false;
    }

    model_ = std::move(model);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, splatBuffer_);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(modelBytes),
        model_->splats.data(),
        GL_STATIC_DRAW);

    sortedIndices_.resize(model_->splats.size());
    std::iota(sortedIndices_.begin(), sortedIndices_.end(), 0u);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer_);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(sortedIndices_.size() * sizeof(std::uint32_t)),
        sortedIndices_.data(),
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    lastRequestedViewProjection_.fill(0.0f);
    return true;
}

void GaussianRenderer::clear()
{
    std::lock_guard<std::mutex> lock(sortMutex_);
    model_.reset();
    sortedIndices_.clear();
    sortRequested_ = false;
    sortedIndicesReady_ = false;
}

void GaussianRenderer::render(const QMatrix4x4& view, const QMatrix4x4& projection, int width, int height)
{
    if (!initialized_ || model_ == nullptr || model_->empty() || width <= 0 || height <= 0) {
        return;
    }

    uploadSortedIndices();
    requestSort(projection * view);

    glUseProgram(program_);
    glUniformMatrix4fv(viewLocation_, 1, GL_FALSE, view.constData());
    glUniformMatrix4fv(projectionLocation_, 1, GL_FALSE, projection.constData());
    glUniform2f(viewportLocation_, static_cast<float>(width), static_cast<float>(height));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, splatBuffer_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indexBuffer_);
    glBindVertexArray(vao_);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(model_->splats.size()));
    glBindVertexArray(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glUseProgram(0);
}

bool GaussianRenderer::hasModel() const
{
    return model_ != nullptr && !model_->empty();
}

bool GaussianRenderer::buildProgram(QString* errorMessage)
{
    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, kVertexShader, errorMessage);
    if (vertexShader == 0) {
        return false;
    }
    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader, errorMessage);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
        QByteArray log(std::max(1, length), '\0');
        glGetProgramInfoLog(program_, length, nullptr, log.data());
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Gaussian shader link failed: %1").arg(QString::fromLocal8Bit(log));
        }
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    viewLocation_ = glGetUniformLocation(program_, "uView");
    projectionLocation_ = glGetUniformLocation(program_, "uProjection");
    viewportLocation_ = glGetUniformLocation(program_, "uViewport");
    return true;
}

unsigned int GaussianRenderer::compileShader(unsigned int type, const char* source, QString* errorMessage)
{
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    QByteArray log(std::max(1, length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Gaussian shader compilation failed: %1").arg(QString::fromLocal8Bit(log));
    }
    glDeleteShader(shader);
    return 0;
}

void GaussianRenderer::requestSort(const QMatrix4x4& viewProjection)
{
    std::lock_guard<std::mutex> lock(sortMutex_);
    if (model_ == nullptr || sortRequested_ || matrixNearlyEqual(viewProjection, lastRequestedViewProjection_)) {
        return;
    }
    pendingViewProjection_ = viewProjection;
    lastRequestedViewProjection_ = viewProjection;
    sortRequested_ = true;
    sortCondition_.notify_one();
}

void GaussianRenderer::sortLoop()
{
    std::vector<std::uint32_t> localScratch;
    std::vector<std::uint32_t> bins(kDepthBinCount);
    std::vector<std::uint32_t> offsets(kDepthBinCount);

    while (true) {
        std::shared_ptr<const GaussianModel> model;
        QMatrix4x4 matrix;
        {
            std::unique_lock<std::mutex> lock(sortMutex_);
            sortCondition_.wait(lock, [this]() { return stopSortThread_ || sortRequested_; });
            if (stopSortThread_) {
                return;
            }
            model = model_;
            matrix = pendingViewProjection_;
            sortRequested_ = false;
        }
        if (model == nullptr || model->empty()) {
            continue;
        }

        const std::size_t count = model->splats.size();
        localScratch.resize(count);
        float minDepth = std::numeric_limits<float>::max();
        float maxDepth = std::numeric_limits<float>::lowest();
        const float* m = matrix.constData();
        for (std::size_t index = 0; index < count; ++index) {
            const float* position = model->splats[index].positionAlpha;
            const float depth = m[2] * position[0] + m[6] * position[1] + m[10] * position[2] + m[14];
            minDepth = std::min(minDepth, depth);
            maxDepth = std::max(maxDepth, depth);
        }

        std::fill(bins.begin(), bins.end(), 0u);
        const float scale = maxDepth > minDepth
            ? static_cast<float>(kDepthBinCount - 1) / (maxDepth - minDepth)
            : 0.0f;
        for (std::size_t index = 0; index < count; ++index) {
            const float* position = model->splats[index].positionAlpha;
            const float depth = m[2] * position[0] + m[6] * position[1] + m[10] * position[2] + m[14];
            const std::size_t bin = static_cast<std::size_t>(std::clamp((depth - minDepth) * scale, 0.0f, static_cast<float>(kDepthBinCount - 1)));
            ++bins[bin];
        }
        offsets[kDepthBinCount - 1] = 0;
        for (std::size_t bin = kDepthBinCount - 1; bin > 0; --bin) {
            offsets[bin - 1] = offsets[bin] + bins[bin];
        }
        for (std::size_t index = 0; index < count; ++index) {
            const float* position = model->splats[index].positionAlpha;
            const float depth = m[2] * position[0] + m[6] * position[1] + m[10] * position[2] + m[14];
            const std::size_t bin = static_cast<std::size_t>(std::clamp((depth - minDepth) * scale, 0.0f, static_cast<float>(kDepthBinCount - 1)));
            localScratch[offsets[bin]++] = static_cast<std::uint32_t>(index);
        }

        {
            std::lock_guard<std::mutex> lock(sortMutex_);
            if (model == model_) {
                sortedIndices_.swap(localScratch);
                sortedIndicesReady_ = true;
            }
        }
    }
}

void GaussianRenderer::uploadSortedIndices()
{
    std::lock_guard<std::mutex> lock(sortMutex_);
    if (!sortedIndicesReady_ || sortedIndices_.empty()) {
        return;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer_);
    glBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        static_cast<GLsizeiptr>(sortedIndices_.size() * sizeof(std::uint32_t)),
        sortedIndices_.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    sortedIndicesReady_ = false;
}

void GaussianRenderer::destroyGlResources()
{
    if (!initialized_ || QOpenGLContext::currentContext() == nullptr) {
        return;
    }
    glDeleteBuffers(1, &indexBuffer_);
    glDeleteBuffers(1, &splatBuffer_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
}
