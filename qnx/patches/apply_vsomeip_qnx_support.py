#!/usr/bin/env python3
"""
Apply QNX support patches to upstream COVESA/vsomeip.

Patches:
  1. CMakeLists.txt - OS detection, DLT/systemd disabling, static Boost libs
  2. asio_tcp_socket.hpp / asio_udp_socket.hpp - remove QNX from SO_BINDTODEVICE
     guard (QNX does not implement SO_BINDTODEVICE)

Based on the vsomeip-qnx reference fork (lixiaolia/vsomeip-qnx).
This script is idempotent: running it twice produces the same result.

Tested against vsomeip 3.6.1.

Usage:
    python3 apply_vsomeip_qnx_support.py /path/to/vsomeip/CMakeLists.txt
"""

import os
import re
import sys


# Idempotency marker we inject into every file we touch.
MARKER = "# QNX_AUTOSAR_PATCHED"


def _is_patched(content: str) -> bool:
    return MARKER in content


def patch_cmakelists(filepath: str) -> None:
    with open(filepath, "r") as f:
        content = f.read()

    if _is_patched(content):
        print(f"[INFO] {filepath} already patched for QNX, skipping.")
        return

    print(f"[INFO] Applying QNX support patch to {filepath}")

    # vsomeip 3.6.1 already ships a minimal QNX block inside the CMakeLists.
    # We only need to apply the patches that are still missing.

    # 1) Ensure USE_RT is empty on QNX (librt does not exist on QNX).
    use_rt_pattern = re.compile(r'set\s*\(\s*USE_RT\s+"rt"\s*\)')
    use_rt_match = use_rt_pattern.search(content)
    if use_rt_match:
        old_use_rt = use_rt_match.group(0)
        if 'if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")' not in content[
            max(0, use_rt_match.start() - 200) : use_rt_match.start()
        ]:
            new_use_rt = (
                'if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")\n'
                "    set(USE_RT \"\")\nelse()\n"
                f"    {old_use_rt}\n"
                "endif()"
            )
            content = content.replace(old_use_rt, new_use_rt, 1)

    # 2) Wrap DLT section to disable on QNX.
    old_dlt = (
        "if (DISABLE_DLT)\n"
        "set (VSOMEIP_ENABLE_DLT 0)\n"
        "else ()\n"
        "set (VSOMEIP_ENABLE_DLT 1)\n"
        "endif ()"
    )
    new_dlt = (
        'if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")\n'
        "set (VSOMEIP_ENABLE_DLT 0)\n"
        "elseif (DISABLE_DLT)\n"
        "set (VSOMEIP_ENABLE_DLT 0)\n"
        "else ()\n"
        "set (VSOMEIP_ENABLE_DLT 1)\n"
        "endif ()"
    )
    if old_dlt in content:
        content = content.replace(old_dlt, new_dlt, 1)

    # 3) Wrap systemd check for QNX.
    if 'if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")' not in content:
        content = content.replace(
            'pkg_check_modules(SystemD "libsystemd")',
            'if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")\npkg_check_modules(SystemD "libsystemd")\nendif()',
            1,
        )

    # 4) Wrap Boost find_package for QNX to use static libs.
    boost_pattern = re.compile(
        r"find_package\(\s*Boost\s+(\d+\.\d+)\s+COMPONENTS\s+system\s+thread\s+filesystem\s+REQUIRED\s*\)"
    )
    boost_match = boost_pattern.search(content)
    if boost_match:
        old_boost = boost_match.group(0)
        # Only wrap if not already wrapped
        ctx_before = content[max(0, boost_match.start() - 100) : boost_match.start()]
        if "BOOST_SYSTEM_LIB" not in ctx_before:
            new_boost = (
                'if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")\n'
                "set(BOOST_SYSTEM_LIB ${Boost_LIBRARY_DIR}/libboost_system.a)\n"
                "set(BOOST_THREAD_LIB ${Boost_LIBRARY_DIR}/libboost_thread.a)\n"
                "set(BOOST_FILESYSTEM_LIB ${Boost_LIBRARY_DIR}/libboost_filesystem.a)\n"
                "else ()\n"
                f"{old_boost}\n"
                "endif ()"
            )
            content = content.replace(old_boost, new_boost, 1)

    # 5) target_link_libraries: replace ${Boost_LIBRARIES} with static .a on QNX.
    link_pattern = re.compile(
        r"target_link_libraries\([^)]*\$\{Boost_LIBRARIES\}[^)]*\)"
    )

    def qnx_link_replace(match: re.Match) -> str:
        original = match.group(0)
        # Already wrapped?
        if MARKER in original:
            return original
        boost_static = (
            "${BOOST_SYSTEM_LIB} ${BOOST_THREAD_LIB} ${BOOST_FILESYSTEM_LIB}"
        )
        qnx_line = original.replace("${Boost_LIBRARIES}", boost_static)
        return (
            'if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")\n\t'
            + qnx_line
            + "\nelse()\n\t"
            + original
            + "\nendif()"
        )

    content = link_pattern.sub(qnx_link_replace, content)

    # Stamp with our marker so we won't re-patch.
    content = MARKER + "\n" + content

    with open(filepath, "w") as f:
        f.write(content)

    print(f"[INFO] QNX support patch applied to {filepath}")


