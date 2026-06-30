#!/usr/bin/env bash
# =============================================================================
# build_xmlpoints_pi.sh
# Builds and packages the XML Points OpenCPN plugin for Debian/Ubuntu systems.
# Usage:  bash build_xmlpoints_pi.sh [--install] [--clean]
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_NAME="xmlpoints_pi"
BUILD_DIR="/tmp/${PLUGIN_NAME}_build"
PACKAGE_DIR="/tmp/${PLUGIN_NAME}_package"
TARBALL="${SCRIPT_DIR}/${PLUGIN_NAME}.tar.gz"

# --------------------------------------------------------------------------
# Colours
# --------------------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# --------------------------------------------------------------------------
# Parse arguments
# --------------------------------------------------------------------------
DO_INSTALL=false
DO_CLEAN=false
for arg in "$@"; do
    case $arg in
        --install) DO_INSTALL=true ;;
        --clean)   DO_CLEAN=true   ;;
        --help|-h)
            echo "Usage: $0 [--install] [--clean]"
            echo "  --install  Copy .so into ~/.opencpn/plugins/ after build"
            echo "  --clean    Remove build/ and package/ directories first"
            exit 0 ;;
        *) warn "Unknown argument: $arg" ;;
    esac
done

# --------------------------------------------------------------------------
# Step 0 – Clean (optional)
# --------------------------------------------------------------------------
if $DO_CLEAN; then
    info "Cleaning build artefacts…"
    rm -rf "${BUILD_DIR}" "${PACKAGE_DIR}"
    rm -f "${TARBALL}"
fi

# --------------------------------------------------------------------------
# Step 1 – Detect package manager & OS info
# --------------------------------------------------------------------------
info "=== Step 1: Installing build dependencies ==="

if ! command -v apt-get &>/dev/null; then
    error "This script requires apt-get (Debian/Ubuntu family)."
fi

# Detect OS version — try lsb_release, fall back to /etc/os-release
detect_os_version() {
    if command -v lsb_release &>/dev/null; then
        lsb_release -rs
    elif [[ -f /etc/os-release ]]; then
        # shellcheck source=/dev/null
        source /etc/os-release
        echo "${VERSION_ID:-unknown}"
    else
        echo "unknown"
    fi
}

detect_os_id() {
    if command -v lsb_release &>/dev/null; then
        lsb_release -is | tr '[:upper:]' '[:lower:]'
    elif [[ -f /etc/os-release ]]; then
        # shellcheck source=/dev/null
        source /etc/os-release
        echo "${ID:-linux}"
    else
        echo "linux"
    fi
}

# --------------------------------------------------------------------------
# Base dependencies (always required)
# --------------------------------------------------------------------------
BASE_DEPS=(
    build-essential
    cmake
    pkg-config
    lsb-release
    libgl-dev
    libglu1-mesa-dev
    libcurl4-openssl-dev
    wget
)

info "Updating package list…"
sudo apt-get update -qq || warn "apt-get update had errors (possibly an unsupported PPA); continuing…"

info "Installing base dependencies: ${BASE_DEPS[*]}"
sudo apt-get install -y --ignore-missing "${BASE_DEPS[@]}"

# --------------------------------------------------------------------------
# wxWidgets: probe for the best available version
# --------------------------------------------------------------------------
install_wxwidgets() {
    local candidates=(
        "libwxgtk3.2-dev"        # Ubuntu 24.04+, Debian 12+
        "libwxgtk3.0-gtk3-dev"   # Ubuntu 20.04 / 22.04
        "libwxgtk3.0-dev"        # older Debian/Ubuntu
    )

    for pkg in "${candidates[@]}"; do
        if apt-cache show "${pkg}" &>/dev/null 2>&1; then
            info "Installing wxWidgets package: ${pkg}"
            sudo apt-get install -y "${pkg}"
            return 0
        fi
    done

    error "No compatible wxWidgets development package found.
Tried: ${candidates[*]}
Please install one manually and re-run."
}

install_wxwidgets

# --------------------------------------------------------------------------
# OpenCPN (install the app; provides headers on some distros)
# --------------------------------------------------------------------------
info "Attempting to install opencpn and opencpn-dev (optional)…"
sudo apt-get install -y --ignore-missing opencpn opencpn-dev || true

