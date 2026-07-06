<#
.SYNOPSIS
    Builds and packages the XML Points OpenCPN plugin on Windows (MSVC + vcpkg).

.DESCRIPTION
    Windows counterpart to build_xmlpoints_pi.sh. Installs/locates build
    dependencies via vcpkg, configures the project with CMake + MSVC, builds
    a Release Win32 (x86) xmlpoints_pi.dll - matching the prebuilt opencpn.lib
    import library available for this plugin API version - and packages it
    into a .zip.

.PARAMETER Install
    Copy the built DLL into this user's OpenCPN plugin directory after build.

.PARAMETER Clean
    Remove the build/ and package/ directories (and the .zip) before building.

.PARAMETER VcpkgRoot
    Path to an existing vcpkg checkout. If omitted, the script looks for
    $env:VCPKG_ROOT, then C:\vcpkg, then clones a fresh one there.

.PARAMETER Generator
    CMake generator to use. Defaults to "Visual Studio 17 2022". Pass e.g.
    "Visual Studio 16 2019" if that's what you have installed instead.

.NOTES
    IMPORTANT: this script has not been run end-to-end on a real Windows
    machine (it was written/reviewed from a Linux session). Treat the first
    run as a shakedown: read the step output, and if vcpkg's wxWidgets
    feature names or your OpenCPN install layout differ from what's assumed
    below, adjust the marked spots rather than assuming the script is wrong
    wholesale.

.EXAMPLE
    .\build_xmlpoints_pi.ps1
    .\build_xmlpoints_pi.ps1 -Install
    .\build_xmlpoints_pi.ps1 -Clean -Install
#>

[CmdletBinding()]
param(
    [switch]$Install,
    [switch]$Clean,
    [string]$VcpkgRoot,
    [string]$Generator = "Visual Studio 17 2022"
)

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------
$ScriptDir   = $PSScriptRoot
$PluginName  = "xmlpoints_pi"
$BuildDir    = Join-Path $env:TEMP "${PluginName}_build"
$PackageDir  = Join-Path $env:TEMP "${PluginName}_package"
$ZipPath     = Join-Path $ScriptDir "${PluginName}_windows.zip"

# ----------------------------------------------------------------------------
# Logging helpers
# ----------------------------------------------------------------------------
function Info  ($msg) { Write-Host "[INFO]  $msg" -ForegroundColor Green }
function Warn  ($msg) { Write-Host "[WARN]  $msg" -ForegroundColor Yellow }
function Die   ($msg) { Write-Host "[ERROR] $msg" -ForegroundColor Red; exit 1 }

# ----------------------------------------------------------------------------
# Step 0 - Clean (optional)
# ----------------------------------------------------------------------------
if ($Clean) {
    Info "Cleaning build artefacts..."
    Remove-Item -Recurse -Force $BuildDir   -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $PackageDir -ErrorAction SilentlyContinue
    Remove-Item -Force $ZipPath             -ErrorAction SilentlyContinue
}

# ----------------------------------------------------------------------------
# Step 1 - Locate build tools (CMake, MSVC, git, vcpkg)
# ----------------------------------------------------------------------------
Info "=== Step 1: Checking build tools ==="

function Test-Command($name) {
    return [bool](Get-Command $name -ErrorAction SilentlyContinue)
}

if (-not (Test-Command "cmake")) {
    Die "cmake not found in PATH. Install it (winget install Kitware.CMake) and re-run."
}
if (-not (Test-Command "git")) {
    Die "git not found in PATH. Install it (winget install Git.Git) and re-run."
}

# Confirm an MSVC toolset is actually installed (vswhere ships with VS installer).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsInstall) {
        Die ("No Visual Studio install with the 'Desktop development with C++' workload was found.`n" + `
             "Install Visual Studio Build Tools with that workload, then re-run.`n" + `
             "  winget install Microsoft.VisualStudio.2022.BuildTools")
    }
    Info "Found Visual Studio at: $vsInstall"
} else {
    Warn "vswhere.exe not found - cannot verify an MSVC C++ toolset is installed. Continuing anyway; cmake will fail later if it's missing."
}

# ----------------------------------------------------------------------------
# vcpkg: locate existing checkout, or bootstrap a new one
# ----------------------------------------------------------------------------
if (-not $VcpkgRoot) {
    if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
        $VcpkgRoot = $env:VCPKG_ROOT
    } elseif (Test-Path "C:\vcpkg\vcpkg.exe") {
        $VcpkgRoot = "C:\vcpkg"
    }
}

if (-not $VcpkgRoot) {
    $VcpkgRoot = "C:\vcpkg"
    Warn "vcpkg not found - cloning a fresh copy into $VcpkgRoot ..."
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
}
Info "Using vcpkg at: $VcpkgRoot"

