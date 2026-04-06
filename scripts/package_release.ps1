param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$Config = "Release",

    [string]$BuildBinDir = "out/build/bin/Release",

    [string]$OutputDir = "out/release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-PathExists {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path"
    }
}

function Ensure-CleanDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    New-Item -ItemType Directory -Path $Path | Out-Null
}

function Copy-IfExists {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildBinDir = (Resolve-Path -LiteralPath (Join-Path $repoRoot $BuildBinDir)).Path
$resolvedOutputDir = Join-Path $repoRoot $OutputDir

Assert-PathExists -Path $resolvedBuildBinDir -Description "Build output directory"
Assert-PathExists -Path (Join-Path $resolvedBuildBinDir "LASPointCloudViewer.exe") -Description "Main executable"

$normalizedVersion = if ($Version.StartsWith("v")) { $Version } else { "v$Version" }
$packageName = "LASPointCloudViewer-$normalizedVersion-windows-x64"
$stagingDir = Join-Path $resolvedOutputDir $packageName
$zipPath = Join-Path $resolvedOutputDir "$packageName.zip"

if (-not (Test-Path -LiteralPath $resolvedOutputDir)) {
    New-Item -ItemType Directory -Path $resolvedOutputDir | Out-Null
}

Ensure-CleanDirectory -Path $stagingDir
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

$rootFiles = @(
    "LASPointCloudViewer.exe",
    "OpenThreads.dll",
    "osg.dll",
    "osgDB.dll",
    "osgGA.dll",
    "osgText.dll",
    "osgUtil.dll",
    "osgViewer.dll",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5OpenGL.dll",
    "Qt5Svg.dll",
    "Qt5Widgets.dll",
    "Qt5Xml.dll",
    "qtnribbon4.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "opengl32sw.dll",
    "zlib.dll",
    "proj_9.dll",
    "sqlite3.dll",
    "tiff.dll",
    "zstd.dll",
    "libcurl.dll",
    "libcrypto-3-x64.dll",
    "libssl-3-x64.dll"
)

foreach ($fileName in $rootFiles) {
    Copy-IfExists -Source (Join-Path $resolvedBuildBinDir $fileName) -Destination $stagingDir
}

$runtimeDirectories = @(
    "platforms",
    "styles",
    "imageformats",
    "iconengines",
    "translations",
    "proj9"
)

foreach ($directoryName in $runtimeDirectories) {
    Copy-IfExists -Source (Join-Path $resolvedBuildBinDir $directoryName) -Destination (Join-Path $stagingDir $directoryName)
}

$packagedExecutable = Join-Path $stagingDir "LASPointCloudViewer.exe"
Assert-PathExists -Path $packagedExecutable -Description "Packaged main executable"

Compress-Archive -LiteralPath $stagingDir -DestinationPath $zipPath -CompressionLevel Optimal

Write-Output "Packaging completed."
Write-Output "Config: $Config"
Write-Output "Staging directory: $stagingDir"
Write-Output "Zip package: $zipPath"
