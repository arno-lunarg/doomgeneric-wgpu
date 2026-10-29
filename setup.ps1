# Fetches third-party dependencies (idempotent):
#   third_party/dawn  - Dawn, Chrome's WebGPU implementation: prebuilt static lib, headers, tint.exe
#   wad/freedoom1.wad - Freedoom Phase 1, a free Doom IWAD (drop doom1.wad / doom.wad next to it to use those instead)
#
# Usage: pwsh -File setup.ps1
#    or: powershell -ExecutionPolicy Bypass -File setup.ps1

$ErrorActionPreference = 'Stop'

# Pinned versions. Dawn's C API still moves; the tutorial matches this exact header.
$DawnTag         = 'v20261027.184652'
$DawnCommit      = '106704a9ac3fc21b1580ea85aea302ac17d416f1'
$FreedoomVersion = '0.13.0'

$Root       = $PSScriptRoot
$ThirdParty = Join-Path $Root 'third_party'
$DawnDir    = Join-Path $ThirdParty 'dawn'
$WadDir     = Join-Path $Root 'wad'
$Downloads  = Join-Path $ThirdParty '_downloads'

New-Item -ItemType Directory -Force $ThirdParty, $WadDir, $Downloads | Out-Null

function Get-File($Url, $Out) {
    Write-Host "Downloading $Url"
    curl.exe -L --fail --progress-bar -o "$Out.part" $Url
    if ($LASTEXITCODE -ne 0) { throw "Download failed: $Url" }
    Move-Item -Force "$Out.part" $Out
}

# Dawn: Windows x64 Release build from google/dawn GitHub releases.
# The VERSION stamp makes re-runs no-ops and bumps re-download.
$Stamp = Join-Path $DawnDir 'VERSION'
if ((Test-Path $Stamp) -and ((Get-Content $Stamp) -eq $DawnTag)) {
    Write-Host "Dawn $DawnTag already installed."
} else {
    $Name    = "Dawn-$DawnCommit-windows-latest-Release"
    $Archive = Join-Path $Downloads "$Name.tar.gz"
    Get-File "https://github.com/google/dawn/releases/download/$DawnTag/$Name.tar.gz" $Archive

    Write-Host 'Extracting Dawn...'
    if (Test-Path $DawnDir) { Remove-Item -Recurse -Force $DawnDir }
    tar.exe -xzf $Archive -C $ThirdParty
    if ($LASTEXITCODE -ne 0) { throw 'Dawn extraction failed' }
    Move-Item (Join-Path $ThirdParty $Name) $DawnDir
    Set-Content $Stamp $DawnTag
}

# DLLs Dawn loads at runtime. It only finds them in the exe folder (its system-path fallback
# fails with Windows error 87), so props/Dawn.props copies third_party/dawn/runtime next to each exe.
#   dxcompiler.dll, dxil.dll : DXC, D3D12 shader compiler (Windows SDK)
#   d3dcompiler_47.dll       : FXC, D3D11 shader compiler (Windows SDK)
#   vulkan-1.dll             : Vulkan loader (System32), for -vulkan
$Runtime = Join-Path $DawnDir 'runtime'
New-Item -ItemType Directory -Force $Runtime | Out-Null

$Dxc = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.*\x64\dxcompiler.dll" -ErrorAction SilentlyContinue |
       Sort-Object { [version]$_.Directory.Parent.Name } | Select-Object -Last 1
if (-not $Dxc) { throw 'dxcompiler.dll not found in any Windows SDK. Install a Windows 11 SDK from the VS installer.' }
foreach ($Dll in 'dxcompiler.dll', 'dxil.dll', 'd3dcompiler_47.dll') {
    Copy-Item -Force (Join-Path $Dxc.DirectoryName $Dll) $Runtime
}

$VulkanLoader = Join-Path $env:SystemRoot 'System32\vulkan-1.dll'
if (Test-Path $VulkanLoader) { Copy-Item -Force $VulkanLoader $Runtime }
else { Write-Warning 'No Vulkan runtime installed: the -vulkan backend will not work.' }

# Freedoom: only needed to run the Doom project.
$Wad = Join-Path $WadDir 'freedoom1.wad'
if (Test-Path $Wad) {
    Write-Host 'Freedoom already installed.'
} else {
    $Zip = Join-Path $Downloads "freedoom-$FreedoomVersion.zip"
    Get-File "https://github.com/freedoom/freedoom/releases/download/v$FreedoomVersion/freedoom-$FreedoomVersion.zip" $Zip

    $Unzipped = Join-Path $Downloads 'freedoom'
    Expand-Archive -Force $Zip $Unzipped
    Copy-Item (Join-Path $Unzipped "freedoom-$FreedoomVersion\freedoom1.wad") $Wad
}

Remove-Item -Recurse -Force $Downloads
Write-Host 'Done. Open doomgeneric-wgpu.sln in Visual Studio 2022.'