# The prebuilt opencpn.lib shipped in OpenCPN/opencpn-libs (see below) is a
# 32-bit (x86) import library, matching the installed OpenCPN.exe this build
# targets - a DLL's bitness must match the process that loads it, so the
# whole toolchain (vcpkg triplet, generator platform) targets x86 throughout.
$Triplet = "x86-windows"

# ----------------------------------------------------------------------------
# Install wxWidgets + curl via vcpkg
# ----------------------------------------------------------------------------
# NOTE: "opengl" is vcpkg's feature flag for wxGLCanvas support (needed for
# RenderGLOverlay). If a future vcpkg release renames/removes it, run
# `& "$VcpkgRoot\vcpkg.exe" search wxwidgets` to see current feature names
# and adjust the line below.
Info "=== Step 1b: Installing wxWidgets + curl via vcpkg (this can take a while the first time) ==="
& "$VcpkgRoot\vcpkg.exe" install "wxwidgets[opengl]:${Triplet}" "curl:${Triplet}"
if ($LASTEXITCODE -ne 0) {
    Warn "vcpkg install with the 'opengl' feature failed - retrying with default features only."
    & "$VcpkgRoot\vcpkg.exe" install "wxwidgets:${Triplet}" "curl:${Triplet}"
    if ($LASTEXITCODE -ne 0) { Die "vcpkg failed to install wxwidgets/curl. See output above." }
}

$VcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"

# ----------------------------------------------------------------------------
# Locate or download the OpenCPN plugin API package (header + import lib)
# ----------------------------------------------------------------------------
# IMPORTANT: this must come from OpenCPN/opencpn-libs's api-XX packages, NOT
# the bare ocpn_plugin.h in OpenCPN/OpenCPN's master branch. opencpn.exe
# implements opencpn_plugin/PlugInChartBase itself; on Linux those symbols
# are left unresolved at link time and satisfied at dlopen() time by the
# running opencpn process, but MSVC's linker needs them resolved up front.
# Each api-XX folder in opencpn-libs ships a matched pair: ocpn_plugin.h and
# a prebuilt msvc-wx32/opencpn.lib import library built against wxWidgets
# 3.2 that exports exactly what that header's class hierarchy declares -
# mixing a header from one source with an unrelated .lib will just move the
# unresolved-symbol errors around instead of fixing them.
$ApiVersion = "api-21"

