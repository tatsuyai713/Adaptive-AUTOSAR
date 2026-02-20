#!/bin/bash
################################################################################
# Run CI Tests Locally
# This script replicates the CI pipeline tests on a local Linux machine
################################################################################

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
VSOMEIP_PREFIX="${VSOMEIP_PREFIX:-/opt/vsomeip}"
ICEORYX_PREFIX="${ICEORYX_PREFIX:-/opt/iceoryx}"
CYCLONEDDS_PREFIX="${CYCLONEDDS_PREFIX:-/opt/cyclonedds}"
SKIP_DEPS="${SKIP_DEPS:-false}"
SKIP_MIDDLEWARE="${SKIP_MIDDLEWARE:-false}"
SKIP_BUILD="${SKIP_BUILD:-false}"
SKIP_TESTS="${SKIP_TESTS:-false}"
SKIP_ARXML="${SKIP_ARXML:-false}"
SKIP_INSTALL="${SKIP_INSTALL:-false}"

# Print functions
print_step() {
    echo -e "${BLUE}==>${NC} ${GREEN}$1${NC}"
}

print_info() {
    echo -e "${YELLOW}INFO:${NC} $1"
}

print_error() {
    echo -e "${RED}ERROR:${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

# Usage
usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Run CI tests locally on Linux (replicates GitHub Actions CI pipeline).

Options:
    --build-type TYPE          Build type (Debug|Release). Default: Debug
    --vsomeip-prefix PATH      vsomeip installation prefix. Default: /opt/vsomeip
    --iceoryx-prefix PATH      iceoryx installation prefix. Default: /opt/iceoryx
    --cyclonedds-prefix PATH   CycloneDDS installation prefix. Default: /opt/cyclonedds
    --skip-deps                Skip system dependency installation
    --skip-middleware          Skip middleware installation
    --skip-build               Skip build step
    --skip-tests               Skip unit tests
    --skip-arxml               Skip ARXML generator checks
    --skip-install             Skip install validation and user app build
    -h, --help                 Show this help message

Environment variables:
    BUILD_TYPE                 Same as --build-type
    VSOMEIP_PREFIX             Same as --vsomeip-prefix
    ICEORYX_PREFIX             Same as --iceoryx-prefix
    CYCLONEDDS_PREFIX          Same as --cyclonedds-prefix

Examples:
    # Run all CI tests (requires sudo for dependencies)
    $(basename "$0")

    # Skip dependency installation (already installed)
    $(basename "$0") --skip-deps --skip-middleware

    # Run only unit tests
    $(basename "$0") --skip-deps --skip-middleware --skip-arxml --skip-install

    # Build in Release mode
    BUILD_TYPE=Release $(basename "$0")

EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --vsomeip-prefix)
            VSOMEIP_PREFIX="$2"
            shift 2
            ;;
        --iceoryx-prefix)
            ICEORYX_PREFIX="$2"
            shift 2
            ;;
        --cyclonedds-prefix)
            CYCLONEDDS_PREFIX="$2"
            shift 2
            ;;
        --skip-deps)
            SKIP_DEPS=true
            shift
            ;;
        --skip-middleware)
            SKIP_MIDDLEWARE=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --skip-arxml)
            SKIP_ARXML=true
            shift
            ;;
        --skip-install)
            SKIP_INSTALL=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Verify we're in the right directory
if [[ ! -f "${REPO_ROOT}/CMakeLists.txt" ]]; then
    print_error "Cannot find CMakeLists.txt. Are you in the correct directory?"
    exit 1
fi

# Export LD_LIBRARY_PATH for tests
export LD_LIBRARY_PATH="${VSOMEIP_PREFIX}/lib:${CYCLONEDDS_PREFIX}/lib:${ICEORYX_PREFIX}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

print_step "Starting CI tests locally"
print_info "Build type: ${BUILD_TYPE}"
print_info "Repository: ${REPO_ROOT}"
print_info "vsomeip prefix: ${VSOMEIP_PREFIX}"
print_info "iceoryx prefix: ${ICEORYX_PREFIX}"
print_info "CycloneDDS prefix: ${CYCLONEDDS_PREFIX}"
echo ""