# --------------------------------------------------------------------------
# Locate ocpn_plugin.h – search standard paths, then installed package, then download
# --------------------------------------------------------------------------
find_or_download_ocpn_header() {
    local candidates=(
        "${HOME}/opencpn-sdk/include"
        /usr/include/opencpn
        /usr/local/include/opencpn
        /usr/include
        /usr/local/include
    )

    for dir in "${candidates[@]}"; do
        if [[ -f "${dir}/ocpn_plugin.h" ]]; then
            echo "${dir}"
            return 0
        fi
    done

    # Try extracting the path from the installed opencpn package
    if command -v dpkg &>/dev/null; then
        local pkg_header
        pkg_header=$(dpkg -L opencpn 2>/dev/null | grep "ocpn_plugin.h" | head -1 || true)
        if [[ -n "${pkg_header}" && -f "${pkg_header}" ]]; then
            echo "$(dirname "${pkg_header}")"
            return 0
        fi
    fi

    # Auto-download from the OpenCPN GitHub repository, pinned to the installed version
    warn "ocpn_plugin.h not found in standard locations. Downloading from GitHub…"
    local sdk_dir="${HOME}/opencpn-sdk/include"
    mkdir -p "${sdk_dir}"

    # Derive git tag from installed opencpn package version (e.g. "1:5.12.4+dfsg-1" → "v5.12.4")
    local ocpn_ver tag url
    ocpn_ver=$(dpkg-query -W -f='${Version}' opencpn 2>/dev/null \
               | sed 's/^[0-9]*://; s/[+~].*//')   # strip epoch and dfsg suffix
    if [[ -n "${ocpn_ver}" ]]; then
        tag="v${ocpn_ver}"
        url="https://raw.githubusercontent.com/OpenCPN/OpenCPN/${tag}/include/ocpn_plugin.h"
        info "Fetching ocpn_plugin.h for OpenCPN ${ocpn_ver} (tag ${tag})…"
    else
        warn "Cannot determine OpenCPN version; falling back to master branch."
        url="https://raw.githubusercontent.com/OpenCPN/OpenCPN/master/include/ocpn_plugin.h"
    fi

    if command -v wget &>/dev/null; then
        wget -q --show-progress -O "${sdk_dir}/ocpn_plugin.h" "${url}" || {
            warn "Tag ${tag} not found on GitHub; retrying with master…"
            wget -q --show-progress -O "${sdk_dir}/ocpn_plugin.h" \
                "https://raw.githubusercontent.com/OpenCPN/OpenCPN/master/include/ocpn_plugin.h"
        }
    elif command -v curl &>/dev/null; then
        curl -fSL "${url}" -o "${sdk_dir}/ocpn_plugin.h" || {
            warn "Tag ${tag} not found on GitHub; retrying with master…"
            curl -fSL "https://raw.githubusercontent.com/OpenCPN/OpenCPN/master/include/ocpn_plugin.h" \
                -o "${sdk_dir}/ocpn_plugin.h"
        }
    else
        error "Neither wget nor curl is available. Install one and re-run, or manually place ocpn_plugin.h in ${sdk_dir}/"
    fi

    if [[ -f "${sdk_dir}/ocpn_plugin.h" ]]; then
        info "Downloaded ocpn_plugin.h to ${sdk_dir}/"
        echo "${sdk_dir}"
        return 0
    fi

    error "Failed to download ocpn_plugin.h. Please download it manually:
  mkdir -p ${sdk_dir}
  wget https://raw.githubusercontent.com/OpenCPN/OpenCPN/master/include/ocpn_plugin.h -O ${sdk_dir}/ocpn_plugin.h"
}

OCPN_SYSTEM_INCLUDE="$(find_or_download_ocpn_header)"
info "OpenCPN include path: ${OCPN_SYSTEM_INCLUDE}"

# --------------------------------------------------------------------------
# Step 2 – Configure with CMake
# --------------------------------------------------------------------------
info "=== Step 2: Configuring with CMake ==="
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${SCRIPT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENCPN_INCLUDE_DIR="${OCPN_SYSTEM_INCLUDE}"

# --------------------------------------------------------------------------
# Step 3 – Build
# --------------------------------------------------------------------------
info "=== Step 3: Building the plugin ==="
JOBS=$(nproc 2>/dev/null || echo 2)
cmake --build . -- -j"${JOBS}"

SO_PATH="${BUILD_DIR}/lib/lib${PLUGIN_NAME}.so"
if [[ ! -f "${SO_PATH}" ]]; then
    error "Build succeeded but ${SO_PATH} not found. Check CMakeLists.txt output names."
fi
info "Built: ${SO_PATH}"

# --------------------------------------------------------------------------
# Step 4 – Assemble package directory
# --------------------------------------------------------------------------
info "=== Step 4: Assembling package ==="

rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/lib/opencpn"
mkdir -p "${PACKAGE_DIR}/share/opencpn/plugins/${PLUGIN_NAME}"

cp "${SO_PATH}" "${PACKAGE_DIR}/lib/opencpn/"
cp "${SCRIPT_DIR}/data/sample_points.xml" \
   "${PACKAGE_DIR}/share/opencpn/plugins/${PLUGIN_NAME}/"

# Build metadata: derive target string from detected OS/arch
ARCH=$(uname -m)
OS_ID=$(detect_os_id)
OS_VER=$(detect_os_version)

# Map OS ID to OpenCPN target prefix (ubuntu covers Debian derivatives too)
case "${OS_ID}" in
    ubuntu|neon|pop|linuxmint) TARGET_OS="ubuntu" ;;
    debian)                    TARGET_OS="debian"  ;;
    *)                         TARGET_OS="ubuntu"  ;;   # safe fallback