function Find-OrDownload-OcpnApi {
    $sdkDir = "$env:USERPROFILE\opencpn-sdk\$ApiVersion"
    $header = "$sdkDir\ocpn_plugin.h"
    $lib    = "$sdkDir\msvc-wx32\opencpn.lib"

    if ((Test-Path $header) -and (Test-Path $lib)) {
        return @{ Include = $sdkDir; Lib = $lib }
    }

    Warn "$ApiVersion plugin API package not found locally. Downloading from OpenCPN/opencpn-libs ..."
    New-Item -ItemType Directory -Force -Path "$sdkDir\msvc-wx32" | Out-Null

    $base = "https://raw.githubusercontent.com/OpenCPN/opencpn-libs/master/$ApiVersion"
    Invoke-WebRequest -Uri "$base/ocpn_plugin.h"         -OutFile $header
    Invoke-WebRequest -Uri "$base/msvc-wx32/opencpn.lib" -OutFile $lib
    # opencpn.pdb (debug symbols) is optional and not present for every api-XX
    # package; Invoke-WebRequest throws a terminating WebException on HTTP
    # errors regardless of -ErrorAction, so this needs a real try/catch.
    try {
        Invoke-WebRequest -Uri "$base/msvc-wx32/opencpn.pdb" -OutFile "$sdkDir\msvc-wx32\opencpn.pdb"
    } catch {
        Warn "opencpn.pdb not available for $ApiVersion (debug symbols only, not required to build) - skipping."
    }

    if (-not (Test-Path $header) -or -not (Test-Path $lib)) {
        Die ("Failed to download the $ApiVersion plugin API package. Download it manually from`n" + `
             "  https://github.com/OpenCPN/opencpn-libs/tree/master/$ApiVersion`n" + `
             "into $sdkDir\")
    }
    return @{ Include = $sdkDir; Lib = $lib }
}

$OcpnApi = Find-OrDownload-OcpnApi
Info "OpenCPN plugin API: $($OcpnApi.Include)"
Info "OpenCPN import lib : $($OcpnApi.Lib)"

# ----------------------------------------------------------------------------
# Step 2 - Configure with CMake
# ----------------------------------------------------------------------------
Info "=== Step 2: Configuring with CMake ($Generator, Win32) ==="
# CMake refuses to change generator/platform in an existing build dir (e.g.
# after switching x64 -> Win32), so always reconfigure from scratch here -
# reconfiguring is cheap compared to the actual compile step, and this keeps
# the script idempotent regardless of what a previous run configured.
Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

cmake -S $ScriptDir -B $BuildDir `
    -G $Generator -A Win32 `
    -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain" `
    -DVCPKG_TARGET_TRIPLET="$Triplet" `
    -DOPENCPN_INCLUDE_DIR="$($OcpnApi.Include)" `
    -DOPENCPN_IMPORT_LIB="$($OcpnApi.Lib)"
if ($LASTEXITCODE -ne 0) { Die "CMake configure failed. See output above." }

# ----------------------------------------------------------------------------
# Step 3 - Build
# ----------------------------------------------------------------------------
Info "=== Step 3: Building the plugin (Release) ==="
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { Die "Build failed. See output above." }

$DllPath = Get-ChildItem -Path $BuildDir -Recurse -Filter "${PluginName}.dll" |
           Select-Object -First 1 -ExpandProperty FullName
if (-not $DllPath) { Die "Build succeeded but ${PluginName}.dll was not found under $BuildDir." }
Info "Built: $DllPath"

# ----------------------------------------------------------------------------
# Step 4 - Assemble package directory
# ----------------------------------------------------------------------------
Info "=== Step 4: Assembling package ==="
Remove-Item -Recurse -Force $PackageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path "$PackageDir\plugins" | Out-Null
Copy-Item $DllPath "$PackageDir\plugins\${PluginName}.dll"

$Arch = "x86"
$OcpnTarget = "MSVC-${Arch}"

@"
<?xml version="1.0" encoding="utf-8" ?>
<plugin version="1">
  <name>$PluginName</name>
  <version>1.0.0</version>
  <release>1</release>
  <summary>Plot points from an XML file on the chart</summary>
  <api-version>1.16</api-version>
  <open-source>yes</open-source>
  <author>xmlpoints_pi</author>
  <description>Reads an XML file with latitude, longitude and information fields. Plots each entry as a clickable marker on the chart. Clicking a marker shows a popup with the information text.</description>
  <target>$OcpnTarget</target>
  <build-target>MSVC</build-target>
  <target-arch>$Arch</target-arch>
</plugin>
"@ | Set-Content -Path "$PackageDir\metadata.xml" -Encoding utf8

Info "Package metadata: target=$OcpnTarget"

# ----------------------------------------------------------------------------
# Step 5 - Create .zip
# ----------------------------------------------------------------------------
Info "=== Step 5: Creating ${PluginName}_windows.zip ==="
Remove-Item -Force $ZipPath -ErrorAction SilentlyContinue
Compress-Archive -Path "$PackageDir\*" -DestinationPath $ZipPath
Info "Package created: $ZipPath"

# ----------------------------------------------------------------------------
# Step 6 (optional) - Install directly for current user
# ----------------------------------------------------------------------------
if ($Install) {
    Info "=== Step 6: Installing plugin for current user ==="
    # Best-guess per-user plugin directory, analogous to ~/.local/lib/opencpn
    # on Linux. Verify this matches your OpenCPN install - if in doubt, use
    # Options -> Plugins -> Import plugin... with the .zip instead.
    $UserPluginDir = "$env:LOCALAPPDATA\opencpn\plugins"
    New-Item -ItemType Directory -Force -Path $UserPluginDir | Out-Null
    Copy-Item $DllPath "$UserPluginDir\${PluginName}.dll" -Force
    Info "Installed DLL to $UserPluginDir\"
}

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Build complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "  DLL     : $DllPath"
Write-Host "  Package : $ZipPath"
Write-Host ""
Write-Host "HOW TO INSTALL IN OPENCPN:"
Write-Host "  Option A - GUI import (recommended):"
Write-Host "    1. Open OpenCPN."
Write-Host "    2. Options -> Plugins -> (scroll to bottom) -> Import plugin..."
Write-Host "    3. Select: $ZipPath"
Write-Host "    4. RESTART OpenCPN completely - the plugin only appears after restart."
Write-Host ""
Write-Host "  Option B - direct user install (no GUI needed):"
Write-Host "    .\build_xmlpoints_pi.ps1 -Install"
Write-Host "    Then restart OpenCPN."
Write-Host ""
Write-Host "  TROUBLESHOOTING:"
Write-Host "    - If OpenCPN doesn't load the plugin, check its log (Help -> About -> Logfile,"
Write-Host "      or %LOCALAPPDATA%\opencpn\opencpn.log) for xmlpoints/DLL load errors."
Write-Host "    - A load failure is very often a wxWidgets ABI mismatch: the wxWidgets version"
Write-Host "      vcpkg installed here must match what your OpenCPN.exe was itself built"
Write-Host "      against. If it won't load, check OpenCPN's About box for its wx version and"
Write-Host "      pin vcpkg to a matching wxwidgets version (vcpkg install wxwidgets --version=...)."
Write-Host ""