# Step 1: Install system dependencies
if [[ "${SKIP_DEPS}" == "false" ]]; then
    print_step "Step 1: Installing system dependencies"
    if [[ $EUID -ne 0 ]]; then
        print_info "This step requires sudo privileges"
        sudo "${SCRIPT_DIR}/install_dependency.sh"
    else
        "${SCRIPT_DIR}/install_dependency.sh"
    fi
    print_success "System dependencies installed"
    echo ""
else
    print_info "Skipping system dependency installation"
fi

# Step 2: Build and install middleware dependencies
if [[ "${SKIP_MIDDLEWARE}" == "false" ]]; then
    print_step "Step 2: Building and installing middleware dependencies"
    print_info "This may take 30-60 minutes..."
    
    "${SCRIPT_DIR}/install_middleware_stack.sh" \
        --vsomeip-prefix "${VSOMEIP_PREFIX}" \
        --iceoryx-prefix "${ICEORYX_PREFIX}" \
        --cyclonedds-prefix "${CYCLONEDDS_PREFIX}" \
        --skip-system-deps
    
    if [[ $EUID -ne 0 ]]; then
        sudo ldconfig
    else
        ldconfig
    fi
    
    print_success "Middleware dependencies installed"
    echo ""
else
    print_info "Skipping middleware installation"
fi

# Step 3: Configure CMake
if [[ "${SKIP_BUILD}" == "false" ]]; then
    print_step "Step 3: Configuring CMake"
    
    rm -rf "${REPO_ROOT}/build"
    mkdir -p "${REPO_ROOT}/build"
    
    cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/build" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -Dbuild_tests=ON \
        -DARA_COM_USE_VSOMEIP=ON \
        -DARA_COM_USE_ICEORYX=ON \
        -DARA_COM_USE_CYCLONEDDS=ON \
        -DARA_COM_ENABLE_ARXML_CODEGEN=ON \
        -DAUTOSAR_AP_BUILD_SAMPLES=OFF \
        -DVSOMEIP_PREFIX="${VSOMEIP_PREFIX}" \
        -DICEORYX_PREFIX="${ICEORYX_PREFIX}" \
        -DCYCLONEDDS_PREFIX="${CYCLONEDDS_PREFIX}"
    
    print_success "CMake configuration complete"
    echo ""
fi

# Step 4: Build
if [[ "${SKIP_BUILD}" == "false" ]]; then
    print_step "Step 4: Building project"
    
    NPROC=$(nproc)
    print_info "Building with ${NPROC} parallel jobs..."
    
    cmake --build "${REPO_ROOT}/build" --config "${BUILD_TYPE}" -j"${NPROC}"
    
    print_success "Build complete"
    echo ""
else
    print_info "Skipping build"
fi

# Step 5: Run unit tests
if [[ "${SKIP_TESTS}" == "false" ]]; then
    print_step "Step 5: Running unit tests"
    
    ctest --test-dir "${REPO_ROOT}/build" --output-on-failure -C "${BUILD_TYPE}"
    
    print_success "Unit tests passed"
    echo ""
else
    print_info "Skipping unit tests"
fi

