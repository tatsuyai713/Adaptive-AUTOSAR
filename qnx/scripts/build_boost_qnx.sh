#!/usr/bin/env bash
set -euo pipefail

# QNX Boost cross-build script.
#
# Builds Boost with the qcc toolset for QNX targets.
#
# QNX SDP 8.0 uses lowercase 'qcc' while Boost's qcc.jam expects
# uppercase 'QCC'. We fix this by:
#   1) Patching qcc.jam to use lowercase 'qcc' as the default command
#   2) Adding -V<variant> to the archive action for cross-arch archiving
#
# Usage:
#   ./qnx/scripts/build_boost_qnx.sh install --arch aarch64le
#   ./qnx/scripts/build_boost_qnx.sh clean

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
INSTALL_PREFIX="${OUT_ROOT}/third_party"
WORK_DIR="${AUTOSAR_QNX_WORK_ROOT:-${REPO_ROOT}/out/qnx/work}/boost/${ARCH}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"; shift 2;;
    --prefix)
      INSTALL_PREFIX="$2"; shift 2;;
    --work-dir)
      WORK_DIR="$2"; shift 2;;
    --jobs)
      JOBS="$2"; shift 2;;
    --version)
      BOOST_VERSION="$2"; shift 2;;
    *)
      qnx_error "Unknown argument: $1"; exit 1;;
  esac
done

qnx_setup_env
qnx_require_cmd "tar"

QCC_VARIANT="$(qnx_arch_to_qcc_variant "${ARCH}")"
QCC_PATH="$(command -v qcc)"

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

mkdir -p "${WORK_DIR}"
sudo mkdir -p "${INSTALL_PREFIX}"
sudo chmod 777 "${INSTALL_PREFIX}"

qnx_info "Build Boost for QNX (static, toolset=qcc)"
qnx_info "  version=${BOOST_VERSION} arch=${ARCH} variant=${QCC_VARIANT}"
qnx_info "  prefix=${INSTALL_PREFIX} work_dir=${WORK_DIR}"
qnx_info "  qcc=${QCC_PATH}"

# Download & extract
if [[ ! -f "${ARCHIVE_PATH}" ]]; then
  qnx_download_file "${BOOST_URL}" "${ARCHIVE_PATH}"
fi
rm -rf "${SOURCE_DIR}"
tar -xzf "${ARCHIVE_PATH}" -C "${WORK_DIR}"

pushd "${SOURCE_DIR}" >/dev/null

# Patch qcc.jam for QNX SDP 8.0:
#  - The default command lookup searches for 'QCC' (uppercase), but QNX 8
#    uses 'qcc' (lowercase). Fix the init rule.
#  - The archive action needs -V<variant> for cross-arch.
QCC_JAM="tools/build/src/tools/qcc.jam"
if [[ -f "${QCC_JAM}" ]]; then
  qnx_info "Patching ${QCC_JAM} for QNX SDP 8.0 (lowercase qcc, arch=${QCC_VARIANT})"

  # Fix 1: Change default command from 'QCC' to 'qcc'
  sed -i.bak 's/common\.get-invocation-command qcc : QCC/common.get-invocation-command qcc : qcc/' "${QCC_JAM}"

  # Fix 2: Add -V<variant> to the archive action so the correct archiver is used
  sed -i 's|"$(CONFIG_COMMAND)" -A "$(<)" "$(>)"|"$(CONFIG_COMMAND)" -V'"${QCC_VARIANT}"' -A "$(<)" "$(>)"|' "${QCC_JAM}"

  rm -f "${QCC_JAM}.bak"
fi

# Create user-config.jam to explicitly point to the qcc compiler
cat > user-config.jam <<CFG
using qcc : : ${QCC_PATH} ;
CFG

./bootstrap.sh
./b2 install \
  --user-config=./user-config.jam \
  link=static \
  toolset=qcc \
  "cxxflags=-V${QCC_VARIANT} -std=gnu++14 -D_QNX_SOURCE" \
  target-os=qnxnto \
  --prefix="${INSTALL_PREFIX}" \
  --with-system --with-thread --with-filesystem \
  --with-program_options --with-chrono --with-atomic --with-date_time \
  -j"${JOBS}"

popd >/dev/null

qnx_info "Boost installation complete: ${INSTALL_PREFIX}"
