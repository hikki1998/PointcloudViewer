# 3D Gaussian Splatting PLY 接入调研

日期：2026-09-18

## 结论

推荐首选 **原生 OpenGL 4.3+ 高斯栅格化后端**，不把高斯模型转换成 OSG 点节点，也不直接引入 CUDA、训练框架或完整 Vulkan viewer。

建议实现形态：

- 独立数据层：`GaussianModel` / `GaussianPlyReader`
- 独立渲染层：`GaussianRenderer`，使用 SSBO、instanced quad、球谐着色和 alpha blending
- 排序分两步：先做异步 CPU bucket/counting sort 验证全链路，再接 OpenGL compute radix sort
- 与现有 viewer 的优先方案：把当前 OpenGL 上下文升级到 4.3 Compatibility Profile，在同一 `QOpenGLWidget` 中按模型类型调用 OSG 或原生高斯 renderer
- 若 OSG 4.3 兼容回归不过，再退到独立 `GaussianOpenGLWidget`，通过 stacked layout 与 OSG viewer 互斥切换

Vulkan 方案适合作为后续高性能后端，不建议作为首版。CUDA 方案不建议作为产品基础依赖。

## 当前仓库约束

### 图形上下文

当前应用和 smoke test 都在启动时强制设置：

- OpenGL 2.1
- Compatibility Profile
- 4x MSAA

位置：

- `src/main.cpp`
- `examples/viewer_smoke_test.cpp`

现有点云 shader 使用 GLSL 1.20，位置为 `src/osg/OsgPointCloudNode.cpp`。现代高斯 renderer 通常需要 SSBO 和 compute shader，其中 compute shader 从 OpenGL 4.3 才进入核心规范。

因此，当前上下文不能直接运行现代 GPU 高斯渲染器。首个技术门槛不是 PLY 解析，而是验证 OSG 和既有 GLSL 1.20 在 OpenGL 4.3 Compatibility Profile 下无回归。

### Viewer 结构

`OsgWidget` 已经是 `QOpenGLWidget`，并拥有：

- OpenGL 生命周期：`initializeGL/resizeGL/paintGL`
- 完整鼠标、滚轮、键盘输入入口
- OSG trackball manipulator 和相机矩阵
- Qt 覆盖层刷新时机

这使“在现有 OpenGL widget 内调用独立 renderer”具备可行性。高斯 renderer 不需要成为 `osg::Node`，但可以复用相机、输入和窗口生命周期。

### 产品数据模型

现有 `PointCloudData` 面向 LAS 点记录、分类、量测、裁剪和业务分析。高斯 PLY 包含完全不同的数据：

- `x/y/z`
- `f_dc_0..2`
- `f_rest_0..44`，通常为 3 阶球谐
- `opacity`，存储值需要 sigmoid
- `scale_0..2`，存储值需要 exp
- `rot_0..3`，四元数需要归一化

高斯模型不应塞入 `PointCloudData`。强行复用会污染 LAS 业务模型，并且无法表达协方差、球谐和透明度。

### 硬件基线

当前开发机为 NVIDIA GeForce GTX 1050 Ti，4 GB VRAM，支持现代 OpenGL，但显存余量有限。因此首版应：

- 不依赖 RTX、mesh shader 或 CUDA
- 支持 SH degree 0-3 可调
- 加载前估算 RAM/VRAM
- 对超预算模型给出明确错误，不依赖驱动 OOM
- 保留 DC-only/低 SH 降级路径

## PLY 格式边界

`.ply` 只是容器后缀，不能据此认定文件是高斯模型。建议读取 header 后按字段签名识别：

必需字段：

```text
x y z
f_dc_0 f_dc_1 f_dc_2
opacity
scale_0 scale_1 scale_2
rot_0 rot_1 rot_2 rot_3
```

可选字段：

```text
f_rest_0 ... f_rest_44
nx ny nz
```

