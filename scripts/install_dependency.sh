#!/usr/bin/env bash
set -euo pipefail

# This script installs common build/runtime dependencies for Linux/macOS
# for local development and Raspberry Pi bring-up.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SKIP_APT_UPDATE="OFF"
SKIP_PYTHON_DEPS="OFF"
MODE="default" # default | rpi

have_brew_formula() {
  brew list --formula "$1" >/dev/null 2>&1
}

install_brew_formula_if_missing() {
  local formula="$1"
  local command_name="${2:-}"

  if [[ -n "${command_name}" ]] && command -v "${command_name}" >/dev/null 2>&1; then
    echo "[INFO] ${command_name} is already available; skip brew install ${formula}"
    return
  fi

  if have_brew_formula "${formula}"; then
    echo "[INFO] ${formula} is already installed"
    return
  fi

  brew install "${formula}"
}

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

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
  if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
  else
    echo "[ERROR] Please run as root or install sudo." >&2
    exit 1
  fi
fi

if command -v apt-get >/dev/null 2>&1; then
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

  echo "[INFO] Installing system dependencies with apt (mode=${MODE})"
  ${SUDO} apt-get install -y --no-install-recommends "${packages[@]}"
elif command -v brew >/dev/null 2>&1; then
  echo "[INFO] Installing system dependencies with brew"
  brew update
  install_brew_formula_if_missing cmake cmake
  install_brew_formula_if_missing ninja ninja
  install_brew_formula_if_missing pkg-config pkg-config
  install_brew_formula_if_missing git git
  install_brew_formula_if_missing curl curl
  install_brew_formula_if_missing wget wget
  install_brew_formula_if_missing python python3
  install_brew_formula_if_missing openssl@3
  install_brew_formula_if_missing boost
else
  echo "[ERROR] Unsupported package manager. Install cmake, ninja, pkg-config, git, curl, wget, Python, OpenSSL, and Boost." >&2
  exit 1
fi

if [[ "${SKIP_PYTHON_DEPS}" != "ON" ]]; then
  if ! python3 -c "import yaml" >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
      brew install pyyaml || true
    fi
  fi

  if ! python3 -c "import yaml" >/dev/null 2>&1; then
    VENV_DIR="${REPO_ROOT}/.venv-autosar-deps"
    echo "[INFO] Installing PyYAML into local virtualenv: ${VENV_DIR}"
    python3 -m venv "${VENV_DIR}"
    "${VENV_DIR}/bin/python" -m pip install --upgrade pip
    "${VENV_DIR}/bin/python" -m pip install pyyaml
    echo "[INFO] Use this virtualenv when running Python tools:"
    echo "       source ${VENV_DIR}/bin/activate"
  fi
fi

echo "[OK] Dependency installation complete."