# Step 6: Run ARXML generator checks
if [[ "${SKIP_ARXML}" == "false" ]]; then
    print_step "Step 6: Running ARXML generator checks"
    
    # Check if Python is available
    if ! command -v python3 &> /dev/null; then
        print_error "python3 is required for ARXML checks"
        exit 1
    fi
    
    TMP_DIR="/tmp/ci_tests_local_$$"
    mkdir -p "${TMP_DIR}"
    
    print_info "Generating pubsub_vsomeip.arxml..."
    python3 "${REPO_ROOT}/tools/arxml_generator/generate_arxml.py" \
        --input "${REPO_ROOT}/tools/arxml_generator/examples/pubsub_vsomeip.yaml" \
        --output "${TMP_DIR}/ci_pubsub_vsomeip.arxml" \
        --overwrite \
        --print-summary
    
    print_info "Generating pubsub_multi_transport.arxml..."
    python3 "${REPO_ROOT}/tools/arxml_generator/generate_arxml.py" \
        --input "${REPO_ROOT}/tools/arxml_generator/examples/pubsub_multi_transport.yaml" \
        --output "${TMP_DIR}/ci_pubsub_multi_transport.arxml" \
        --allow-extensions \
        --overwrite \
        --print-summary
    
    print_info "Generating ARA COM binding header..."
    python3 "${REPO_ROOT}/tools/arxml_generator/generate_ara_com_binding.py" \
        --input "${TMP_DIR}/ci_pubsub_vsomeip.arxml" \
        --output "${TMP_DIR}/ci_vehicle_status_manifest_binding.h" \
        --namespace sample::vehicle_status::generated \
        --provided-service-short-name VehicleStatusProviderInstance \
        --provided-event-group-short-name VehicleStatusEventGroup
    
    # Verify generated files
    for file in ci_pubsub_vsomeip.arxml ci_pubsub_multi_transport.arxml ci_vehicle_status_manifest_binding.h; do
        if [[ ! -s "${TMP_DIR}/${file}" ]]; then
            print_error "Generated file is empty or missing: ${file}"
            exit 1
        fi
    done
    
    print_success "ARXML generator checks passed"
    print_info "Generated files in: ${TMP_DIR}"
    echo ""
else
    print_info "Skipping ARXML generator checks"
fi

# Step 7: Validate install split and user app build
if [[ "${SKIP_INSTALL}" == "false" ]]; then
    print_step "Step 7: Validating install split and user app build"
    
    INSTALL_PREFIX="/tmp/autosar_ap_local_$$"
    BUILD_INSTALL_DIR="${REPO_ROOT}/build-install-ci-local"
    BUILD_USER_DIR="${REPO_ROOT}/build-user-apps-ci-local"
    BUILD_USER_RUN_DIR="${REPO_ROOT}/build-user-apps-ci-local-run"
    
    # Clean up old build directories to avoid CMake cache conflicts
    rm -rf "${BUILD_INSTALL_DIR}" "${BUILD_USER_DIR}" "${BUILD_USER_RUN_DIR}"
    
    print_info "Installing to: ${INSTALL_PREFIX}"
    
    "${SCRIPT_DIR}/build_and_install_autosar_ap.sh" \
        --prefix "${INSTALL_PREFIX}" \
        --build-dir "${BUILD_INSTALL_DIR}" \
        --build-type Release \
        --source-dir "${REPO_ROOT}" \
        --with-platform-app
    
    print_info "Building user apps..."
    "${SCRIPT_DIR}/build_user_apps_from_opt.sh" \
        --prefix "${INSTALL_PREFIX}" \
        --source-dir "${INSTALL_PREFIX}/user_apps" \
        --build-dir "${BUILD_USER_DIR}"
    
    print_info "Building and running user apps..."
    "${SCRIPT_DIR}/build_user_apps_from_opt.sh" \
        --prefix "${INSTALL_PREFIX}" \
        --source-dir "${INSTALL_PREFIX}/user_apps" \
        --build-dir "${BUILD_USER_RUN_DIR}" \
        --run
    
    print_success "Install validation and user app build passed"
    print_info "Installed to: ${INSTALL_PREFIX}"
    echo ""
else
    print_info "Skipping install validation"
fi

# Summary
echo ""
print_step "CI Tests Summary"
echo -e "${GREEN}✓ All CI tests passed successfully!${NC}"
echo ""
print_info "Test artifacts:"
[[ "${SKIP_BUILD}" == "false" ]] && echo "  - Build directory: ${REPO_ROOT}/build"
[[ "${SKIP_ARXML}" == "false" ]] && echo "  - ARXML files: /tmp/ci_tests_local_*/"
[[ "${SKIP_INSTALL}" == "false" ]] && echo "  - Install directory: /tmp/autosar_ap_local_*/"
echo ""
print_info "To clean up temporary files, run:"
echo "  rm -rf /tmp/ci_tests_local_* /tmp/autosar_ap_local_*"
echo ""
