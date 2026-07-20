# S-124 Navigational Warnings (OpenCPN plugin)

OpenCPN plugin for displaying IHO S-124 navigational warnings on the chart.

This is a proof of concept: it demonstrates S-124 GML loading and SECOM-based
retrieval of navigational warnings within OpenCPN, but has not undergone the
testing or validation expected of production navigational software. See the
[Disclaimer and warranty](#disclaimer-and-warranty) section below before use.

## Features

- Load S-124 GML files from disk (single file or entire folder)
- Fetch live warnings from one or more SECOM service endpoints, with automatic
  pagination and optional periodic auto-refresh
- Renders warning areas, lines, and points as overlays on the chart, colored
  by warning severity (Local, Coastal, Sub-area, NAVAREA) — colors are
  user-configurable and apply the same way across every SECOM connection
- Click any warning marker or area on the chart to read its full text

## Installation

Pre-built packages are published on the [Releases](../../releases) page as
`.tar.gz` archives, one per platform:

- **Windows** (x86 `.dll`, MSVC build)
- **Linux** (x86_64 `.so`, Debian/Ubuntu build)
- **Raspberry Pi** (armhf/aarch64 `.so`, Raspberry Pi OS build)

To install:

1. Download the archive matching your platform from the Releases page.
2. Open OpenCPN, go to **Options → Plugins**, and use **Import Plugin** (or
   drag the `.tar.gz` onto the plugin list, depending on your OpenCPN
   version) to load the downloaded archive directly — there's no need to
   extract it first.
3. Enable the "S-124 Navigational Warnings" plugin and restart OpenCPN if
   prompted.

If no package is available for your platform, or you want to build from
source instead, see [Building](#building) below.

## SECOM Service URLs (France — PING / SHOM)

You can use [PING platform](https://portail.ping-info-nautique.fr/static/s124-2-0) for accesing opperational S-124 data. The IDs are available in the [Documentation](https://portail.ping-info-nautique.fr/static/s124-2-0).

Enter one of these URLs in the **SECOM Base URL** field of the plugin settings.

> No API key is required for public access. Leave the API Key field empty.

### AVURNAV (navigational warnings)

| Series | Base URL |
|---|---|
| NAVAREA II | `https://services.ping-info-nautique.fr/secom/navarea_ii_fr` |
| AVURNAV BREST | `https://services.ping-info-nautique.fr/secom/avurnav_brest_fr` |
| AVURNAV CHERBOURG | `https://services.ping-info-nautique.fr/secom/avurnav_cherbourg_fr` |
| AVURNAV TOULON | `https://services.ping-info-nautique.fr/secom/avurnav_toulon_fr` |
| AVURNAV CAYENNE | `https://services.ping-info-nautique.fr/secom/avurnav_cayenne_fr` |
| AVURNAV FORT DE FRANCE | `https://services.ping-info-nautique.fr/secom/avurnav_fort_de_france_fr` |
| AVURNAV LA REUNION | `https://services.ping-info-nautique.fr/secom/avurnav_la_reunion_fr` |
| AVURNAV LOCAL BREST | `https://services.ping-info-nautique.fr/secom/avurnav_local_brest_fr` |
| AVURNAV LOCAL CHERBOURG | `https://services.ping-info-nautique.fr/secom/avurnav_local_cherbourg_fr` |
| AVURNAV LOCAL TOULON | `https://services.ping-info-nautique.fr/secom/avurnav_local_toulon_fr` |
| AVURNAV LOCAL CAYENNE | `https://services.ping-info-nautique.fr/secom/avurnav_local_cayenne_fr` |
| AVURNAV LOCAL FORT DE FRANCE | `https://services.ping-info-nautique.fr/secom/avurnav_local_fort_de_france_fr` |
| AVURNAV LOCAL LA REUNION | `https://services.ping-info-nautique.fr/secom/avurnav_local_la_reunion_fr` |
| AVURNAV LOCAL PAPEETE | `https://services.ping-info-nautique.fr/secom/avurnav_local_papeete_fr` |

### AVINAV (inland waterway notices)

| Series | Base URL |
|---|---|
| AVINAV BREST | `https://services.ping-info-nautique.fr/secom/avinav_brest_fr` |
| AVINAV CHERBOURG | `https://services.ping-info-nautique.fr/secom/avinav_cherbourg_fr` |
| AVINAV TOULON | `https://services.ping-info-nautique.fr/secom/avinav_toulon_fr` |
| AVINAV CAYENNE | `https://services.ping-info-nautique.fr/secom/avinav_cayenne_fr` |
| AVINAV FORT DE FRANCE | `https://services.ping-info-nautique.fr/secom/avinav_fort_de_france_fr` |
| AVINAV LA REUNION | `https://services.ping-info-nautique.fr/secom/avinav_la_reunion_fr` |
| AVINAV PAPEETE | `https://services.ping-info-nautique.fr/secom/avinav_papeete_fr` |

### AVIRADE (port and harbour notices)

| Series | Base URL |
|---|---|
| AVIRADE BREST | `https://services.ping-info-nautique.fr/secom/avirade_brest_fr` |
| AVIRADE CHERBOURG | `https://services.ping-info-nautique.fr/secom/avirade_cherbourg_fr` |

## Building

The plugin is a CMake project (`CMakeLists.txt`) with two helper scripts that
wrap the full build-and-package process, so you normally don't need to
invoke CMake by hand:

- **`build_linux.sh`** — Linux (Debian/Ubuntu, `apt-get`).
  Installs build dependencies, configures and builds the plugin, and packages
  it into `s124navwarnings_pi.tar.gz`.
  ```bash
  bash build_linux.sh [--install] [--clean]
  ```
  - `--install` copies the built `.so` into `~/.opencpn/plugins/` afterwards
  - `--clean` removes previous build/package artifacts first

- **`build_win.ps1`** — Windows (MSVC + vcpkg). Locates or
  installs dependencies (vcpkg for curl, prebuilt wxWidgets binaries),
  configures with CMake/MSVC, builds a Release x86 `.dll`, and packages it
  into the same tarball format expected by OpenCPN's plugin manager.
  ```powershell
  .\build_win.ps1 [-Install] [-Clean] [-VcpkgRoot <path>] [-Generator <name>]
  ```
  - `-Install` copies the built DLL into the OpenCPN plugin directory
  - `-Clean` removes previous build artifacts (and the tarball) first
  - `-VcpkgRoot` points at an existing vcpkg checkout instead of cloning one
  - `-Generator` selects the Visual Studio generator (defaults to VS 2022)

The resulting `.tar.gz` can be imported directly through OpenCPN's plugin
manager on either platform.

## Disclaimer and warranty

This software is provided "as is", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose, and noninfringement. The authors and
contributors are not responsible for any damage, loss, or navigational
incident arising from the use, misuse, or inability to use this software.
This plugin is not a certified navigational aid and must not be used as the
sole means of receiving navigational warnings. Always cross-check with
official sources.

## Development

This plugin was developed with the assistance of [Claude](https://www.anthropic.com/claude), Anthropic's AI model.

## Version history

- **1.0** — Initial version
