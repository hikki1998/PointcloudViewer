#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QMatrix4x4>
#include <QOpenGLFunctions_4_3_Compatibility>
#include <QString>

#include "gaussian/GaussianModel.h"

class GaussianRenderer final : protected QOpenGLFunctions_4_3_Compatibility
{
public:
    GaussianRenderer();
    ~GaussianRenderer();

    bool initialize(QString* errorMessage = nullptr);
    bool setModel(std::shared_ptr<const GaussianModel> model, QString* errorMessage = nullptr);
    void clear();
    void render(const QMatrix4x4& view, const QMatrix4x4& projection, int width, int height);
    [[nodiscard]] bool hasModel() const;

private:
    bool buildProgram(QString* errorMessage);
    unsigned int compileShader(unsigned int type, const char* source, QString* errorMessage);
    void requestSort(const QMatrix4x4& viewProjection);
    void sortLoop();
    void uploadSortedIndices();
    void destroyGlResources();

    std::shared_ptr<const GaussianModel> model_;
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int splatBuffer_ = 0;
    unsigned int indexBuffer_ = 0;
    int viewLocation_ = -1;
    int projectionLocation_ = -1;
    int viewportLocation_ = -1;
    bool initialized_ = false;

    std::thread sortThread_;
    std::mutex sortMutex_;
    std::condition_variable sortCondition_;
    bool stopSortThread_ = false;
    bool sortRequested_ = false;
    bool sortedIndicesReady_ = false;
    QMatrix4x4 pendingViewProjection_;
    QMatrix4x4 lastRequestedViewProjection_;
    std::vector<std::uint32_t> sortedIndices_;
};