首版建议仅支持 `binary_little_endian 1.0`。这是主流 3DGS 输出，也是几个原生 viewer 的共同基础。ASCII PLY 和任意普通 PLY 暂不支持，错误信息中列出缺失字段。

内部模型建议保留原始语义，而不是加载时永久转成普通点：

```cpp
struct GaussianModel
{
    std::vector<float> positions;
    std::vector<float> scales;
    std::vector<float> rotations;
    std::vector<float> opacities;
    std::vector<float> sphericalHarmonics;
    int shDegree = 0;
    Bounds bounds;
};
```

上传 GPU 时再生成 renderer 需要的 packed buffer，例如 position/alpha、协方差和 SH。这样解析、坐标变换和渲染布局不会互相锁死。

## 开源方案评估

### 1. Splatapult

仓库：[hyperlogic/splatapult](https://github.com/hyperlogic/splatapult)

- 许可证：MIT
- C++17 + OpenGL
- 支持标准 3DGS PLY、完整 SH
- compute shader 做可见性和深度 key
- GPU radix sort，instanced/geometry shader 渲染
- Windows 可构建

优点：和目标最接近，包含 PLY 转换、协方差、SH、GPU 排序完整链路。

问题：完整项目依赖 SDL2、GLEW、GLM、Eigen、OpenXR、PNG、JSON 等；直接作为子项目引入过重。shader 中有 OpenGL 4.6/subgroup 快速路径，不能作为所有显卡的唯一实现。

结论：**最适合作为 OpenGL GPU 算法参考，不适合整仓接入。** 可以复用 MIT 许可下的必要数学和 shader 思路，并保留不依赖 subgroup 的通用路径。

### 2. splatview

仓库：[lukaslaobeyer/splatview](https://github.com/lukaslaobeyer/splatview)

- 许可证：MIT
- C++ + OpenGL 4.3
- SSBO + instanced quad
- 后台线程进行 CPU counting sort
- 支持 0-3 阶 SH
- PLY reader 仅支持 binary little-endian

优点：结构小，渲染和排序边界清楚；异步 counting sort 很适合作为低端 GPU fallback 和首个可运行版本。

问题：依赖 Eigen、LLFIO、GLFW/Bazel；CPU 排序会在高速相机运动时产生排序延迟。

结论：**最适合作为 MVP 结构参考。** 不需要引入其构建系统或 LLFIO，可用 Qt `QFile`/memory map 和本仓库线程模式重写最小实现。

### 3. NVIDIA vk_gaussian_splatting

仓库：[nvpro-samples/vk_gaussian_splatting](https://github.com/nvpro-samples/vk_gaussian_splatting)

- 许可证：Apache-2.0
- C++ + Vulkan
- GPU radix sort、CPU 异步排序、vertex/mesh/ray tracing 多管线
- 使用 `miniply` 解析 PLY
- 工程质量和文档最好

优点：产品级参考价值高，格式、坐标系、异步加载、GPU/CPU 排序和性能分析都较完整。

问题：依赖 Vulkan、nvpro_core2、Slang、VRDX，默认能力远超本需求；当前文档要求 Vulkan 1.4 或 NVIDIA GPU，直接嵌入 Qt/OSG 应用成本高。

结论：**作为算法、数据布局和性能验证基准；不作为首版依赖。**

### 4. vkgs

仓库：[jaesung-cs/vkgs](https://github.com/jaesung-cs/vkgs)

- 许可证：MIT
- Vulkan 全 GPU 排序和间接绘制
- 作者明确说明当前不活跃维护，并建议使用 NVIDIA 项目

结论：可参考 radix sort 和 Vulkan 数据流，不建议选为主要上游。

### 5. graphdeco / gsplat

- [graphdeco-inria/gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting)：原始格式和数学事实来源，但代码许可证限制为非商业研究用途。
- [nerfstudio-project/gsplat](https://github.com/nerfstudio-project/gsplat)：Apache-2.0，但主要是 Python/PyTorch/CUDA 可微栅格化训练组件。

结论：前者只作为格式依据，后者不适合本桌面查看器运行时。

## 方案对比

| 方案 | 首版成本 | 性能上限 | 硬件覆盖 | 与当前 Qt/OSG 适配 | 结论 |
|---|---:|---:|---:|---:|---|
| OSG 点精灵模拟 | 低 | 低 | 高 | 高 | 不能正确表达椭圆协方差和排序，不采用 |
| OpenGL 4.3 + CPU 异步排序 | 中 | 中 | 高 | 高 | MVP/低端 fallback |
| OpenGL 4.3+ + compute radix sort | 中高 | 高 | 较高 | 高 | 推荐目标方案 |
| CUDA rasterizer | 高 | 高 | 仅 NVIDIA | 中 | 训练框架依赖重，不采用 |
| Vulkan renderer | 高 | 很高 | 中 | 低 | 二期后端 |
| WebEngine/WebGL viewer | 中 | 中 | 高 | 中 | 数据复制和业务交互成本高，不采用 |

## 推荐架构

### 模块边界

建议新增最少四个组件：

```text
src/gaussian/GaussianModel.h
src/gaussian/GaussianPlyReader.h/.cpp
src/gaussian/GaussianRenderer.h/.cpp
src/gaussian/shaders/*
```

职责：

- `GaussianPlyReader`：header 校验、字段读取、数值变换、bounds、取消和进度
- `GaussianModel`：CPU 数据与元信息，不依赖 Qt OpenGL/OSG
- `GaussianRenderer`：GPU 资源、排序、shader、绘制、显存估算
- `OsgWidget`：持有 renderer、提供相机矩阵和 framebuffer 生命周期

暂时不增加通用 renderer interface。当前只有一个新增实现，先用直接成员和模式枚举即可，避免为未来后端预建抽象。

### 渲染主链路

```text
PLY header 识别
  -> 后台解析 GaussianModel
  -> UI 线程/OpenGL context 上传 packed SSBO
  -> 相机变化时计算可见 splat 和深度 key
  -> CPU counting sort 或 compute radix sort
  -> sorted index buffer
  -> instanced quad
  -> vertex shader 投影 3D covariance 到 2D ellipse
  -> fragment shader 计算 Gaussian alpha
  -> back-to-front alpha blend
```

### 与 OSG 的组合

优先做以下 PoC：

1. 将默认上下文改为 OpenGL 4.3 Compatibility Profile。
2. 不加高斯代码，先跑现有 viewer、route、backstage smoke。
3. 在同一 `QOpenGLWidget` 中加入最小三角形/SSBO compute 探针。
4. 通过后再接 `GaussianRenderer`。

这样可以复用 OSG manipulator 和相机。高斯渲染时 OSG 仍负责相机更新，但不把高斯数据构造成 OSG geometry。

如果 OSG 或目标驱动在 4.3 compatibility 下出现问题，再采用独立 `GaussianOpenGLWidget`。不要一开始就同时维护 OSG/OpenGL/Vulkan 三套窗口和输入系统。

### LAS 与高斯模型共存边界

首版建议只做“当前视图显示一种主数据类型”：

- LAS/LAZ 使用现有 OSG viewer
- Gaussian PLY 使用原生 Gaussian renderer
- 项目树可以同时记录两类文件，但首版不要求同一 framebuffer 混合渲染

同时混合 LAS、路线、杆塔、隐患与 Gaussian 的正确深度组合，需要共享深度、坐标系和 overlay 绘制顺序，属于下一阶段。先把 PLY 加载和高斯显示闭环做稳。

## 分阶段落地

### Phase 0：上下文兼容性探针

- OpenGL 4.3 Compatibility Profile
- 检查 `GL_VERSION`、SSBO、compute shader、最大 SSBO 大小
- 跑现有构建和 smoke

退出条件：现有 LAS 显示、点选、量测、路线漫游无回归。

### Phase 1：最小高斯 viewer

- binary little-endian 3DGS PLY
- 单模型加载、清空、适配视图
- SH degree 0/3
- 异步 CPU counting sort
- GPU instanced quad rasterization
- 项目树显示 `.ply`

退出条件：小型 PLY 可稳定加载、旋转、缩放，画面方向和颜色正确。

### Phase 2：全 GPU 排序

- compute shader 生成深度 key和可见索引
- 通用 radix sort，不强制 subgroup 扩展
- indirect draw 或至少避免每帧 CPU readback
- 保留 CPU sorter fallback

退出条件：相机移动期间排序无明显停顿，在 GTX 1050 Ti 上有可接受帧率。

### Phase 3：产品集成

- 工程文件保存 Gaussian 数据集类型、路径和可见状态
- Gaussian 专属显示参数：SH degree、splat scale、opacity cutoff、sort mode
- 翻译和错误提示
- 新增同一 `LASViewerSmokeTest.exe` 下的 gaussian mode

### Phase 4：按需要扩展

- LAS 与 Gaussian 同屏组合
- 业务覆盖层和高斯深度交互
- 多 Gaussian 模型
- Vulkan 后端

没有明确需求前不做训练、编辑、SPZ、`.splat`、LOD/流式加载。

## 验证建议

至少准备两个可提交的小型测试 PLY：

- `gaussian_minimal_sh0.ply`：几十个 splat，验证解析、scale/opacity/rotation
- `gaussian_small_sh3.ply`：包含 45 个 `f_rest_*`，验证球谐排列和颜色

Smoke 建议并入现有可执行文件：

```text
LASViewerSmokeTest --mode gaussian-render --ply <small-model.ply>
```

断言：

- 解析成功且 splat count/bounds 正确
- OpenGL 能力满足或给出明确跳过原因
- framebuffer 非空且非纯背景色
- 相机改变后连续帧有像素变化
- 清空/二次加载不崩溃、不泄漏明显 GPU 资源

性能记录应至少包含：模型 splat 数、SH degree、CPU load time、GPU upload time、sort time、frame time、估算 VRAM。

## 主要风险

1. **OpenGL 上下文升级回归**：当前 OSG shader 是 GLSL 1.20，必须使用 Compatibility Profile 并完整跑 smoke。
2. **排序正确性**：透明 Gaussian 必须按视角排序；仅绘制不排序会产生明显错误。
3. **球谐排列错误**：PLY 的 `f_rest_*` 通常按通道分组，上传布局错误会导致视角颜色异常。
4. **坐标系差异**：不同工具可能使用 RDF/RUB 等约定，首版需要明确默认转换和可测试样例。
5. **显存压力**：完整 SH 数据很大，必须在上传前估算并允许降低 SH degree。
6. **GL 状态污染**：原生 renderer 与 OSG 共用上下文时，要明确保存/恢复 program、VAO、blend、depth、buffer binding 和 framebuffer 状态。
7. **许可证**：不复制 graphdeco 非商业许可证代码；实际代码来源只选 MIT/Apache-2.0，并保留 attribution。

## 最终建议

实施顺序应是：

1. 先验证 OpenGL 4.3 Compatibility Profile 对现有 OSG 的兼容性。
2. 以 `splatview` 的小型结构实现 CPU 异步排序 MVP。
3. 以 `splatapult` 的 compute/radix 思路补全 GPU 排序。
4. 用 NVIDIA Vulkan 项目做结果和性能对照，不直接引入其完整框架。

这条路线能满足“PLY 高斯模型 + GPU 原生渲染”，同时保持当前 Qt/OSG 业务代码基本不动，并给 GTX 1050 Ti 这类 4 GB 显卡保留可运行的降级路径。
