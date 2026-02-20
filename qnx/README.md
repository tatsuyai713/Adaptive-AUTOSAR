# QNX 8.0 Cross-Build

This directory contains scripts for cross-compiling this repository using QNX SDP 8.0.

## Target Environment

- Host: Linux (x86\_64)
- Target: QNX 8.0 (`aarch64le` / `x86_64`)
- Prerequisites: `QNX_HOST` and `QNX_TARGET` set (source `qnxsdp-env.sh`)

---

## Quick Start (Batch Build)

> **Note:** Middleware is installed to `/opt/qnx`. Run `sudo mkdir -p /opt/qnx && sudo chmod 777 /opt/qnx` beforehand.

```bash
# Load QNX SDK environment
source ~/qnx800/qnxsdp-env.sh

# Build and install all components
./qnx/scripts/build_all_qnx.sh install

# Build + create deploy image in one step
./qnx/scripts/build_all_qnx.sh install --target-ip 192.168.1.100
```

---

## Detailed Steps

### 1. Environment Setup

```bash
source ~/qnx800/qnxsdp-env.sh
source qnx/env/qnx800.env.example   # copy and adjust values as needed
```

### 2. Check Host Tools

```bash
./qnx/scripts/install_dependency.sh
```

- Does not use `apt` or other package managers
- Verifies required commands exist and builds host tools if necessary

### 3. Cross-Build Middleware

```bash
# Install all middleware at once
./qnx/scripts/build_libraries_qnx.sh all install

# Individual builds
./qnx/scripts/build_libraries_qnx.sh cyclonedds install --disable-shm
./qnx/scripts/build_libraries_qnx.sh vsomeip install
./qnx/scripts/build_libraries_qnx.sh all clean
```

Install location: `/opt/qnx/` (boost → `third_party/`, iceoryx / cyclonedds / vsomeip)

### 4. Cross-Build AUTOSAR AP Runtime

```bash
./qnx/scripts/build_autosar_ap_qnx.sh install
```

Install location: `/opt/qnx/autosar_ap/aarch64le/`

### 5. Cross-Build user\_apps

```bash
./qnx/scripts/build_user_apps_qnx.sh build
```

### 6. Batch Build

```bash
./qnx/scripts/build_all_qnx.sh install
```

### Out of Memory During Build

```bash
export AUTOSAR_QNX_JOBS=2
./qnx/scripts/build_libraries_qnx.sh all install
```

---

## 7. Create Deploy Image

Package build artifacts and shared library dependencies into an image file:

```bash
./qnx/scripts/create_qnx_deploy_image.sh --target-ip <QNX_TARGET_IP>
```

**Output** (`out/qnx/deploy/`):

| File | Description |
| --- | --- |
| `autosar_ap-aarch64le.tar.gz` | Extractable package (default) |
| `autosar_ap-aarch64le.ifs` | QNX IFS image (with `--no-tar` option) |

**Contents:**

- `bin/`: 14 AUTOSAR AP platform daemons + 13 user apps
- `lib/`: vsomeip3 / CycloneDDS / iceoryx shared libraries
- `etc/vsomeip_routing.json`, `vsomeip_client.json`: vsomeip configuration
- `startup.sh`, `env.sh`: startup and environment configuration scripts

Key options:

| Option | Description |
| --- | --- |
| `--target-ip <ip>` | Unicast IP written into vsomeip configuration |
| `--no-tar` | Create IFS image only (no tar.gz) |
| `--no-ifs` | Create tar.gz only (no mkifs required) |
| `--output-dir <dir>` | Change output directory |

---

## 8. Deploy to QNX Target (SSH Transfer / Mount)

### 8-1. SCP tar.gz and Extract (Recommended)

```bash
./qnx/scripts/deploy_to_qnx_target.sh --host 192.168.1.100
```

The script automatically:

1. SCP transfers `out/qnx/deploy/autosar_ap-aarch64le.tar.gz`
2. Connects via SSH and extracts to `/autosar` on the target
3. Updates vsomeip unicast IP to the target IP

