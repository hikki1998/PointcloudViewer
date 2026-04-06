# Agent Onboarding

本目录提供面向 agent 的渐进式披露文档。

目标只有两个：
- 让新 agent 在 2-5 分钟内找到正确入口，而不是先读完整个仓库。
- 让后续维护尽量只改一处，避免 `README`、`PROJECT_CONTEXT`、`AGENTS`、`CLAUDE` 长期漂移。

## 推荐阅读顺序

### 第 1 层：先建立最小上下文
1. 根目录 `AGENTS.md`
2. 根目录 `CLAUDE.md`（如果你使用 Claude Code）
3. `context.md`

### 第 2 层：按任务选专题
- 改架构、定位模块边界：
  - `architecture.md`
- 想知道当前产品已经做到哪里：
  - `product-state.md`
- 准备动手改代码、跑验证、更新翻译或发布：
  - `workflows.md`

### 第 3 层：按专题深挖
- 航线模块规划与后续优化：
  - `planning/ROUTE_MODULE_ROADMAP.md`
  - `planning/PLAN.md`
- 近期设计/实现过程：
  - `docs/superpowers/specs/`
  - `docs/superpowers/plans/`
- 发布说明：
  - `docs/releases/`

## 按任务快速跳转

| 任务 | 先读 |
|---|---|
| UI / Ribbon / dock / 交互 | `architecture.md` + `workflows.md` |
| OSG 渲染 / 显示参数 / 点云表现 | `architecture.md` + `workflows.md` |
| 航线编辑 / 巡检业务 / 导出 | `product-state.md` + `planning/ROUTE_MODULE_ROADMAP.md` |
| 构建 / 依赖 / smoke test / 发布 | `workflows.md` |
| 只想快速知道项目是什么 | 根目录 `README.md` |

## 文档定位

- 根目录 `README.md`
  - 面向人和 agent 的总入口，保留项目概览、构建运行、文档导航。
- 根目录 `AGENTS.md`
  - Codex 自动读取的仓库级规则入口。
- 根目录 `CLAUDE.md`
  - Claude Code 自动读取的兼容入口，内容尽量薄。
- `docs/agent/context.md`
  - 5 分钟内建立仓库上下文。
- `docs/history/codex-collaboration-retrospective.md`
  - 项目演进复盘，不作为日常 onboarding 主入口。

## 维护原则

- 新增功能时，优先更新 `product-state.md`。
- 新增常改路径、验证方法、翻译/发布流程时，优先更新 `workflows.md`。
- 模块边界、核心数据流变化时，优先更新 `architecture.md`。
- 只有当首读顺序变化时，才更新本文件和根目录 `AGENTS.md` / `CLAUDE.md`。