esac

# Map machine arch to OpenCPN arch label
case "${ARCH}" in
    x86_64)  OCPN_ARCH="x86_64" ;;
    aarch64) OCPN_ARCH="arm64"  ;;
    armv7l)  OCPN_ARCH="armhf"  ;;
    *)       OCPN_ARCH="${ARCH}" ;;
esac

OCPN_TARGET="${TARGET_OS}-${OCPN_ARCH}"

cat > "${PACKAGE_DIR}/metadata.xml" <<EOF
<?xml version="1.0" encoding="utf-8" ?>
<plugin version="1">
  <name>${PLUGIN_NAME}</name>
  <version>1.0.0</version>
  <release>1</release>
  <summary>Plot points from an XML file on the chart</summary>
  <api-version>1.16</api-version>
  <open-source>yes</open-source>
  <author>xmlpoints_pi</author>
  <description>Reads an XML file with latitude, longitude and information fields. Plots each entry as a clickable marker on the chart. Clicking a marker shows a popup with the information text.</description>
  <target>${OCPN_TARGET}</target>
  <build-target>${TARGET_OS}</build-target>
  <build-gtk>gtk3</build-gtk>
  <target-version>${OS_VER}</target-version>
  <target-arch>${ARCH}</target-arch>
</plugin>
EOF

info "Package metadata: target=${OCPN_TARGET}, version=${OS_VER}, arch=${ARCH}"

# --------------------------------------------------------------------------
# Step 5 – Create .tar.gz
# --------------------------------------------------------------------------
info "=== Step 5: Creating ${PLUGIN_NAME}.tar.gz ==="
cd "${PACKAGE_DIR}"
tar -czf "${TARBALL}" .
info "Package created: ${TARBALL}"

# --------------------------------------------------------------------------
# Step 6 (optional) – Install directly for current user
# --------------------------------------------------------------------------
if $DO_INSTALL; then
    info "=== Step 6: Installing plugin for current user ==="

    # OpenCPN 5.x scans ~/.local/lib/opencpn/ for user-installed plugins.
    # The legacy ~/.opencpn/plugins/ path is NOT scanned by modern OpenCPN.
    USER_PLUGIN_DIR="${HOME}/.local/lib/opencpn"
    mkdir -p "${USER_PLUGIN_DIR}"
    cp "${SO_PATH}" "${USER_PLUGIN_DIR}/lib${PLUGIN_NAME}.so"
    info "Installed .so to ${USER_PLUGIN_DIR}/"

    USER_DATA_DIR="${HOME}/.local/share/opencpn/plugins/${PLUGIN_NAME}"
    mkdir -p "${USER_DATA_DIR}"
    cp "${SCRIPT_DIR}/data/sample_points.xml" "${USER_DATA_DIR}/"
    info "Sample XML copied to ${USER_DATA_DIR}/"
fi

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Build complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "  Shared library : ${SO_PATH}"
echo "  Tarball        : ${TARBALL}"
echo "  Target         : ${OCPN_TARGET} / OS ${OS_VER}"
echo ""
echo "HOW TO INSTALL IN OPENCPN:"
echo "  Option A – GUI import (recommended):"
echo "    1. Open OpenCPN."
echo "    2. Options → Plugins → (scroll to bottom) → Import plugin…"
echo "    3. Select: ${TARBALL}"
echo "    4. RESTART OpenCPN completely – the plugin only appears after restart."
echo ""
echo "  Option B – direct user install (no GUI needed):"
echo "    bash ${0} --install"
echo "    Then restart OpenCPN."
echo ""
echo "  Option C – system-wide install (all users):"
echo "    sudo cp ${SO_PATH} /usr/lib/opencpn/"
echo "    Then restart OpenCPN."
echo ""
echo "  TROUBLESHOOTING – if the plugin does not appear after restart:"
echo "    Check: ~/.opencpn/opencpn.log  (look for xmlpoints errors)"
echo "    Verify: ls ~/.local/lib/opencpn/libxmlpoints_pi.so  (after option B)"
echo "    Verify: ls /usr/lib/opencpn/libxmlpoints_pi.so      (after option C)"
echo ""
echo "HOW TO USE:"
echo "  1. Start OpenCPN."
echo "  2. Go to Options → Plugins → XML Points → Enable."
echo "  3. Close Options and click the XML Points toolbar button (blue circle icon)."
echo "  4. Choose your XML file."
echo "  5. Click any red marker on the chart to see its information."
echo ""
echo "SAMPLE XML FORMAT:"
echo "  See: ${SCRIPT_DIR}/data/sample_points.xml"
echo ""