### 8-2. Transfer IFS Image and Mount via devb-ram

```bash
./qnx/scripts/deploy_to_qnx_target.sh \
  --host 192.168.1.100 \
  --image out/qnx/deploy/autosar_ap-aarch64le.ifs \
  --image-type ifs
```

Operations on the target:

```sh
devb-ram unit=9 capacity=<sectors> &        # create RAM block device
dd if=autosar_ap-aarch64le.ifs of=/dev/ram9 # write IFS
mount -t ifs /dev/ram9 /autosar             # mount
```

### 8-3. Deploy Options Reference

| Option | Description | Default |
| --- | --- | --- |
| `--host <ip>` | QNX target IP or hostname | Required |
| `--user <user>` | SSH user | `root` |
| `--port <port>` | SSH port | `22` |
| `--image <file>` | Image file to transfer | Auto-detected |
| `--image-type tar\|ifs` | Image type | Auto-detected |
| `--remote-dir <path>` | Extraction directory on target | `/autosar` |
| `--target-ip <ip>` | vsomeip unicast IP (defaults to `--host` value) | — |
| `--dry-run` | Print commands without executing | — |

---

## 9. Startup on Target

After deployment, SSH into the target and run:

```sh
# Start all daemons
/autosar/startup.sh --all

# Start only vsomeip routing manager
/autosar/startup.sh --routing-only

# Stop
/autosar/startup.sh --stop

# Manually start a user app (set LD_LIBRARY_PATH via env.sh)
. /autosar/env.sh
VSOMEIP_CONFIGURATION=/autosar/etc/vsomeip_client.json \
  /autosar/bin/autosar_user_minimal_runtime
```

---

## Key Scripts

| Script | Description |
| --- | --- |
| `qnx/scripts/build_libraries_qnx.sh` | Build Boost / iceoryx / CycloneDDS / vSomeIP |
| `qnx/scripts/build_autosar_ap_qnx.sh` | Build AUTOSAR AP runtime |
| `qnx/scripts/build_user_apps_qnx.sh` | Build user apps |
| `qnx/scripts/build_all_qnx.sh` | Run all of the above in order |
| `qnx/scripts/create_qnx_deploy_image.sh` | Create deploy image (tar.gz / IFS) |
| `qnx/scripts/deploy_to_qnx_target.sh` | SSH transfer and extract/mount on target |
| `qnx/cmake/toolchain_qnx800.cmake` | CMake toolchain for QNX |

## Output Locations (Default)

```text
/opt/qnx/                        ← middleware install location
  third_party/                   Boost etc.
  iceoryx/
  cyclonedds/
  vsomeip/
  autosar_ap/<arch>/             AUTOSAR AP platform daemons
out/qnx/
  work/build/user_apps-<arch>/   user app build artifacts
  deploy/                        deploy image output
    autosar_ap-<arch>.tar.gz
    autosar_ap-<arch>.ifs
```

---

## QNX vs Linux Differences (Porting Notes)

QNX is a POSIX-compliant RTOS but differs fundamentally from Linux in kernel architecture,
with several incompatibilities when porting.

### OS Architecture Differences

| Aspect | Linux | QNX |
| --- | --- | --- |
| Kernel structure | Monolithic kernel | Microkernel (Neutrino) |
| Driver location | Kernel space | Userspace processes (`devb-*`, `io-pkt`, etc.) |
| Network stack | Built into kernel | `io-pkt` userspace process |
| Filesystem | VFS + ext4/xfs etc. | `io-blk` + QNX6 fs, or `fs-*` processes |
| IPC | Sockets / pipes / shared memory | Message passing (`MsgSend/MsgReceive`) |
| Real-time | Approximated with PREEMPT\_RT patch | Deterministic scheduling by design |

### Library and Header Differences

The following incompatibilities were actually addressed in this project:

#### pthread (threads)

| Item | Linux | QNX 8.0 |
| --- | --- | --- |
| pthread location | `libpthread.so` (standalone library) | **Integrated into libc** (`libpthread.a` absent) |
| Link flag | `-lpthread` | Not required (but cmake adds it automatically) |
| Workaround | — | Auto-generate empty `libpthread.a` stub |

