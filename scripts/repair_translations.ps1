$ErrorActionPreference = 'Stop'

$currentPath = Resolve-Path 'translations/lasviewer_zh_CN.ts'
$baselineTmpPath = Join-Path $env:TEMP 'lasviewer_zh_CN.baseline.ts'

$baselineRaw = git show HEAD:translations/lasviewer_zh_CN.ts
if ([string]::IsNullOrWhiteSpace($baselineRaw)) {
    throw 'Failed to load baseline translation file from HEAD.'
}
[System.IO.File]::WriteAllText($baselineTmpPath, $baselineRaw, (New-Object System.Text.UTF8Encoding($false)))

$currentRaw = [System.IO.File]::ReadAllText($currentPath)
$currentRaw = $currentRaw -replace '<!DOCTYPE TS\s*\[\]>', '<!DOCTYPE TS>'
[xml]$currentXml = $currentRaw
[xml]$baselineXml = [System.IO.File]::ReadAllText($baselineTmpPath)

$baselineByContextAndSource = @{}
$baselineBySource = @{}
foreach ($context in $baselineXml.TS.context) {
    $contextName = [string]$context.name
    foreach ($message in $context.message) {
        $source = [string]$message.source
        if ([string]::IsNullOrWhiteSpace($source)) {
            continue
        }

        $translationText = ''
        if ($message.translation -ne $null) {
            $translationText = [string]$message.translation.'#text'
        }
        if ([string]::IsNullOrWhiteSpace($translationText)) {
            continue
        }

        $key = "$contextName||$source"
        $baselineByContextAndSource[$key] = $translationText

        if (-not $baselineBySource.ContainsKey($source)) {
            $baselineBySource[$source] = New-Object System.Collections.Generic.HashSet[string]
        }
        [void]$baselineBySource[$source].Add($translationText)
    }
}

$filledFromBaseline = 0
$stillEmpty = 0

foreach ($context in $currentXml.TS.context) {
    $contextName = [string]$context.name
    foreach ($message in $context.message) {
        $source = [string]$message.source
        if ([string]::IsNullOrWhiteSpace($source) -or $message.translation -eq $null) {
            continue
        }

        $currentTranslation = [string]$message.translation.'#text'
        if (-not [string]::IsNullOrWhiteSpace($currentTranslation)) {
            continue
        }

        $replacement = $null
        $key = "$contextName||$source"
        if ($baselineByContextAndSource.ContainsKey($key)) {
            $replacement = $baselineByContextAndSource[$key]
            $filledFromBaseline++
        } elseif ($baselineBySource.ContainsKey($source) -and $baselineBySource[$source].Count -eq 1) {
            $replacement = ($baselineBySource[$source] | Select-Object -First 1)
            $filledFromBaseline++
        }

        if ($null -ne $replacement -and -not [string]::IsNullOrWhiteSpace([string]$replacement)) {
            $message.translation.RemoveAllAttributes()
            $message.translation.InnerText = [string]$replacement
        }
    }
}

foreach ($context in $currentXml.TS.context) {
    foreach ($message in $context.message) {
        if ($message.translation -eq $null) { continue }
        $t = [string]$message.translation.'#text'
        if ([string]::IsNullOrWhiteSpace($t)) { $stillEmpty++ }
    }
}

$settings = New-Object System.Xml.XmlWriterSettings
$settings.Indent = $true
$settings.IndentChars = '    '
$settings.Encoding = New-Object System.Text.UTF8Encoding($false)
$writer = [System.Xml.XmlWriter]::Create($currentPath, $settings)
$currentXml.Save($writer)
$writer.Close()

Write-Output "filled_from_baseline=$filledFromBaseline"
Write-Output "still_empty=$stillEmpty"
