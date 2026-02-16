#!/usr/bin/env bash
set -euo pipefail

# This script installs common build/runtime dependencies for Linux (Debian/Ubuntu)
# for local development and Raspberry Pi bring-up.

SKIP_APT_UPDATE="OFF"
SKIP_PYTHON_DEPS="OFF"
MODE="default" # default | rpi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-apt-update)
      SKIP_APT_UPDATE="ON"
      shift
      ;;
    --skip-python-deps)
      SKIP_PYTHON_DEPS="ON"
      shift
      ;;
    --mode)
      MODE="$2"
      shift 2
      ;;
    --rpi)
      MODE="rpi"
      shift
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if ! command -v apt-get >/dev/null 2>&1; then
  echo "[ERROR] This script currently supports Debian/Ubuntu (apt-get) only." >&2
  exit 1
fi

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
  if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
  else
    echo "[ERROR] Please run as root or install sudo." >&2
    exit 1
  fi
fi

if [[ "${SKIP_APT_UPDATE}" != "ON" ]]; then
  ${SUDO} apt-get update -qq
fi

packages=(
  build-essential
  cmake
  ninja-build
  pkg-config
  git
  curl
  wget
  tar
  unzip
  python3
  python3-pip
  python3-venv
  python3-setuptools
  python3-wheel
  python3-yaml
  libboost-system-dev
  libboost-thread-dev
  libboost-filesystem-dev
  libboost-log-dev
  libboost-program-options-dev
  libacl1-dev
  libncurses5-dev
  libssl-dev
  libsystemd-dev
)

if [[ "${MODE}" == "rpi" ]]; then
  packages+=(
    can-utils
    iproute2
  )
fi

echo "[INFO] Installing system dependencies (mode=${MODE})"
${SUDO} apt-get install -y --no-install-recommends "${packages[@]}"

if [[ "${SKIP_PYTHON_DEPS}" != "ON" ]]; then
  if ! python3 -c "import yaml" >/dev/null 2>&1; then
    python3 -m pip install --user pyyaml || \
      python3 -m pip install --break-system-packages pyyaml
  fi
fi

echo "[OK] Dependency installation complete."