def patch_asio_socket_header(filepath: str) -> None:
    """Fix SO_BINDTODEVICE guard: QNX does not have SO_BINDTODEVICE.

    The base class declares bind_to_device() and can_read_fd_flags() as pure
    virtual, so we must provide implementations for QNX:
      - bind_to_device: return false (SO_BINDTODEVICE not supported on QNX)
      - can_read_fd_flags: use fcntl(F_GETFD) which works on QNX
    """
    if not os.path.exists(filepath):
        return

    with open(filepath, "r") as f:
        content = f.read()

    if _is_patched(content):
        print(f"[INFO] {filepath} already patched for QNX, skipping.")
        return

    old_guard = '#if defined(__linux__) || defined(__QNX__)'
    # Replace the combined guard with: Linux uses SO_BINDTODEVICE, QNX uses stubs
    new_guard_block = (
        '#if defined(__linux__)\n'
        '    [[nodiscard]] bool bind_to_device(std::string const& _device) override {\n'
        '        return setsockopt(socket_.native_handle(), SOL_SOCKET, SO_BINDTODEVICE, _device.c_str(), static_cast<socklen_t>(_device.size()))\n'
        '                != -1;\n'
        '    }\n'
        '    [[nodiscard]] bool can_read_fd_flags() override { return fcntl(socket_.native_handle(), F_GETFD) != -1; }\n'
        '#elif defined(__QNX__)\n'
        '    // QNX: SO_BINDTODEVICE is not supported; provide stubs for pure virtuals.\n'
        '    [[nodiscard]] bool bind_to_device(std::string const& /*_device*/) override { return false; }\n'
        '    [[nodiscard]] bool can_read_fd_flags() override { return fcntl(socket_.native_handle(), F_GETFD) != -1; }\n'
        '#endif'
    )

    if old_guard not in content:
        print(f"[INFO] {filepath}: SO_BINDTODEVICE guard not found, skipping.")
        return

    # Find the full #if...#endif block to replace
    start = content.index(old_guard)
    # Find closing #endif
    end_tag = '#endif'
    end = content.index(end_tag, start) + len(end_tag)
    old_block = content[start:end]

    print(f"[INFO] Patching {filepath}: replacing SO_BINDTODEVICE block with Linux+QNX stubs")
    content = content[:start] + new_guard_block + content[end:]
    # Stamp
    content = f"// {MARKER}\n" + content

    with open(filepath, "w") as f:
        f.write(content)


def patch_wrappers_qnx(filepath: str) -> None:
    """Rewrite wrappers_qnx.cpp for QNX SDP 8.0.

    The upstream file uses sys/sockmsg.h which was removed in QNX 8.0.
    In QNX 8.0, accept4() is provided natively by libsocket, so we can
    drop the custom implementation and just use the native one.
    """
    if not os.path.exists(filepath):
        return

    with open(filepath, "r") as f:
        content = f.read()

    if _is_patched(content):
        print(f"[INFO] {filepath} already patched for QNX, skipping.")
        return

    if "sys/sockmsg.h" not in content:
        print(f"[INFO] {filepath}: sys/sockmsg.h not referenced, skipping.")
        return

    print(f"[INFO] Patching {filepath}: replacing custom accept4() with QNX 8.0 native accept4()")

    new_content = f"""\
// {MARKER}
// wrappers_qnx.cpp - patched for QNX SDP 8.0
// QNX 8.0 provides accept4() natively in libsocket; sys/sockmsg.h is gone.
// We keep the __wrap_socket/__wrap_accept/__wrap_open wrappers but use the
// native accept4() instead of the custom message-passing implementation.
#ifdef __QNX__

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cerrno>
#include <pthread.h>
#include <cstdlib>
#include <string.h>
#include <unistd.h>

/*
 * These definitions MUST remain in the global namespace.
 */
extern "C" {{
/*
 * The real socket(2), renamed by GCC.
 */
int __real_socket(int domain, int type, int protocol) noexcept;

/*
 * Overrides socket(2) to set SOCK_CLOEXEC by default.
 */
int __wrap_socket(int domain, int type, int protocol) noexcept {{
    return __real_socket(domain, type | SOCK_CLOEXEC, protocol);
}}

/*
 * Overrides accept(2) to set SOCK_CLOEXEC by default.
 * QNX 8.0 provides accept4() natively in libsocket.
 */
int __wrap_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {{
    return accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);
}}

/*
 * The real open(2), renamed by GCC.
 */
int __real_open(const char* pathname, int flags, mode_t mode);

/*
 * Overrides open(2) to set O_CLOEXEC by default.
 */
int __wrap_open(const char* pathname, int flags, mode_t mode) {{
    return __real_open(pathname, flags | O_CLOEXEC, mode);
}}
}}

#endif
"""
    with open(filepath, "w") as f:
        f.write(new_content)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <CMakeLists.txt>", file=sys.stderr)
        sys.exit(1)

    cmake_file = sys.argv[1]
    vsomeip_root = os.path.dirname(cmake_file)

    patch_cmakelists(cmake_file)

    # Patch C++ headers with QNX-incompatible Linux socket options
    for header in [
        "implementation/endpoints/include/asio_tcp_socket.hpp",
        "implementation/endpoints/include/asio_udp_socket.hpp",
    ]:
        patch_asio_socket_header(os.path.join(vsomeip_root, header))

    # Patch wrappers_qnx.cpp for QNX SDP 8.0 (sys/sockmsg.h removed)
    patch_wrappers_qnx(
        os.path.join(vsomeip_root, "implementation/utility/src/wrappers_qnx.cpp")
    )