#### Sockets / Networking

| Header / Feature | Linux | QNX 8.0 | Workaround |
| --- | --- | --- | --- |
| `sys/socket.h` | Included in libc | Requires `libsocket` (`-lsocket`) | Add `-lsocket` to toolchain |
| `SO_BINDTODEVICE` | Supported | **Unsupported** | Stub implementation in vsomeip (always `false`) |
| `sys/sockmsg.h` | N/A | Present through QNX 7, **removed in 8.0** | Replace with native `accept4()` implementation |
| `epoll` (`sys/epoll.h`) | Supported | **Absent** | `poll()`-based alternative implementation |

#### Signals

| Feature | Linux | QNX 8.0 | Workaround |
| --- | --- | --- | --- |
| `sys/signalfd.h` | Supported | **Absent** (Linux-specific) | `pipe()`-based stop signal implementation |
| `SA_RESTART` | Supported | **Not implemented** (commented out in header) | Add `-DSA_RESTART=0x0040` compile flag |
| `SIGRTMIN` / `SIGRTMAX` | Supported | Limited | Avoid via `ENABLE_SIGNAL_HANDLING=OFF` in vsomeip |

#### Serial Communication

| Feature | Linux | QNX 8.0 | Workaround |
| --- | --- | --- | --- |
| `asm/termbits.h` | Present | **Absent** | Change to `<termios.h>` |
| `CRTSCTS` (hardware flow control) | Present | **Absent** | Alternative using `IHFLOW \| OHFLOW` |

#### Type Definitions

| Type | Linux (`sys/types.h`) | QNX 8.0 | Workaround |
| --- | --- | --- | --- |
| `u_int8_t`, `u_int32_t`, etc. | BSD-compatible types present | **Absent** | Replace with `uint8_t`, `uint32_t` (stdint.h) |

### Build Tool Differences

| Item | Linux (aarch64 cross) | QNX aarch64le |
| --- | --- | --- |
| Compiler | `aarch64-linux-gnu-g++` | `ntoaarch64-g++` (no `le`/`be` suffix) |
| Archiver | `aarch64-linux-gnu-ar` | `ntoaarch64-ar` |
| Dynamic linker | `/lib/ld-linux-aarch64.so.1` | `/usr/lib/ldqnx-64.so.2` |
| Macro | `__linux__` | `__QNX__`, `__QNXNTO__` |
| Target OS name (cmake) | `Linux` | `QNX` |

#### Boost b2 Build Notes

```bash
# BAD: qcc toolset segfaults in b2 5.x
./b2 toolset=qcc target-os=qnxnto ...    # crashes

# GOOD: register as gcc-ntoaarch64
using gcc : ntoaarch64 : ntoaarch64-g++ ;
./b2 toolset=gcc-ntoaarch64 target-os=qnxnto ...
```

Since vsomeip links against shared libraries (`.so`), Boost must be built **with `-fPIC`**.

### Filesystem / Path Differences

| Item | Linux | QNX |
| --- | --- | --- |
| `/proc` | Rich (`/proc/cpuinfo`, etc.) | Limited (`/proc/<pid>/`, etc.) |
| `/dev/loop*` | Present (loop device) | **Absent** → use `devb-ram` instead |
| `/dev/null`, `/dev/zero` | Present | Present |
| Config file location | `/etc/` | `/etc/` or `/tmp/` |
| Runtime data | `/var/run/` | `/dev/shmem/` or `/tmp/` |

### vsomeip Functional Limitations

| Feature | Linux | QNX 8.0 (this project) |
| --- | --- | --- |
| NIC binding (`SO_BINDTODEVICE`) | Supported | **Disabled** (stub) |
| SOME/IP TCP/UDP communication | ○ | ○ |
| Service Discovery (multicast) | ○ | ○ (requires `io-pkt`) |
| UNIX domain sockets (local IPC) | ○ | ○ |
| DLT logging | ○ | **Disabled** (`DISABLE_DLT=ON`) |
| SIGTERM handler | ○ | **Disabled** (`ENABLE_SIGNAL_HANDLING=OFF`) |

