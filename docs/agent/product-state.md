# Product State

本文档描述当前代码已经具备的用户可见能力，避免 agent 在旧假设上工作。

## 当前核心能力

- 多个 `.las/.laz` 数据加载与项目树管理；单文件、多文件和追加导入均支持后台加载与进度/取消，大文件单文件打开时先显示 preview，多数据集使用独立渲染节点
- 单个 3D Gaussian Splatting `.ply` 加载与 OpenGL 4.3 GPU 渲染
- Gaussian 大文件工作线程解析、交互期降级排序/渲染、视图适配与自由轨迹球交互
- 当前 Gaussian 与 LAS/LAZ 不支持混载或同屏显示
- 多数据集渲染不再常驻合并点云副本；需要连续全量数组的分析功能会按需构建兼容缓存
- 完整数据集建立轻量 XY 网格索引用于缩小点选/悬停候选范围；preview 阶段禁用依赖完整点云的分析/分类操作
- 大数据集交互时使用有上限的抽样节点，相机静止后自动恢复完整节点
- 视角预设、场景适配、悬停坐标显示
- RGB / 高程 / 单色 / 分类显示
- `Point Size`、`Point Opacity`、`Depth Cue`、`EDL-style Shading`、`Round splats`
- 量测、连续点选、右键回退、量测覆盖层
- 净空分析、分段明细、CSV 导出、剖面视图
- 杆塔编辑、属性维护
- 隐患台账、列表管理、导出
- 工程文件保存/加载
- 空场景“最近工程 / 最近数据”工作台
  - 最近工程业务摘要
  - 最近数据文件类型与大小
  - 被动场景缩略图及类型占位图
  - 缺失文件状态
  - 打开、追加、定位文件夹、移除最近记录
  - 缩略图启动时不重读点云，缓存有容量上限

## Gaussian 模块当前状态

### 已具备
- binary little-endian、float32 3DGS PLY 校验与读取
- 必需属性：`x/y/z`、`f_dc_0..2`、`opacity`、`scale_0..2`、`rot_0..3`
- SH0 颜色、透明度、缩放、旋转和三维协方差
- OpenGL 4.3 SSBO + instanced quad GPU rasterization
- 大型 PLY 后台解析和并行转换/排序
- Ribbon、拖放、通用打开、项目树和工程恢复入口
- `viewer-render` smoke 可把 `--las` 参数指向受支持 `.ply` 验证

### 当前边界
- 不支持普通任意 PLY、ASCII PLY 或缺少 3DGS 字段的 PLY
- 不支持 SH1-SH3 渲染
- 不支持多 Gaussian 模型
- 不支持 Gaussian 与 LAS/LAZ、杆塔/隐患/航线 overlay 的同屏深度组合

## 最近工作台当前状态

- 空场景工作台取代静态启动海报
- 最近工程沿用 `project/recentProjects`；最近数据使用 `project/recentDataFiles`
- 缩略图由正常渲染后的 framebuffer 被动捕获，位于 `AppLocalDataLocation/thumbnails`
- 缓存按路径、文件大小和修改时间失效，使用原子写入，最多 200 张 / 100MB
- 单数据场景保存数据缩略图；工程保存完整场景缩略图，避免多数据场景误写给单文件
- 工作台读取最近工程摘要时对异常大 JSON 设有限制，不在启动时读取 LAS/LAZ/PLY

## 航线模块当前状态

### 已具备
- 标准 route JSON 导入、保存、重载
- KML 导入、KML / DJI KMZ 导出
- 航线与工程文件外部关联
- 右侧 route dock：
  - 航点表
  - 部件点表
  - 目标列表
  - Route QA 问题表
- 主视图航线显示：
  - 航点折线
  - 部件点
  - 航点与部件点连线
  - 视锥体
  - 右下角相机预览
- 航线编辑：
  - 双击定位
  - 右键删除
  - 航点对话框编辑
  - 场景内拖拽航点
  - `Esc` 取消拖拽
- 漫游与预览：
  - 预览中目标偏离提示
  - 航线漫游控制
  - 第一人称 / 第三人称漫游视角
- 航线质量检查：
  - Route QA 模型
  - 阻断/警告/提示级问题汇总

### 仍然是演进中的部分
- 多目标航点编辑仍未完全成为一等体验
- 按部件自动生成 / 局部重生成仍然偏弱
- 覆盖完整性视图还不完整
- 计划航线与实飞结果闭环尚未建立

## 近期文档入口

- 航线路线图：
  - `planning/ROUTE_MODULE_ROADMAP.md`
- 标准航线 JSON / IO 方案：
  - `planning/PLAN.md`
- Ribbon 重组设计：
  - `docs/superpowers/specs/2026-04-05-ribbon-page-restructure-design.md`

## 使用本文档的方式

- 要判断“功能已经有了吗”，先看这里。
- 要判断“应该改哪层”，再去看 `architecture.md`。
- 要判断“怎么验证改动没回归”，再去看 `workflows.md`。
