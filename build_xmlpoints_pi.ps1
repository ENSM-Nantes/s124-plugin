<#
.SYNOPSIS
    Builds and packages the XML Points OpenCPN plugin on Windows (MSVC + vcpkg).

.DESCRIPTION
    Windows counterpart to build_xmlpoints_pi.sh. Installs/locates build
    dependencies (curl via vcpkg, wxWidgets via wxWidgets.org's own prebuilt
    Windows binaries - see the wxWidgets section below for why NOT vcpkg),
    configures the project with CMake + MSVC, builds a Release Win32 (x86)
    xmlpoints_pi.dll - matching the prebuilt opencpn.lib import library
    available for this plugin API version - and packages it into a .tar.gz
    (OpenCPN's plugin manager import expects a tarball, not a .zip, on every
    platform including Windows).

.PARAMETER Install
    Copy the built DLL into this user's OpenCPN plugin directory after build.

.PARAMETER Clean
    Remove the build/ and package/ directories (and the .tar.gz) before building.

.PARAMETER VcpkgRoot
    Path to an existing vcpkg checkout. If omitted, the script looks for
    $env:VCPKG_ROOT, then C:\vcpkg, then clones a fresh one there.

.PARAMETER Generator
    CMake generator to use. Defaults to "Visual Studio 17 2022". Pass e.g.
    "Visual Studio 16 2019" if that's what you have installed instead.

.NOTES
    IMPORTANT: this script has not been run end-to-end on a real Windows
    machine (it was written/reviewed from a Linux session). Treat the first
    run as a shakedown: read the step output, and if the wxWidgets version
    ($WxVersion below) or your OpenCPN install layout differ from what's
    assumed below, adjust the marked spots rather than assuming the script
    is wrong wholesale.

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
$TarballPath = Join-Path $ScriptDir "${PluginName}_windows.tar.gz"

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
    Remove-Item -Force $TarballPath          -ErrorAction SilentlyContinue
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
#
# Use a STATIC triplet (x86-windows-static-md) for curl only (wxWidgets is
# NOT sourced from vcpkg - see the wxWidgets section below): OpenCPN plugins
# ship as a single .dll with no companion runtime files, so curl must be
# linked directly into the plugin rather than pulled in as a separate
# vcpkg-built DLL that will never exist next to opencpn.exe. The "-md" suffix
# keeps the dynamic CRT (/MD) to match opencpn.exe's own runtime, since
# mixing static (/MT) and dynamic (/MD) CRT allocators across a DLL boundary
# that passes wxString/std containers by value is a classic source of heap
# corruption.
$Triplet = "x86-windows-static-md"

# ----------------------------------------------------------------------------
# Install curl via vcpkg
# ----------------------------------------------------------------------------
Info "=== Step 1b: Installing curl via vcpkg (this can take a while the first time) ==="
& "$VcpkgRoot\vcpkg.exe" install "curl:${Triplet}"
if ($LASTEXITCODE -ne 0) { Die "vcpkg failed to install curl. See output above." }

$VcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"

# ----------------------------------------------------------------------------
# wxWidgets: download wxWidgets.org's own prebuilt Windows binaries - NOT vcpkg
# ----------------------------------------------------------------------------
# OpenCPN's plugin loader (plugin_loader.cpp) decides whether a Windows
# plugin DLL is "compatible" by scanning its PE import table for a DLL name
# containing "wxmsw" + "_core_" and checking whether that name is a substring
# match against the wxWidgets core DLL the running opencpn.exe process
# already has loaded (e.g. "wxmsw32u_core_vc14x.dll"). This is a literal
# filename check, not a real ABI/version probe - so the plugin must
# dynamically import a wx build with that *exact* DLL name, or the loader
# logs "Plugin is compatible: false" and OpenCPN silently uninstalls it (the
# generic "encountered errors during startup ... will be uninstalled"
# message). vcpkg's wxwidgets port uses its own DLL naming scheme regardless
# of static/dynamic triplet, so building against it - as earlier versions of
# this script did - can never satisfy that check.
#
# The fix is to build against the same official wxWidgets.org Windows
# binaries OpenCPN itself ships with (see OpenCPN/OpenCPN's
# buildwin/win_deps.bat for the upstream equivalent of this step), which
# produce the "lib\vc14x_dll" layout CMake's FindwxWidgets module already
# knows how to auto-detect via wxWidgets_ROOT_DIR - no CMakeLists.txt changes
# needed.
#
# $WxVersion MUST match the wxWidgets version OpenCPN itself was built
# against - check OpenCPN's log (the "wxWidgets version: wxWidgets X.Y.Z"
# line near the top after a restart) or Help -> About, and adjust below if
# it differs from what's currently set.
$WxVersion = "3.2.9"
$WxRoot = "$env:USERPROFILE\opencpn-sdk\wxWidgets-$WxVersion"