---

## AUTOSAR AP and Middleware Functional Differences

In the QNX target build, several features are disabled or restricted compared to the Linux version
due to POSIX incompatibilities and OS architecture differences.

---

### vsomeip (SOME/IP Middleware)

vsomeip cross-builds and links successfully for QNX 8.0, and basic SOME/IP communication works.
However, the following features are disabled or restricted:

#### Build-Time Flags

| cmake flag | QNX setting | Reason |
| --- | --- | --- |
| `ENABLE_SIGNAL_HANDLING` | **OFF** | `sys/signalfd.h` does not exist in QNX |
| `DISABLE_DLT` | **ON** | QNX has no DLT daemon |
| `ENABLE_SHM` | OFF (default) | iceoryx SHM transport requires separate RouDi startup |

#### Detailed Feature Differences

| Feature | Linux | QNX (this project) | Impact |
| --- | --- | --- | --- |
| **NIC binding** (`SO_BINDTODEVICE`) | Works | **Stub (always false)** | Cannot bind to specific IF in multi-NIC env; unicast IP communication works |
| **SIGTERM / SIGINT auto-handling** | routing manager handles automatically | **Disabled** (`ENABLE_SIGNAL_HANDLING=OFF`) | Application must implement shutdown logic |
| **DLT logging** | vsomeip → DLT daemon | **Disabled** (console only) | vsomeip logs go to stdout/stderr |
| **SOME/IP TCP/UDP** | Works | **Works** | No difference |
| **Service Discovery (multicast)** | Works | **Works** (requires `io-pkt`) | Network driver is userspace process |
| **UNIX domain sockets (IPC)** | Works | **Works** | vsomeip client ↔ routing manager communication |
| **SHM local transport** | Works | **Unconfirmed** (requires RouDi) | iceoryx integration requires separate verification |

#### vsomeip Configuration Notes

- On QNX, the `unicast` field must explicitly specify the **actual target IP address** (not `0.0.0.0`, etc.)
- The IP specified with `--target-ip` during deploy image creation is automatically written to `vsomeip_routing.json` and `vsomeip_client.json`
- Routing manager name is fixed as `autosar_vsomeip_routing_manager` (same in `startup.sh`)

---

### CycloneDDS

CycloneDDS is built for QNX in UDP network communication mode.

| Feature | Linux | QNX (this project) | Status |
| --- | --- | --- | --- |
| **UDP multicast Discovery** | Works | **Works** (requires `io-pkt`) | DDS Participant Discovery possible |
| **UDP unicast communication** | Works | **Works** | Pub/Sub possible |
| **iceoryx SHM transport** | Works (requires RouDi) | **Limited** | `ddsi_shm_transport.h` iceoryx headers require versioned include path |
| **TCP channel** | Works | Works | Via QNX socket implementation |
| **idlc (IDL compiler)** | Use host build | Use host build | Cross-build unnecessary (code gen runs on host) |

---

### iceoryx (Shared Memory IPC)

iceoryx is built and installed for QNX, but the RouDi daemon must be started at runtime.

| Item | Linux | QNX | Description |
| --- | --- | --- | --- |
| **Shared memory path** | `/dev/shm/` | `/dev/shmem/` | QNX POSIX shared memory implementation |
| **RouDi daemon** | Required | Required (same) | Included as `bin/autosar_iceoryx_roudi` |
| **Include path** | `include/iceoryx/` | `include/iceoryx/v2.0.6/` | Placed in versioned subdirectory |
| **Zero-copy IPC** | Works | Works | mmap-based SHM available on QNX too |

---

### ara::com (AUTOSAR Communication API)

