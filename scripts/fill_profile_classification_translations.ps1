$path = Resolve-Path 'translations/lasviewer_zh_CN.ts'
$raw = [System.IO.File]::ReadAllText($path)
$raw = $raw -replace '<!DOCTYPE TS\[\]>', '<!DOCTYPE TS>'
[xml]$ts = $raw

$map = @{
    'Profile Classify' = '剖面分类'
    'Drag a rectangle to reclassify points. Hold Alt and drag left mouse to adjust view while the tool is active' = '拖拽矩形框重分类点云。工具启用时按住 Alt 并拖动鼠标左键可调整视角'
    'Classify Panel' = '分类面板'
    'Save Classify Result' = '保存分类结果'
    'Undo Classify' = '撤销分类'
    'Redo Classify' = '重做分类'
    'Clear Classify Edits' = '清空分类编辑'
    '3D Profile Classification' = '三维剖面分类'
    'Start Tool' = '启动工具'
    'Select All' = '全选'
    'Clear Sources' = '清空来源类别'
    'Undo' = '撤销'
    'Redo' = '重做'
    'Clear Edits' = '清空编辑'
    'Save Result' = '保存结果'
    'Source Classes' = '来源类别'
    'Target Class' = '目标类别'
    'Profile Classification' = '剖面分类'
    'Show or hide the profile classification dock' = '显示或隐藏剖面分类面板'
    'Write current profile classification edits back to LAS files' = '将当前剖面分类编辑写回 LAS 文件'
    'Exit Tool' = '退出工具'
    'Load a point cloud and switch to a stable scene before using profile classification.' = '使用剖面分类前，请先加载点云并确保场景稳定。'
    'Profile classification is processing the current rectangular selection.' = '正在处理当前矩形选择的剖面分类。'
    'Source classes %1 | Target class %2 | Edited points %3 | Save state %4' = '来源类别 %1 | 目标类别 %2 | 已编辑点数 %3 | 保存状态 %4'
    'unsaved' = '未保存'
    'saved' = '已保存'
    'This build does not support writing LAS/LAZ files.' = '当前构建不支持写入 LAS/LAZ 文件。'
    'Save Classification Results' = '保存分类结果'
    'Saving classification results to LAS files...' = '正在将分类结果保存到 LAS 文件...'
    'Preparing LAS write tasks...' = '正在准备 LAS 写入任务...'
    'Failed to save classification result: dataset file not found (%1).' = '保存分类结果失败：未找到数据集文件（%1）。'
    'Failed to open dataset for write-back (%1).' = '打开数据集进行回写失败（%1）。'
    'Failed to create output LAS file (%1).' = '创建输出 LAS 文件失败（%1）。'
    'Failed while writing classification result (%1).' = '写入分类结果时失败（%1）。'
    'Writing %1 (%2/%3 points)' = '正在写入 %1（%2/%3 点）'
    'Failed to replace dataset while saving (%1).' = '保存时替换数据集失败（%1）。'
    'Failed to finalize LAS save (%1).' = '完成 LAS 保存失败（%1）。'
    'Saved %1 (%2/%3 files)' = '已保存 %1（%2/%3 个文件）'
    'Classification results were written to %1 LAS file(s).' = '分类结果已写入 %1 个 LAS 文件。'
    "%1`n`nSaved files: %2" = "%1`n`n已保存文件：%2"
    'Profile classification results are not saved. Write them to LAS files now?' = '剖面分类结果尚未保存。是否现在写入 LAS 文件？'
    'Wait until the current point cloud is fully ready before starting profile classification.' = '请等待当前点云完全就绪后再启动剖面分类。'
    'Profile classification mode enabled. Drag a rectangle to classify source classes, hold Alt and drag left mouse to adjust view, right-click to exit, and press Esc to cancel.' = '已启用剖面分类模式。拖拽矩形框可对来源类别进行分类，按住 Alt 并拖动鼠标左键可调整视角，右键退出，按 Esc 取消。'
    'Profile classification mode disabled.' = '已关闭剖面分类模式。'
    'Reverted %1 profile classification point(s).' = '已撤销 %1 个剖面分类点。'
    'Reapplied %1 profile classification point(s).' = '已重做 %1 个剖面分类点。'
    'Cleared all project profile classification edits.' = '已清空工程内全部剖面分类编辑。'
    ' | Profile classify: processing' = ' | 剖面分类：处理中'
    ' | Profile classify: source %1 -> target %2 | edits %3' = ' | 剖面分类：来源 %1 -> 目标 %2 | 编辑 %3'
    'Profile classification selection cancelled.' = '已取消剖面分类选择。'
    'Choose at least one source classification before profile classification.' = '进行剖面分类前请至少选择一个来源类别。'
    'No visible datasets are available for profile classification.' = '当前没有可用于剖面分类的可见数据集。'
    'Applying profile classification selection...' = '正在应用剖面分类选择...'
    'Profile classification completed. %1 point(s) hit, %2 point(s) changed to class %3.' = '剖面分类完成。命中 %1 个点，%2 个点已改为类别 %3。'
}

foreach ($context in $ts.TS.context) {
    foreach ($message in $context.message) {
        $sourceText = [string]$message.source
        if ($map.ContainsKey($sourceText)) {
            $message.translation = $map[$sourceText]
        }
    }
}

$settings = New-Object System.Xml.XmlWriterSettings
$settings.Indent = $true
$settings.IndentChars = '    '
$settings.Encoding = New-Object System.Text.UTF8Encoding($false)
$writer = [System.Xml.XmlWriter]::Create($path, $settings)
$ts.Save($writer)
$writer.Close()
