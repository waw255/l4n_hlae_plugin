param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build-hlae-native"),
    [string]$StageDir = (Join-Path $PSScriptRoot "..\stage\native"),
    [string]$HlaeRoot = "E:\SteamLibrary\steamapps\common\Left 4 Dead 2\l4d2_dev\hlae",
    [switch]$Deploy,
    [string]$DeployDir = "E:\SteamLibrary\steamapps\common\Left 4 Dead 2\bin\neko\plugins"
)

$ErrorActionPreference = 'Stop'

$buildRoot = (Resolve-Path -LiteralPath $BuildDir).Path
$stageRoot = [IO.Path]::GetFullPath($StageDir)
$plugin = Join-Path $buildRoot 'advancedfx-build\AfxHookSource\Release\l4n_hlae_plugin.dll'
$configTemplate = Join-Path $PSScriptRoot '..\config\l4n_hlae_plugin.ini.example'

if (-not (Test-Path -LiteralPath $plugin -PathType Leaf)) {
    throw "Built native plugin not found: $plugin"
}
if (-not (Test-Path -LiteralPath $HlaeRoot -PathType Container)) {
    throw "HLAE resource root not found: $HlaeRoot"
}

New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
Copy-Item -LiteralPath $plugin -Destination (Join-Path $stageRoot 'l4n_hlae_plugin.dll') -Force

$config = Get-Content -LiteralPath $configTemplate -Raw
$config = [Text.RegularExpressions.Regex]::Replace(
    $config,
    '(?m)^HlaeRoot=.*$',
    "HlaeRoot=$HlaeRoot")
[IO.File]::WriteAllText((Join-Path $stageRoot 'l4n_hlae_plugin.ini'), $config,
    [Text.UTF8Encoding]::new($false))

if ($Deploy) {
    $deployRoot = (Resolve-Path -LiteralPath $DeployDir).Path
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $oldPlugin = Join-Path $deployRoot 'l4n_hlae_plugin.dll'
    $oldConfig = Join-Path $deployRoot 'l4n_hlae_plugin.ini'

    foreach ($existing in @($oldPlugin, $oldConfig)) {
        if (Test-Path -LiteralPath $existing -PathType Leaf) {
            Copy-Item -LiteralPath $existing -Destination "$existing.before-native-$stamp" -Force
        }
    }

    Get-ChildItem -LiteralPath $stageRoot -File |
        Where-Object Name -in @('l4n_hlae_plugin.dll', 'l4n_hlae_plugin.ini') |
        ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $deployRoot $_.Name) -Force
    }

    Write-Host "Deployed native HLAE plugin to $deployRoot" -ForegroundColor Green
}

Write-Host "Staged native HLAE plugin in $stageRoot" -ForegroundColor Green
Get-ChildItem -LiteralPath $stageRoot -File |
    Where-Object Name -in @('l4n_hlae_plugin.dll', 'l4n_hlae_plugin.ini') |
    Select-Object Name, Length
Write-Host 'OpenEXR is expected to be statically bundled in the plugin.'
