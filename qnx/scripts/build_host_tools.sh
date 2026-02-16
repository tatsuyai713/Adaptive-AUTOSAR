#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

# Build host-side tools from source (no apt/yum).
# These tools are used by the target cross-build flow.

OUT_ROOT="$(qnx_default_out_root)"
HOST_TOOLS_PREFIX="${OUT_ROOT}/host-tools"
WORK_DIR="${OUT_ROOT}/work/host-tools"
JOBS="$(qnx_default_jobs)"

CMAKE_VERSION="3.30.5"
NINJA_VERSION="1.12.1"
PKGCONF_VERSION="2.3.0"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      HOST_TOOLS_PREFIX="$2"
      shift 2
      ;;
    --work-dir)
      WORK_DIR="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --cmake-version)
      CMAKE_VERSION="$2"
      shift 2
      ;;
    --ninja-version)
      NINJA_VERSION="$2"
      shift 2
      ;;
    --pkgconf-version)
      PKGCONF_VERSION="$2"
      shift 2
      ;;
    *)
      qnx_error "Unknown argument: $1"
      exit 1
      ;;
  esac
done

for cmd in cmake curl tar make gcc g++; do
  qnx_require_cmd "${cmd}"
done

mkdir -p "${HOST_TOOLS_PREFIX}" "${WORK_DIR}"

build_pkgconf() {
  local archive="pkgconf-${PKGCONF_VERSION}.tar.xz"
  local url="https://distfiles.ariadne.space/pkgconf/${archive}"
  local src_dir="${WORK_DIR}/pkgconf-${PKGCONF_VERSION}"
  local build_dir="${WORK_DIR}/pkgconf-build"

  rm -rf "${src_dir}" "${build_dir}"
  curl -L "${url}" -o "${WORK_DIR}/${archive}"
  tar -xf "${WORK_DIR}/${archive}" -C "${WORK_DIR}"

  mkdir -p "${build_dir}"
  cmake -S "${src_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${HOST_TOOLS_PREFIX}"
  cmake --build "${build_dir}" -j"${JOBS}"
  cmake --install "${build_dir}"
}

build_ninja() {
  local archive="v${NINJA_VERSION}.tar.gz"
  local url="https://github.com/ninja-build/ninja/archive/refs/tags/${archive}"
  local src_dir="${WORK_DIR}/ninja-${NINJA_VERSION}"
  local build_dir="${WORK_DIR}/ninja-build"

  rm -rf "${src_dir}" "${build_dir}"
  curl -L "${url}" -o "${WORK_DIR}/ninja-${archive}"
  tar -xf "${WORK_DIR}/ninja-${archive}" -C "${WORK_DIR}"

  mkdir -p "${build_dir}"
  cmake -S "${src_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${HOST_TOOLS_PREFIX}"
  cmake --build "${build_dir}" -j"${JOBS}"
  cmake --install "${build_dir}"
}

build_cmake() {
  local archive="cmake-${CMAKE_VERSION}.tar.gz"
  local url="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${archive}"
  local src_dir="${WORK_DIR}/cmake-${CMAKE_VERSION}"

  rm -rf "${src_dir}"
  curl -L "${url}" -o "${WORK_DIR}/${archive}"
  tar -xf "${WORK_DIR}/${archive}" -C "${WORK_DIR}"

  pushd "${src_dir}" >/dev/null
  ./bootstrap --prefix="${HOST_TOOLS_PREFIX}" --parallel="${JOBS}"
  make -j"${JOBS}"
  make install
  popd >/dev/null
}

qnx_info "Build host tools (cmake/ninja/pkgconf)"
qnx_info "  host_tools_prefix=${HOST_TOOLS_PREFIX}"
qnx_info "  work_dir=${WORK_DIR}"
qnx_info "  versions: cmake=${CMAKE_VERSION} ninja=${NINJA_VERSION} pkgconf=${PKGCONF_VERSION}"
build_pkgconf
build_ninja
build_cmake
qnx_info "Host tools build complete."
echo "export PATH=${HOST_TOOLS_PREFIX}/bin:\$PATH"