| Backend | QNX Build | Verified | Notes |
| --- | --- | --- | --- |
| **vsomeip (SOME/IP)** | **○** | ○ (requires routing manager) | SO_BINDTODEVICE restriction |
| **iceoryx (SHM)** | **○** | Requires RouDi | For inter-process communication |
| **CycloneDDS (DDS)** | **○** | ○ (requires io-pkt) | UDP/multicast Discovery |

---

### AUTOSAR AP Platform Daemons

14 daemons are built and installed to `/opt/qnx/autosar_ap/aarch64le/bin/`:

| Daemon | Function | QNX Status |
| --- | --- | --- |
| `adaptive_autosar` | AUTOSAR AP main process (SM/Exec integration) | ○ |
| `autosar_vsomeip_routing_manager` | vsomeip Routing Manager | ○ (must start first) |
| `autosar_dlt_daemon` | DLT (Diagnostic Log and Trace) daemon | △ (QNX DLT receiver unconfirmed) |
| `autosar_watchdog_supervisor` | Watchdog monitoring | ○ |
| `autosar_network_manager` | Network management (NM module) | ○ |
| `autosar_time_sync_daemon` | Time sync (NTP/PTP) | ○ |
| `autosar_sm_state_daemon` | State Management state notification | ○ |
| `autosar_iam_policy_loader` | IAM policy loader | ○ |
| `autosar_persistency_guard` | Persistency data protection | ○ |
| `autosar_ucm_daemon` | Update and Config Management | ○ |
| `autosar_user_app_monitor` | User app monitoring | ○ |
| Other (tools) | Log viewer, diagnostic tools, etc. | Use as needed |

#### Recommended Startup Order

```sh
# 1. vsomeip routing manager (all other processes depend on this)
VSOMEIP_CONFIGURATION=/autosar/etc/vsomeip_routing.json \
  /autosar/bin/autosar_vsomeip_routing_manager &
sleep 1

# 2. Platform services (order does not matter)
/autosar/bin/autosar_dlt_daemon &
/autosar/bin/autosar_watchdog_supervisor &
/autosar/bin/autosar_network_manager &
/autosar/bin/autosar_time_sync_daemon &
/autosar/bin/autosar_sm_state_daemon &

# 3. Main process
/autosar/bin/adaptive_autosar &
```

`startup.sh --all` runs this sequence automatically.

---

### Feature Difference Summary vs. Linux Native

| Feature Area | Linux Native | QNX Target |
| --- | --- | --- |
| SOME/IP (vsomeip) | Full operation | **Basic operation** (SO_BINDTODEVICE, DLT, signal handler disabled) |
| DDS (CycloneDDS) | Full operation | **Basic operation** (UDP unicast, multicast) |
| SHM IPC (iceoryx) | Full operation (requires RouDi) | **Works** (uses `/dev/shmem`, requires RouDi) |
| ara::com Pub/Sub | Full operation | **Works** (backend selectable) |
| DLT log collection | Supported | **Console output only** (DLT daemon included; QNX operation unconfirmed) |
| Signal handling (SIGTERM, etc.) | Automatic | **Application must implement** |
| Multicast SD | Supported | **Supported** (requires `io-pkt` userspace driver) |

---

## Known Limitations Summary

| Item | Details | Workaround |
| --- | --- | --- |
| `SO_BINDTODEVICE` | QNX unsupported | Stub implementation (always `false`) |
| `SA_RESTART` | Undefined in header | Add `-DSA_RESTART=0x0040` compile flag |
| `libpthread` | Integrated into libc in QNX 8.0 | Auto-generate empty `.a` stub |
| `sys/signalfd.h` | Linux-specific | `pipe()`-based alternative implementation |
| `sys/sockmsg.h` | Removed in QNX 8.0 | Replace with native `accept4()` |
| `CRTSCTS` | Use `IHFLOW`/`OHFLOW` on QNX | Conditional compilation alternative |
| `epoll` | Linux-specific | `poll()`-based alternative implementation |
| `u_int*_t` types | No BSD-compatible types in QNX | Replace with `uint*_t` (stdint.h) |
| Boost b2 + qcc | Segfaults in b2 5.x | Use `gcc-ntoaarch64` toolset |
