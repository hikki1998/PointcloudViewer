# CRS 系统迁移与工程属性化方案

## Goal
- 评估当前 CRS 设计问题，提出把 CRS 从 `Inspection Route Planning` 中抽离为“工程属性”的可实施方案。
- 方案需覆盖：数据模型、UI 结构、工程文件兼容、常见坐标系支持范围、第三方库依赖与接入建议。

## Phases
| Phase | Status | Description |
|-------|--------|-------------|
| 1 | complete | 建立计划文件并梳理仓库上下文 |
| 2 | complete | 调研当前 CRS、工程属性、Inspection Route Planning 相关代码 |
| 3 | complete | 设计工程级 CRS 数据模型、UI 与交互流 |
| 4 | complete | 设计工程文件迁移、兼容策略和验证方案 |
| 5 | complete | 评估第三方库缺口并输出实施建议 |
| 6 | complete | 实现工程级 CRS 数据模型与 JSON 序列化 |
| 7 | complete | MainWindow 接入工程级 CRS，并兼容旧 routePlanning.crs 字段 |
| 8 | complete | 构建与 smoke test 验证首批改动 |
| 9 | complete | 独立坐标系界面与工程树入口 |
| 10 | complete | 航线导入导出彻底切换到工程级 CRS |

## Constraints
- 方案需要贴合当前 Qt + OSG + 工程文件序列化结构。
- 默认中文输出，优先强调结果、验证状态和下一步。
- 若涉及新增 UI 文本，需要考虑 `translations/lasviewer_zh_CN.ts`。

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| `rg.exe` 在当前环境拒绝访问 | 1 | 改用 PowerShell `Get-ChildItem` + `Select-String` 搜索 |
| `LNK1104` 无法打开 `LASPointCloudViewer.exe` | 1 | 发现主程序仍在运行，结束进程后重跑构建 |
| `QVariant::toObject()` 编译失败 | 1 | 改为 Qt 5.15 可用的 `toJsonObject()` 读取 CRS 选择器里的 JSON 数据 |
