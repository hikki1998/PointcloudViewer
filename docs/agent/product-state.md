# Product State

本文档描述当前代码已经具备的用户可见能力，避免 agent 在旧假设上工作。

## 当前核心能力

- 多个 `.las/.laz` 数据加载与项目树管理
- 视角预设、场景适配、悬停坐标显示
- RGB / 高程 / 单色 / 分类显示
- `Point Size`、`Point Opacity`、`Depth Cue`、`EDL-style Shading`、`Round splats`
- 量测、连续点选、右键回退、量测覆盖层
- 净空分析、分段明细、CSV 导出、剖面视图
- 杆塔编辑、属性维护
- 隐患台账、列表管理、导出
- 工程文件保存/加载

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
