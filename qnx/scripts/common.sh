#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QNX_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${QNX_ROOT}/.." && pwd)"

function qnx_info() {
  echo "[QNX][INFO] $*"
}

function qnx_warn() {
  echo "[QNX][WARN] $*" >&2
}

function qnx_error() {
  echo "[QNX][ERROR] $*" >&2
}

function qnx_die() {
  qnx_error "$*"
  exit 1
}

function qnx_nproc() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi

  getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
}

function qnx_default_jobs() {
  if [[ -n "${AUTOSAR_QNX_JOBS:-}" ]]; then
    echo "${AUTOSAR_QNX_JOBS}"
  else
    qnx_nproc
  fi
}

function qnx_default_out_root() {
  if [[ -n "${AUTOSAR_QNX_OUT_ROOT:-}" ]]; then
    echo "${AUTOSAR_QNX_OUT_ROOT}"
  else
    echo "${REPO_ROOT}/out/qnx"
  fi
}

function qnx_default_arch() {
  if [[ -n "${AUTOSAR_QNX_ARCH:-}" ]]; then
    echo "${AUTOSAR_QNX_ARCH}"
  else
    echo "aarch64le"
  fi
}

function qnx_require_cmd() {
  local cmd="$1"
  command -v "${cmd}" >/dev/null 2>&1 || qnx_die "Required command not found: ${cmd}"
}

function qnx_require_env() {
  local var_name="$1"
  [[ -n "${!var_name:-}" ]] || qnx_die "Required environment variable is not set: ${var_name}"
}

function qnx_setup_env() {
  qnx_require_env "QNX_HOST"
  qnx_require_env "QNX_TARGET"

  [[ -d "${QNX_HOST}" ]] || qnx_die "QNX_HOST directory does not exist: ${QNX_HOST}"
  [[ -d "${QNX_TARGET}" ]] || qnx_die "QNX_TARGET directory does not exist: ${QNX_TARGET}"

  export PATH="${QNX_HOST}/usr/bin:${PATH}"

  qnx_require_cmd "qcc"
  qnx_require_cmd "q++"
  qnx_require_cmd "cmake"
  qnx_require_cmd "git"
}

function qnx_arch_to_qcc_variant() {
  local arch="$1"
  case "${arch}" in
    aarch64le)
      echo "gcc_ntoaarch64le"
      ;;
    x86_64)
      echo "gcc_ntox86_64"
      ;;
    *)
      qnx_die "Unsupported QNX target arch: ${arch} (supported: aarch64le, x86_64)"
      ;;
  esac
}

function qnx_arch_to_boost_arch() {
  local arch="$1"
  case "${arch}" in
    aarch64le)
      echo "arm"
      ;;
    x86_64)
      echo "x86"
      ;;
    *)
      qnx_die "Unsupported QNX target arch for Boost: ${arch}"
      ;;
  esac
}

function qnx_arch_to_address_model() {
  local arch="$1"
  case "${arch}" in
    aarch64le|x86_64)
      echo "64"
      ;;
    *)
      qnx_die "Unsupported QNX target arch for address model: ${arch}"
      ;;
  esac
}

function qnx_resolve_toolchain_file() {
  echo "${QNX_ROOT}/cmake/toolchain_qnx800.cmake"
}

function qnx_prepare_dir() {
  local path="$1"
  rm -rf "${path}"
  mkdir -p "${path}"
}

function qnx_detect_libdir() {
  local prefix="$1"
  if [[ -d "${prefix}/lib64" ]]; then
    echo "lib64"
  else
    echo "lib"
  fi
}

function qnx_download_file() {
  local url="$1"
  local output_path="$2"
  qnx_require_cmd "curl"
  curl -fL "${url}" -o "${output_path}"
}

function qnx_clone_or_update() {
  local repo_url="$1"
  local ref="$2"
  local destination="$3"

  if [[ -d "${destination}/.git" ]]; then
    git -C "${destination}" fetch --tags --force --prune
  else
    rm -rf "${destination}"
    git clone "${repo_url}" "${destination}"
  fi

  if [[ -n "${ref}" ]]; then
    git -C "${destination}" checkout --force "${ref}"
  fi
}
