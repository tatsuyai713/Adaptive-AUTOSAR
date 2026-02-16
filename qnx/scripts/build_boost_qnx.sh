#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

ACTION="build"
if [[ $# -gt 0 ]] && [[ "$1" != --* ]]; then
  ACTION="$1"
  shift
fi

ARCH="$(qnx_default_arch)"
OUT_ROOT="$(qnx_default_out_root)"
JOBS="$(qnx_default_jobs)"

BOOST_VERSION="1.86.0"
BOOST_LIBRARIES="system,thread,filesystem,program_options,chrono,atomic,date_time,log"
INSTALL_PREFIX="${OUT_ROOT}/install/third_party/${ARCH}"
WORK_DIR="${OUT_ROOT}/work/boost/${ARCH}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"
      shift 2
      ;;
    --prefix)
      INSTALL_PREFIX="$2"
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
    --version)
      BOOST_VERSION="$2"
      shift 2
      ;;
    --libraries)
      BOOST_LIBRARIES="$2"
      shift 2
      ;;
    *)
      qnx_error "Unknown argument: $1"
      exit 1
      ;;
  esac
done

qnx_setup_env
qnx_require_cmd "tar"
qnx_require_cmd "make"
qnx_require_cmd "perl"

QCC_VARIANT="$(qnx_arch_to_qcc_variant "${ARCH}")"
BOOST_ARCH="$(qnx_arch_to_boost_arch "${ARCH}")"
BOOST_ADDRESS_MODEL="$(qnx_arch_to_address_model "${ARCH}")"
BOOST_VERSION_U="${BOOST_VERSION//./_}"
BOOST_ARCHIVE="boost_${BOOST_VERSION_U}.tar.gz"
BOOST_URL="https://archives.boost.io/release/${BOOST_VERSION}/source/${BOOST_ARCHIVE}"
SOURCE_DIR="${WORK_DIR}/boost_${BOOST_VERSION_U}"
ARCHIVE_PATH="${WORK_DIR}/${BOOST_ARCHIVE}"

if [[ "${ACTION}" == "clean" ]]; then
  rm -rf "${WORK_DIR}"
  qnx_info "Cleaned ${WORK_DIR}"
  exit 0
fi

mkdir -p "${WORK_DIR}" "${INSTALL_PREFIX}"

qnx_info "Build Boost for QNX"
qnx_info "  version=${BOOST_VERSION} arch=${ARCH} variant=${QCC_VARIANT}"
qnx_info "  prefix=${INSTALL_PREFIX} work_dir=${WORK_DIR}"

qnx_download_file "${BOOST_URL}" "${ARCHIVE_PATH}"
rm -rf "${SOURCE_DIR}"
tar -xf "${ARCHIVE_PATH}" -C "${WORK_DIR}"

cat > "${SOURCE_DIR}/user-config.jam" <<CFG
using gcc : qnx : q++ :
  <compileflags>-V${QCC_VARIANT}
  <compileflags>-D_QNX_SOURCE
  <cxxflags>-std=gnu++14
  <linkflags>-V${QCC_VARIANT}
  <linkflags>-Wl,-z,relro
  <linkflags>-Wl,-z,now
;
CFG

pushd "${SOURCE_DIR}" >/dev/null
./bootstrap.sh --prefix="${INSTALL_PREFIX}" --with-libraries="${BOOST_LIBRARIES}"
./b2 -j"${JOBS}" \
  --user-config=./user-config.jam \
  toolset=gcc-qnx \
  target-os=qnx \
  architecture="${BOOST_ARCH}" \
  address-model="${BOOST_ADDRESS_MODEL}" \
  threading=multi \
  threadapi=pthread \
  variant=release \
  runtime-link=shared \
  link=shared \
  install
popd >/dev/null

qnx_info "Boost installation complete: ${INSTALL_PREFIX}"
