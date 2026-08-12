param(
    [string]$OutputDir = (Join-Path $PSScriptRoot "..\.deps"),
    [switch]$Force
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$ProgressPreference = "SilentlyContinue"

function Test-Pattern([string]$path, $entries) {
    foreach ($e in $entries) {
        if ($e.Prefix -and $path.StartsWith($e.Prefix, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
        if ($e.Wildcard -and $path -like $e.Wildcard) { return $true }
    }
    return $false
}

function Download-GitHubFiles([string]$owner, [string]$repo, [string]$tag, $entries, [string]$dest) {
    $headers = @{ "User-Agent" = "htb-fetch-deps" }
    $tree = Invoke-RestMethod -Uri "https://api.github.com/repos/$owner/$repo/git/trees/$tag`?recursive=1" -Headers $headers -TimeoutSec 30
    $files = @($tree.tree | Where-Object { $_.type -eq "blob" -and (Test-Pattern $_.path $entries) })
    Write-Host "  $($files.Count) files"
    $count = 0
    foreach ($f in $files) {
        $out = Join-Path $dest ($f.path -replace '/', '\')
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $out) | Out-Null
        $url = "https://raw.githubusercontent.com/$owner/$repo/$tag/$($f.path)"
        for ($attempt = 1; $attempt -le 3; $attempt++) {
            try {
                Invoke-WebRequest -Uri $url -OutFile $out -TimeoutSec 90 -UseBasicParsing
                break
            } catch {
                if ($attempt -eq 3) { throw "Failed to download $url : $($_.Exception.Message)" }
                Start-Sleep -Seconds 2
            }
        }
        $count++
    }
    return $count
}

function Fetch-Dep([string]$name, [scriptblock]$action) {
    $target = Join-Path $OutputDir $name
    if (-not $Force -and (Test-Path (Join-Path $target ".complete"))) {
        Write-Host "  skipped (already fetched)"
        return
    }
    Write-Host "Fetching $name"
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    & $action $target
    Set-Content -Path (Join-Path $target ".complete") -Value "fetched by tools/fetch_deps.ps1"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$imguiEntries = @(
    @{ Wildcard = "imgui*" },
    @{ Wildcard = "imconfig.h" },
    @{ Wildcard = "imstb*" },
    @{ Wildcard = "LICENSE.txt" },
    @{ Wildcard = "backends/imgui_impl_win32*" },
    @{ Wildcard = "backends/imgui_impl_dx11*" }
)

$spdlogEntries = @(
    @{ Prefix = "include/spdlog/" },
    @{ Wildcard = "src/*.cpp" },
    @{ Wildcard = "LICENSE" }
)

$tomlppEntries = @(
    @{ Prefix = "include/toml++/" },
    @{ Wildcard = "LICENSE.md" }
)

Fetch-Dep "imgui" { param($dest) Download-GitHubFiles "ocornut" "imgui" "v1.91.8" $imguiEntries $dest | ForEach-Object { Write-Host "  downloaded $_ files" } }
Fetch-Dep "spdlog" { param($dest) Download-GitHubFiles "gabime" "spdlog" "v1.15.3" $spdlogEntries $dest | ForEach-Object { Write-Host "  downloaded $_ files" } }
Fetch-Dep "tomlplusplus" { param($dest) Download-GitHubFiles "marzer" "tomlplusplus" "v3.4.0" $tomlppEntries $dest | ForEach-Object { Write-Host "  downloaded $_ files" } }

if (-not $Force -and (Test-Path (Join-Path $OutputDir "googletest\.complete"))) {
    Write-Host "Fetching googletest: skipped (already fetched)"
} else {
    Write-Host "Fetching googletest"
    $dest = Join-Path $OutputDir "googletest"
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    git clone --depth 1 --branch v1.15.2 https://gitee.com/mirrors/googletest.git $dest 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "gitee clone failed; try mirror manually" }
    Set-Content -Path (Join-Path $dest ".complete") -Value "fetched by tools/fetch_deps.ps1"
}

Write-Host "All dependencies ready in $OutputDir"
