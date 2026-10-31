# Sync the RCIF workbench plugin from the repository into a DSH profile.
# Usage:
#   .\scripts\sync_rcif_plugin.ps1            # pack + install + verify
#   .\scripts\sync_rcif_plugin.ps1 -Check     # verify only (no writes)
param(
  [string]$ProfileDir = "$env:USERPROFILE\.dsh\profiles\rcif-studio\node_modules\@rcif\dsh-studio-plugin",
  [switch]$Check
)

$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\dsh-rcif')
$files = @(
  'dist\index.js',
  'dist\client.js',
  'dist\lib\ingest.js',
  'dist\lib\package.js',
  'workbench\index.html',
  'workbench\workbench.js',
  'workbench\workbench.css',
  'package.json'
)

function Get-Sha([string]$Path) {
  (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

if (-not $Check) {
  $cache = Join-Path $repo '.npm-cache'
  $tmp = Join-Path $repo '.tmp'
  New-Item -ItemType Directory -Force $cache, $tmp | Out-Null
  $oldTemp = $env:TEMP
  $oldTmp = $env:TMP
  $env:TEMP = $tmp
  $env:TMP = $tmp
  $stage = Join-Path $repo '.pkg-stage'
  try {
    Push-Location $repo
    & npm pack --cache $cache --pack-destination . | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'npm pack failed' }
    $tgz = Join-Path $repo 'rcif-dsh-studio-plugin-1.0.0.tgz'
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    New-Item -ItemType Directory $stage | Out-Null
    & tar -xzf $tgz -C $stage
    if ($LASTEXITCODE -ne 0) { throw 'tar extraction failed' }
    $backup = "$ProfileDir.bak-" + (Get-Date -Format 'yyyyMMdd-HHmmss')
    Copy-Item -LiteralPath $ProfileDir -Destination $backup -Recurse -Force
    Get-ChildItem -LiteralPath $ProfileDir -Force | Remove-Item -Recurse -Force
    Copy-Item -Path (Join-Path $stage 'package\*') -Destination $ProfileDir -Recurse -Force
    Write-Host "installed -> $ProfileDir"
    Write-Host "backup    -> $backup"
  }
  finally {
    Pop-Location
    $env:TEMP = $oldTemp
    $env:TMP = $oldTmp
    Remove-Item $stage, $cache, $tmp -Recurse -Force -ErrorAction SilentlyContinue
  }
}

$mismatch = 0
foreach ($rel in $files) {
  $src = Join-Path $repo $rel
  $dst = Join-Path $ProfileDir $rel
  if (-not (Test-Path -LiteralPath $dst)) {
    Write-Host "MISSING  $rel"
    $mismatch++
    continue
  }
  if ((Get-Sha $src) -eq (Get-Sha $dst)) {
    Write-Host "OK       $rel"
  }
  else {
    Write-Host "DIFF     $rel"
    $mismatch++
  }
}

if ($mismatch -gt 0) {
  Write-Host "$mismatch file(s) differ; run without -Check to sync."
  exit 1
}
Write-Host 'plugin copy matches the repository.'