function Find-OrDownload-WxWidgets {
    if (Test-Path "$WxRoot\include\wx\wx.h") {
        return $WxRoot
    }

    if (-not (Test-Command "7z")) {
        Die ("7z.exe not found in PATH - required to unpack wxWidgets' .7z release archives.`n" + `
             "Install it (winget install 7zip.7zip) and re-run.")
    }

    Warn "wxWidgets $WxVersion prebuilt binaries not found locally. Downloading from wxWidgets/wxWidgets ..."
    New-Item -ItemType Directory -Force -Path $WxRoot | Out-Null
    $dlDir = Join-Path $env:TEMP "wxWidgets-$WxVersion-dl"
    New-Item -ItemType Directory -Force -Path $dlDir | Out-Null

    # Three archives, unpacked on top of each other into the same root:
    # headers (platform-independent), Dev (import .lib stubs + headers
    # config needed to compile against the DLLs), ReleaseDLL (the actual
    # runtime DLLs, e.g. wxmsw32u_core_vc14x.dll).
    $base = "https://github.com/wxWidgets/wxWidgets/releases/download/v$WxVersion"
    $archives = @(
        "wxWidgets-$WxVersion-headers.7z",
        "wxMSW-${WxVersion}_vc14x_Dev.7z",
        "wxMSW-${WxVersion}_vc14x_ReleaseDLL.7z"
    )
    foreach ($archive in $archives) {
        $out = Join-Path $dlDir $archive
        Invoke-WebRequest -Uri "$base/$archive" -OutFile $out
        & 7z x -y "-o$WxRoot" $out | Out-Null
        if ($LASTEXITCODE -ne 0) { Die "Failed to extract $archive into $WxRoot." }
    }

    if (-not (Test-Path "$WxRoot\include\wx\wx.h")) {
        Die "Failed to set up wxWidgets $WxVersion at $WxRoot - check the download/extract output above."
    }
    return $WxRoot
}

$WxRoot = Find-OrDownload-WxWidgets
Info "wxWidgets: $WxRoot"

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

# wxWidgets_ROOT_DIR points CMake's FindwxWidgets module at the
# wxWidgets.org binaries downloaded above (see the wxWidgets section in
# Step 1b) instead of anything vcpkg knows about; the vcpkg toolchain file
# below is still needed for curl.
cmake -S $ScriptDir -B $BuildDir `
    -G $Generator -A Win32 `
    -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain" `
    -DVCPKG_TARGET_TRIPLET="$Triplet" `
    -DwxWidgets_ROOT_DIR="$WxRoot" `
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

# "msvc-wx32" is the actual enum value OpenCPN's plugin schema (ocpn-plugin.xsd
# in OpenCPN/plugins) recognizes for a 32-bit MSVC build against wxWidgets 3.2
# - matching the "msvc-wx32" opencpn-libs folder this build's opencpn.lib came
# from. An unrecognized <target> string is exactly what "Plugin incompatible"
# means; it is not a free-form label.
$Arch = "x86"
$OcpnTarget = "msvc-wx32"
$WinVersion = [System.Environment]::OSVersion.Version.ToString()
$TarballUri = "file:///" + ($TarballPath -replace '\\', '/')

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
  <source>https://example.invalid/xmlpoints_pi</source>
  <description>Reads an XML file with latitude, longitude and information fields. Plots each entry as a clickable marker on the chart. Clicking a marker shows a popup with the information text.</description>
  <target>$OcpnTarget</target>
  <target-version>$WinVersion</target-version>
  <target-arch>$Arch</target-arch>
  <tarball-url>$TarballUri</tarball-url>
</plugin>
"@ | Set-Content -Path "$PackageDir\metadata.xml" -Encoding utf8

Info "Package metadata: target=$OcpnTarget target-version=$WinVersion"

# ----------------------------------------------------------------------------
# Step 5 - Create .tar.gz
# ----------------------------------------------------------------------------
Info "=== Step 5: Creating ${PluginName}_windows.tar.gz ==="
if (-not (Test-Command "tar")) {
    Die ("tar.exe not found in PATH. It ships built-in with Windows 10 1803+/11 - " + `
         "if it's missing, install bsdtar or 7-Zip and adjust this step.")
}
Remove-Item -Force $TarballPath -ErrorAction SilentlyContinue
Push-Location $PackageDir
tar -czf $TarballPath .
Pop-Location
if ($LASTEXITCODE -ne 0) { Die "tar failed to create the package archive. See output above." }
Info "Package created: $TarballPath"

# ----------------------------------------------------------------------------
# Step 6 (optional) - Install directly for current user
# ----------------------------------------------------------------------------
if ($Install) {
    Info "=== Step 6: Installing plugin for current user ==="
    # Best-guess per-user plugin directory, analogous to ~/.local/lib/opencpn
    # on Linux. Verify this matches your OpenCPN install - if in doubt, use
    # Options -> Plugins -> Import plugin... with the .tar.gz instead.
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
Write-Host "  Package : $TarballPath"
Write-Host ""
Write-Host "HOW TO INSTALL IN OPENCPN:"
Write-Host "  Option A - GUI import (recommended):"
Write-Host "    1. Open OpenCPN."
Write-Host "    2. Options -> Plugins -> (scroll to bottom) -> Import plugin..."
Write-Host "    3. Select: $TarballPath"
Write-Host "    4. RESTART OpenCPN completely - the plugin only appears after restart."
Write-Host ""
Write-Host "  Option B - direct user install (no GUI needed):"
Write-Host "    .\build_xmlpoints_pi.ps1 -Install"
Write-Host "    Then restart OpenCPN."
Write-Host ""
Write-Host "  TROUBLESHOOTING:"
Write-Host "    - If OpenCPN doesn't load the plugin, check its log (Help -> About -> Logfile,"
Write-Host "      or %LOCALAPPDATA%\opencpn\opencpn.log) for xmlpoints/DLL load errors."
Write-Host "    - Look for 'Checking plugin compatibility: ...xmlpoints_pi.dll' in that log."
Write-Host "      If the next line is NOT 'Found wxWidgets core DLL: ...wxmsw32u_core_vc14x.dll',"
Write-Host "      OpenCPN will mark it 'Plugin is compatible: false' and silently uninstall it."
Write-Host "      That means the `$WxVersion setting in this script ($WxVersion) no longer"
Write-Host "      matches the wxWidgets version in OpenCPN's own log/About box - update it"
Write-Host "      and rebuild."
Write-Host ""
