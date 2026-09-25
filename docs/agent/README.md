# Agent Onboarding

本目录只保留面向 agent 的当前项目事实与工作指引，目标是让新 agent 在 2-5 分钟内找到正确入口，而不是保存历史过程、会话快照或一次性调研。

## 推荐阅读顺序

1. 根目录 `AGENTS.md`
2. 根目录 `CLAUDE.md`（如果存在且使用 Claude Code）
3. `context.md`
4. 按任务继续阅读：
   - 模块边界与核心链路：`architecture.md`
   - 当前产品能力与限制：`product-state.md`
   - 改动路径、构建和验证：`workflows.md`

## 按任务快速跳转

| 任务 | 先读 |
|---|---|
| UI / Ribbon / dock / 交互 | `architecture.md` + `workflows.md` |
| MainWindow 重构 / 回归防复发 | `architecture.md` + `workflows.md` |
| OSG 渲染 / 显示参数 / 点云表现 | `architecture.md` + `workflows.md` |
| Gaussian PLY / GPU splat 渲染 | `product-state.md` + `architecture.md` + `workflows.md` |
| 最近工程 / 最近数据 / 缩略图 | `architecture.md` + `workflows.md` |
| 航线编辑 / 巡检业务 / 导出 | `product-state.md` + `planning/ROUTE_MODULE_ROADMAP.md` |
| 构建 / 依赖 / smoke test / 发布 | `workflows.md` |
| 只想快速知道项目是什么 | 根目录 `README.md` |

## 文件定位

- `context.md`
  - 5 分钟上下文、关键入口、当前能力和验证基线。
- `architecture.md`
  - 当前模块边界、核心运行链路和代码职责。
- `product-state.md`
  - 当前已经具备的用户可见能力及明确限制。
- `workflows.md`
  - 常见改动路径、构建、smoke、翻译、发布和提交规则。

历史过程、版本发布、专题 smoke 和平台迁移资料分别放在：

- `docs/history/`
- `docs/releases/`
- `docs/smoketest/`
- `docs/linux-*.md`
- `planning/`

## 维护原则

- 功能状态变化：更新 `product-state.md`。
- 模块边界或核心数据流变化：更新 `architecture.md`。
- 常改路径、验证、翻译、发布流程变化：更新 `workflows.md`。
- 关键入口、首读信息或验证基线变化：更新 `context.md`。
- 历史复盘、一次性调研和会话交接记录不要放回 `docs/agent/`。
