#!/usr/bin/env python3
"""
Apply QNX support patches to upstream COVESA/vsomeip CMakeLists.txt.

Based on the vsomeip-qnx reference fork (lixiaolia/vsomeip-qnx).
This script is idempotent: running it twice produces the same result.

Tested against vsomeip 3.6.1 CMakeLists.txt.

Usage:
    python3 apply_vsomeip_qnx_support.py /path/to/vsomeip/CMakeLists.txt
"""

import re
import sys


def patch_cmakelists(filepath):
    with open(filepath, "r") as f:
        content = f.read()

    # Skip if already patched
    if 'CMAKE_SYSTEM_NAME} MATCHES "QNX"' in content:
        print(f"[INFO] {filepath} already patched for QNX, skipping.")
        return

    print(f"[INFO] Applying QNX support patch to {filepath}")

    # 1) Add QNX OS block after FreeBSD block
    qnx_os_block = """
if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(OS "QNX")
    set(DL_LIBRARY "")
    set(EXPORTSYMBOLS "-Wl,-export-dynamic -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exportmap.gcc")
    set(NO_DEPRECATED "")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-D_GLIBCXX_USE_NANOSLEEP -lsocket -O -Wall -Wextra -Wformat -Wformat-security -fexceptions -fstrict-aliasing -fstack-protector -fasynchronous-unwind-tables -fno-omit-frame-pointer")
    set(Boost_USE_STATIC_LIBS ON)
endif (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
"""
    # Find end of FreeBSD block
    idx = content.find('endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")')
    if idx >= 0:
        end = content.index("\n", idx) + 1
        content = content[:end] + qnx_os_block + content[end:]
    else:
        # Fallback: insert before Options section
        options_marker = re.search(
            r"#{10,}\s*\n#\s*Options", content
        )
        if options_marker:
            content = (
                content[: options_marker.start()]
                + qnx_os_block
                + "\n"
                + content[options_marker.start() :]
            )
        else:
            print("[WARN] Could not find insertion point for QNX OS block")

    # 2) Wrap DLT section to disable on QNX
    old_dlt = """if (DISABLE_DLT)
set (VSOMEIP_ENABLE_DLT 0)
else ()
set (VSOMEIP_ENABLE_DLT 1)
endif ()"""
    new_dlt = """if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
set (VSOMEIP_ENABLE_DLT 0)
elseif (DISABLE_DLT)
set (VSOMEIP_ENABLE_DLT 0)
else ()
set (VSOMEIP_ENABLE_DLT 1)
endif ()"""
    content = content.replace(old_dlt, new_dlt, 1)

    # 3) Wrap systemd check for QNX
    content = content.replace(
        'pkg_check_modules(SystemD "libsystemd")',
        'if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")\npkg_check_modules(SystemD "libsystemd")\nendif()',
        1,
    )

    # 4) Wrap Boost find_package for QNX (use static libs)
    # Handle both 1.55 (older) and 1.66+ (3.6.x) minimum version
    boost_pattern = re.compile(
        r"find_package\(\s*Boost\s+(\d+\.\d+)\s+COMPONENTS\s+system\s+thread\s+filesystem\s+REQUIRED\s*\)"
    )
    boost_match = boost_pattern.search(content)
    if boost_match:
        old_boost = boost_match.group(0)
        new_boost = f"""if (${{CMAKE_SYSTEM_NAME}} MATCHES "QNX")
set(BOOST_SYSTEM_LIB ${{Boost_LIBRARY_DIR}}/libboost_system.a)
set(BOOST_THREAD_LIB ${{Boost_LIBRARY_DIR}}/libboost_thread.a)
set(BOOST_FILESYSTEM_LIB ${{Boost_LIBRARY_DIR}}/libboost_filesystem.a)
else ()
{old_boost}
endif ()"""
        content = content.replace(old_boost, new_boost, 1)

    # 5) DLT pkg_check wrapped for QNX
    old_dlt_pkg = """if(VSOMEIP_ENABLE_DLT EQUAL 1)
pkg_check_modules(DLT "automotive-dlt >= 2.11")"""
    new_dlt_pkg = """if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
# QNX: DLT disabled
elseif(VSOMEIP_ENABLE_DLT EQUAL 1)
pkg_check_modules(DLT "automotive-dlt >= 2.11")"""
    content = content.replace(old_dlt_pkg, new_dlt_pkg, 1)

    # 6) Fix USE_RT for QNX (handle both with and without space before parens)
    use_rt_pattern = re.compile(r'set\s*\(\s*USE_RT\s+"rt"\s*\)')
    use_rt_match = use_rt_pattern.search(content)
    if use_rt_match:
        old_use_rt = use_rt_match.group(0)
        new_use_rt = f"""if (${{CMAKE_SYSTEM_NAME}} MATCHES "QNX")
    set(USE_RT "")
else()
    {old_use_rt}
endif()"""
        content = content.replace(old_use_rt, new_use_rt, 1)

    # 7) Replace target_link_libraries with QNX-aware versions
    # In 3.6.x, lines use ${SystemD_LIBRARIES} instead of ${OS_LIBS}
    # We need to match both old and new patterns
    link_pattern = re.compile(
        r"target_link_libraries\([^)]*\$\{Boost_LIBRARIES\}[^)]*\)"
    )

    def qnx_link_replace(match):
        original = match.group(0)
        # Replace ${Boost_LIBRARIES} with static .a references
        boost_static = "${BOOST_SYSTEM_LIB} ${BOOST_THREAD_LIB} ${BOOST_FILESYSTEM_LIB}"
        qnx_line = original.replace("${Boost_LIBRARIES}", boost_static)
        return (
            'if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")\n\t'
            + qnx_line
            + "\nelse()\n\t"
            + original
            + "\nendif()"
        )

    # Apply to all target_link_libraries with Boost
    content = link_pattern.sub(qnx_link_replace, content)

    # 8) If VSOMEIP_BOOST_HELPER version selection exists (older versions), patch it
    if "VSOMEIP_BOOST_HELPER" in content and 'Using boost version' in content:
        old_boost_ver = 'message( STATUS "Using boost version: ${VSOMEIP_BOOST_VERSION}" )'
        new_boost_ver = """if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
\tset(VSOMEIP_BOOST_HELPER implementation/helper/1.70)
else()
message( STATUS "Using boost version: ${VSOMEIP_BOOST_VERSION}" )"""
        content = content.replace(old_boost_ver, new_boost_ver, 1)

        # Close the else/endif for boost helper selection
        lines = content.split("\n")
        in_boost_helper = False
        insert_idx = -1
        for i, line in enumerate(lines):
            if "VSOMEIP_BOOST_HELPER implementation/helper/1.55" in line:
                in_boost_helper = True
            if in_boost_helper and line.strip() == "endif()":
                insert_idx = i
                break
        if insert_idx > 0:
            lines.insert(insert_idx + 1, "endif()")
            content = "\n".join(lines)

    with open(filepath, "w") as f:
        f.write(content)

    print(f"[INFO] QNX support patch applied to {filepath}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <CMakeLists.txt>", file=sys.stderr)
        sys.exit(1)
    patch_cmakelists(sys.argv[1])
