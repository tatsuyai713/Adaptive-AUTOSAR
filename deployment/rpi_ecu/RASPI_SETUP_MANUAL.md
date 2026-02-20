# Raspberry Pi In-Vehicle ECU Setup Manual

This manual describes the procedure for operating a Raspberry Pi as an AUTOSAR Adaptive Platform-based
prototype in-vehicle ECU. Including detailed gPTP (IEEE 802.1AS) configuration, the manual is written
so that first-time users can follow it step by step from start to finish.

---

## Table of Contents

1. [Introduction and Verified Configurations](#1-introduction-and-verified-configurations)
2. [Hardware Requirements](#2-hardware-requirements)
3. [Installing Raspberry Pi OS](#3-installing-raspberry-pi-os)
4. [Basic System Configuration](#4-basic-system-configuration)
5. [gPTP (IEEE 802.1AS) Configuration — Detailed](#5-gptp-ieee-80211as-configuration--detailed)
6. [CAN Bus Configuration](#6-can-bus-configuration)
7. [Middleware Installation](#7-middleware-installation)
8. [Building and Installing AUTOSAR AP](#8-building-and-installing-autosar-ap)
9. [systemd Service Configuration](#9-systemd-service-configuration)
10. [Starting and Verifying Services](#10-starting-and-verifying-services)
11. [Adding User Applications](#11-adding-user-applications)
12. [Verification Command Reference](#12-verification-command-reference)
13. [Troubleshooting](#13-troubleshooting)

---

## 1. Introduction and Verified Configurations

### What This Manual Achieves

| Feature | Status |
|---------|--------|
| SOME/IP service communication (Pub/Sub, RPC) | ✓ Verified |
| DDS Pub/Sub (Cyclone DDS) | ✓ Verified |
| iceoryx zero-copy communication | ✓ Verified |
| CAN bus receive (SocketCAN) | ✓ Verified |
| gPTP/PTP hardware time sync | ✓ Verified (requires PTP-capable NIC) |
| NTP time sync | ✓ Verified |
| Platform Health Monitoring (PHM) | ✓ Verified |
| Persistent storage (Persistency) | ✓ Verified |
| Diagnostic UDS service | ✓ Verified |
| Network Management (NM) | ✓ Verified |
| Software update (UCM) | ✓ Verified |
| IAM policy management | ✓ Verified |
| DoIP diagnostic communication | ✓ Verified |
| QNX 8.0 cross-build | ✓ See separate QNX README |

### Verified Hardware

- **Raspberry Pi 4 Model B** (4GB / 8GB RAM recommended)
- **Raspberry Pi 5** (8GB RAM recommended)
- Raspberry Pi 3 Model B+ also works but build times will be longer

---

## 2. Hardware Requirements

### Required

| Item | Recommended Spec |
|------|-----------------|
| SBC | Raspberry Pi 4B / 5 |
| RAM | 4GB or more (4GB required for on-device builds) |
| Storage | microSD 32GB+ (Class 10 or higher) or SSD |
| OS | Raspberry Pi OS (64-bit, Bookworm/Bullseye) |
| Network | Wired Ethernet (required for gPTP) |

### Additional Requirements for gPTP

| Item | Description |
|------|-------------|
| PTP-capable NIC | Raspberry Pi 4/5 built-in Ethernet **does not support PTP hardware timestamps** |
| Recommended USB Ethernet | Adapter with AX88179 chipset (e.g., UGREEN USB-C to Ethernet) |
| PCIe NIC (Pi 5) | PTP-capable NIC such as Intel I210, I350 (via HAT) |
| Alternative | Software timestamps (lower accuracy but functional) |

> **Note:** `linuxptp` works even with NICs that don't support PTP hardware timestamps.
> It can participate in gPTP using software timestamp mode, but accuracy is approximately ±100μs.
> A PTP-capable NIC is required for automotive requirements (±1μs).

### Additional Requirements for CAN Bus

| Item | Options |
|------|---------|
| CAN HAT | Waveshare RS485 CAN HAT / PiCAN2 / UCAN Board |
| USB-CAN | PCAN-USB / CANDLELIGHT / socketcan_slcan |
| Bitrate | 500 kbps / 1 Mbps (choose per application) |
| CAN FD | Requires compatible HAT (with MCP2517FD / TCAN4550) |

---

## 3. Installing Raspberry Pi OS

### 3.1 Download and Install Raspberry Pi Imager

Install Raspberry Pi Imager on your PC (Windows/macOS/Linux):

```
https://www.raspberrypi.com/software/
```

### 3.2 Write the OS Image

1. Launch Raspberry Pi Imager
2. **Choose OS** → `Raspberry Pi OS (other)` → select `Raspberry Pi OS Lite (64-bit)`
   - No GUI needed. The Lite version handles everything from build to execution.
3. **Choose Storage** → select the microSD card
4. Click the gear icon (⚙) to open advanced settings:
   - Hostname: set to `raspi-ecu` or similar
   - Enable SSH: enabled
   - Username/password: as desired (e.g., `pi` / `raspberry`)
   - WiFi: configure if needed (wired recommended)
   - Locale: set as appropriate
5. Click **Write**

### 3.3 First Boot and SSH Connection

```bash
# SSH from PC
ssh pi@raspi-ecu.local

# Or connect by IP address
ssh pi@192.168.x.x
```

---

## 4. Basic System Configuration

### 4.1 System Update

```bash
sudo apt update && sudo apt full-upgrade -y
sudo reboot
```

### 4.2 Install Required Packages

```bash
sudo apt install -y \
  build-essential \
  cmake \
  git \
  python3 \
  python3-pip \
  python3-yaml \
  libssl-dev \
  libboost-all-dev \
  can-utils \
  iproute2 \
  net-tools \
  ethtool \
  curl \
  wget \
  unzip \
  pkg-config
```

### 4.3 Expand Swap Space (to prevent out-of-memory during builds)

If RAM is less than 4GB or memory issues occur during builds, expand swap:

```bash
# Check current swap size
free -h

# Expand swap to 4GB
sudo dphys-swapfile swapoff
sudo sed -i 's/CONF_SWAPSIZE=.*/CONF_SWAPSIZE=4096/' /etc/dphys-swapfile
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
free -h  # verify
```

### 4.4 Clone the Git Repository

```bash
# Place in home directory
cd ~
git clone https://github.com/your-org/Adaptive-AUTOSAR.git
cd Adaptive-AUTOSAR
```

---

## 5. gPTP (IEEE 802.1AS) Configuration — Detailed

gPTP (Generalized Precision Time Protocol, IEEE 802.1AS) is a high-precision time synchronization
protocol for automotive Ethernet. The AUTOSAR Adaptive Platform accesses gPTP time through the
`ara::tsync` API.

### 5.1 Install linuxptp

```bash
# Install from package (available on Raspberry Pi OS Bookworm)
sudo apt install -y linuxptp

# Verify version
ptp4l --version
phc2sys --version
```

If the version is old or running on non-Bookworm, build from source:

```bash
# Build from source
sudo apt install -y libnl-3-dev libnl-genl-3-dev
git clone https://git.code.sf.net/p/linuxptp/code linuxptp
cd linuxptp
make -j$(nproc)
sudo make install
cd ..
```

### 5.2 Check NIC PTP Support

```bash
# Check NIC interface name
ip link show

# Check PTP hardware timestamp support
ethtool -T eth0
# or
ethtool -T enp3s0  # for USB Ethernet, etc.
```

Sample output (with hardware timestamp support):
```
Time stamping parameters for eth0:
Capabilities:
    hardware-transmit     (SOF_TIMESTAMPING_TX_HARDWARE)
    software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)
    hardware-receive      (SOF_TIMESTAMPING_RX_HARDWARE)
    software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)
    hardware-raw-clock    (SOF_TIMESTAMPING_RAW_HARDWARE)
PTP Hardware Clock: 0          # <--- this number corresponds to /dev/ptp0
Hardware Transmit Timestamp Modes:
    off                   (HWTSTAMP_TX_OFF)
    on                    (HWTSTAMP_TX_ON)
Hardware Receive Filter Modes:
    none                  (HWTSTAMP_FILTER_NONE)
    all                   (HWTSTAMP_FILTER_ALL)
```

> **When hardware timestamps are not supported** (e.g., Raspberry Pi 4 built-in Ethernet):
> `PTP Hardware Clock: none` is displayed. ptp4l operates in software timestamp mode.

### 5.3 Create gPTP Configuration Files

#### ptp4l configuration for AUTOSAR automotive profile

`/etc/linuxptp/automotive-master.cfg` (for gPTP Grand Master):

```ini
# /etc/linuxptp/automotive-master.cfg
# AUTOSAR automotive gPTP (IEEE 802.1AS) - Grand Master configuration

[global]
transportSpecific           1          # gPTP (802.1AS) transport-specific field
clockClass                  6          # Locked to primary reference (GNSS/GPS ideal)
clockAccuracy               0x21       # within 100 ns (GPS-locked)
offsetScaledLogVariance     0x4E5D
priority1                   128
priority2                   128
domainNumber                0
logAnnounceInterval         0          # 1 second
logSyncInterval             -3         # 125 ms (AUTOSAR requirement)
logMinDelayReqInterval      0
announceReceiptTimeout      3
clock_servo                 linreg
pi_proportional_const       0.0
pi_integral_const           0.0
summary_interval            0
time_stamping               hardware   # use 'software' if no HW timestamps
```

`/etc/linuxptp/automotive-slave.cfg` (for gPTP Slave / ECU):

```ini
# /etc/linuxptp/automotive-slave.cfg
# AUTOSAR automotive gPTP (IEEE 802.1AS) - Slave (ECU) configuration

[global]
transportSpecific           1
slaveOnly                   1
clockClass                  255
clockAccuracy               0xFE
offsetScaledLogVariance     0xFFFF
priority1                   255
priority2                   255
domainNumber                0
logAnnounceInterval         0
logSyncInterval             -3
logMinDelayReqInterval      0
announceReceiptTimeout      3
clock_servo                 linreg
pi_proportional_const       0.0
pi_integral_const           0.0
summary_interval            0
time_stamping               hardware   # 'software' if no HW timestamps
```

Append a NIC interface section at the end of each file (example: `eth0`):

```ini
[eth0]
delay_mechanism             P2P
network_transport           L2
```

### 5.4 Software Timestamp Mode Configuration (for non-PTP NICs)

For NICs without PTP hardware timestamp support (e.g., Raspberry Pi 4 built-in Ethernet):

```ini
# /etc/linuxptp/automotive-slave-sw.cfg
# Software timestamp mode (accuracy: approx. ±100μs)

[global]
transportSpecific           1
slaveOnly                   1
clockClass                  255
clockAccuracy               0xFE
offsetScaledLogVariance     0xFFFF
priority1                   255
priority2                   255
domainNumber                0
logAnnounceInterval         0
logSyncInterval             -3
logMinDelayReqInterval      0
announceReceiptTimeout      3
clock_servo                 linreg
summary_interval            0
time_stamping               software    # ← software timestamps

[eth0]
delay_mechanism             P2P
network_transport           L2
```

### 5.5 ptp4l systemd Service Configuration

```bash
sudo mkdir -p /etc/linuxptp
```

Create `/etc/systemd/system/ptp4l-automotive.service`:

```ini
[Unit]
Description=IEEE 802.1AS (gPTP) - ptp4l automotive profile
Documentation=man:ptp4l(8)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/sbin/ptp4l \
    -f /etc/linuxptp/automotive-slave.cfg \
    -i eth0 \
    -m \
    --summary_interval=60
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable ptp4l-automotive.service
sudo systemctl start ptp4l-automotive.service

# Check status
sudo systemctl status ptp4l-automotive.service
```

### 5.6 phc2sys — Sync PTP Hardware Clock to System Clock

If a PTP hardware timestamp-capable NIC is available, synchronize the PTP Hardware Clock
(PHC `/dev/ptp0`) to the system clock:

```bash
# First check the PHC device number
ethtool -T eth0 | grep "PTP Hardware Clock"
# e.g.: PTP Hardware Clock: 0  → /dev/ptp0
```

Create `/etc/systemd/system/phc2sys-automotive.service`:

```ini
[Unit]
Description=Sync PTP Hardware Clock to System Clock (phc2sys)
Documentation=man:phc2sys(8)
After=ptp4l-automotive.service
Requires=ptp4l-automotive.service

[Service]
Type=simple
ExecStart=/usr/sbin/phc2sys \
    -s /dev/ptp0 \
    -c CLOCK_REALTIME \
    -n 0 \
    -O 0 \
    -R 256 \
    -u 60 \
    -m
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable phc2sys-automotive.service
sudo systemctl start phc2sys-automotive.service

# Check status
sudo systemctl status phc2sys-automotive.service
```

### 5.7 Verify gPTP Sync Status

```bash
# Check sync status in ptp4l logs
sudo journalctl -u ptp4l-automotive.service -f

# Normal sync output example:
# ptp4l[123.456]: rms 45 max 123 freq -456 +/- 78 delay  890 +/-  12
# ptp4l[123.456]: master offset       -45 s2 freq  +1234 path delay       890

# Check phc2sys logs
sudo journalctl -u phc2sys-automotive.service -f

# Check sync offset
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
sudo pmc -u -b 0 'GET TIME_STATUS_NP'

# Check offset between PHC and CLOCK_REALTIME
phc_ctl /dev/ptp0 cmp
```

### 5.8 Chrony Coexistence (NTP Fallback)

For environments where gPTP is unavailable, or when NTP is desired as a fallback:

```bash
sudo apt install -y chrony
```

Edit `/etc/chrony/chrony.conf`:

```conf
# NTP servers (for fallback)
pool ntp.pool.ntp.org iburst

# Add PTP hardware clock as source (higher priority)
refclock PHC /dev/ptp0 poll 0 dpoll -2 offset 0 precision 1e-9 trust prefer

# Local clock (last resort fallback)
local stratum 10

# Allow time step on startup
makestep 0.1 3

# Sync RTC
rtcsync
```

```bash
sudo systemctl enable chrony
sudo systemctl restart chrony

# Check chrony status
chronyc sources -v
chronyc tracking
```

---

## 6. CAN Bus Configuration

### 6.1 CAN HAT Setup (MCP2515-based SPI CAN HAT)

#### Add to `/boot/firmware/config.txt`:

```bash
sudo nano /boot/firmware/config.txt
```

```ini
# SPI CAN HAT (MCP2515) - adjust for your board
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
# For Waveshare CAN HAT:
# dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25
# For PiCAN2:
# dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25
```

```bash
sudo reboot
```

### 6.2 Verify and Configure SocketCAN Interface

```bash
# Check CAN interface
ip link show type can

# Configure using project script (500 kbps)
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000

# Manual configuration:
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0
ip link show can0  # verify it is UP
```

### 6.3 Virtual CAN (for testing without hardware)

```bash
# Enable virtual CAN module
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Using project script:
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan

# Test (use two terminals)
# Receiver:
candump vcan0 &
# Sender:
cansend vcan0 123#DEADBEEF
```

### 6.4 Persist CAN Interface

To automatically configure the CAN interface after reboot:

```bash
sudo nano /etc/systemd/network/can0.network
```

```ini
[Match]
Name=can0

[CAN]
BitRate=500K
```

```bash
sudo systemctl enable systemd-networkd
sudo systemctl restart systemd-networkd
```

---

## 7. Middleware Installation

### 7.1 Batch Install (Recommended)

Use the project script to install all middleware at once.
**Build time is approximately 30–90 minutes** (on Raspberry Pi 4).

```bash
cd ~/Adaptive-AUTOSAR
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

This script installs:
- **vSomeIP** → `/opt/vsomeip` (SOME/IP middleware)
- **iceoryx** → `/opt/iceoryx` (zero-copy shared memory IPC)
- **Cyclone DDS** → `/opt/cyclonedds` (DDS middleware)

### 7.2 Individual Install (if issues arise)

```bash
# Base dependencies only
sudo ./scripts/install_dependency.sh

# vSomeIP only
sudo ./scripts/install_vsomeip.sh

# iceoryx only
sudo ./scripts/install_iceoryx.sh

# Cyclone DDS only
sudo ./scripts/install_cyclonedds.sh
```

### 7.3 Verify Installation

```bash
ls /opt/vsomeip/lib/      # libvsomeip3.so should exist
ls /opt/iceoryx/lib/      # libiceoryx_posh.a etc. should exist
ls /opt/cyclonedds/lib/   # libddsc.so should exist
```

---

## 8. Building and Installing AUTOSAR AP

### 8.1 Integrated Build Script (Recommended)

```bash
cd ~/Adaptive-AUTOSAR
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware
```

**Estimated build time:**
- Raspberry Pi 4 (4GB): approximately 15–30 minutes
- Raspberry Pi 5 (8GB): approximately 8–15 minutes

### 8.2 Verify Installation

```bash
ls /opt/autosar_ap/lib/         # AUTOSAR AP libraries
ls /opt/autosar_ap/include/     # header files
ls /opt/autosar_ap/bin/         # binaries (adaptive_autosar, etc.)
ls /opt/autosar_ap/user_apps_build/  # user app binaries
```

### 8.3 Configure Library PATH

```bash
echo '/opt/autosar_ap/lib' | sudo tee /etc/ld.so.conf.d/autosar_ap.conf
echo '/opt/vsomeip/lib' | sudo tee -a /etc/ld.so.conf.d/autosar_ap.conf
echo '/opt/iceoryx/lib' | sudo tee -a /etc/ld.so.conf.d/autosar_ap.conf
echo '/opt/cyclonedds/lib' | sudo tee -a /etc/ld.so.conf.d/autosar_ap.conf
sudo ldconfig
```

---

## 9. systemd Service Configuration

### 9.1 Install Services

```bash
cd ~/Adaptive-AUTOSAR
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

### 9.2 Service Dependency and Startup Order

```
network-online.target
         │
         ▼
autosar-iox-roudi.service        ← iceoryx RouDi (shared memory IPC)
         │
         ▼
autosar-vsomeip-routing.service  ← SOME/IP routing manager
autosar-can-manager.service      ← CAN interface management
         │
         ▼
autosar-time-sync.service
autosar-persistency-guard.service
autosar-iam-policy.service
autosar-network-manager.service
autosar-dlt.service
         │
         ▼
autosar-platform-app.service     ← platform core (EM/SM/PHM/Diag)
         │
         ▼
autosar-exec-manager.service     ← launch user apps (bringup.sh)
         │
         ├── autosar-watchdog.service
         ├── autosar-user-app-monitor.service
         ├── autosar-ptp-time-provider.service
         ├── autosar-ntp-time-provider.service
         ├── autosar-sm-state.service
         ├── autosar-ucm.service
         └── autosar-ecu-full-stack.service
```

### 9.3 Edit Environment Configuration Files

Runtime parameters for each service are configured in `/etc/default/autosar-*`.

#### SOME/IP Routing Manager

```bash
sudo nano /etc/default/autosar-vsomeip-routing
```

```bash
VSOMEIP_CONFIGURATION=/opt/autosar_ap/etc/vsomeip-rpi.json
VSOMEIP_APPLICATION_NAME=routingmanagerd
```

#### CAN Interface Manager

```bash
sudo nano /etc/default/autosar-can-manager
```

```bash
AUTOSAR_CAN_IFNAME=can0
AUTOSAR_CAN_BITRATE=500000
AUTOSAR_CAN_TRIGGER_DIR=/run/autosar/can_triggers
```

---

## 10. Starting and Verifying Services

### 10.1 Initial Startup Procedure

```bash
# 1. iceoryx RouDi (start first)
sudo systemctl start autosar-iox-roudi.service
sleep 2

# 2. SOME/IP routing manager
sudo systemctl start autosar-vsomeip-routing.service
sleep 2

# 3. CAN interface manager
sudo systemctl start autosar-can-manager.service

# 4. Core services
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-network-manager.service

# 5. Platform application
sudo systemctl start autosar-platform-app.service
sleep 3

# 6. Execution manager (launches user apps)
sudo systemctl start autosar-exec-manager.service

# 7. Monitoring and auxiliary services
sudo systemctl start autosar-watchdog.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-ptp-time-provider.service
sudo systemctl start autosar-ntp-time-provider.service
```

### 10.2 Batch Status Check

```bash
# Check all AUTOSAR service statuses
systemctl status 'autosar-*.service' --no-pager | grep -E '(●|Active:|●)'

# Real-time log monitoring
sudo journalctl -u 'autosar-*' -f

# Check individual services
sudo systemctl status autosar-vsomeip-routing.service --no-pager
sudo systemctl status autosar-platform-app.service --no-pager
```

---

## 11. Adding User Applications

### 11.1 Edit bringup.sh

Add applications to `bringup.sh` to run at ECU startup:

```bash
sudo nano /etc/autosar/bringup.sh
```

```bash
#!/usr/bin/env bash
# /etc/autosar/bringup.sh

APP_DIR="${AUTOSAR_USER_APPS_BUILD_DIR:-/opt/autosar_ap/user_apps_build}"

# Basic launch (PID monitoring only)
launch_app "my_app" "${APP_DIR}/my_application" --param1 value1

# Launch with heartbeat monitoring
launch_app_with_heartbeat \
  "my_hb_app" \
  "/run/autosar/my_hb_app.hb" \
  5000 \
  "${APP_DIR}/my_hb_application"

# Full managed mode (PHM integration + auto-restart policy)
launch_app_managed \
  "vehicle_monitor" \
  "/vehicle/monitor/1" \
  "/run/autosar/vehicle_monitor.hb" \
  3000 \
  5 \
  60000 \
  "${APP_DIR}/vehicle_monitor_app"
```

### 11.2 Restart Service After Registering App

```bash
sudo systemctl restart autosar-exec-manager.service
cat /run/autosar/user_apps_registry.csv
```

### 11.3 Build User Applications

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir user_apps \
  --build-dir build-user-apps
```

---

## 12. Verification Command Reference

### 12.1 Full Profile Verification Script

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary

# With physical CAN
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0 \
  --require-platform-binary
```

### 12.2 Individual Communication Checks

```bash
./tools/sample_runner/verify_ara_com_someip_transport.sh
./tools/sample_runner/verify_ara_com_zerocopy_transport.sh
./tools/sample_runner/verify_ara_com_dds_transport.sh
./tools/sample_runner/verify_ecu_someip_dds.sh
./tools/sample_runner/verify_ecu_can_to_dds.sh
```

### 12.3 Check gPTP Sync Status

```bash
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
sudo pmc -u -b 0 'GET GRANDMASTER_SETTINGS_NP'
sudo pmc -u -b 0 'GET PORT_DATA_SET'
sudo pmc -u -b 0 'GET TIME_STATUS_NP'
sudo journalctl -u ptp4l-automotive.service --since "1 min ago" --no-pager | tail -20
```

### 12.4 CAN Bus Verification

```bash
candump can0
candump can0 123:7FF  # specific ID only
ip -details link show can0
ip -s link show can0
cansend can0 123#DEADBEEF
```

---

## 13. Troubleshooting

### 13.1 Build Errors

#### Out of memory during build

```bash
# Reduce parallel build count
MAKEFLAGS="-j2" sudo ./scripts/build_and_install_rpi_ecu_profile.sh ...
```

#### Middleware not found

```bash
ls /opt/vsomeip/lib/libvsomeip3*
ls /opt/iceoryx/lib/libiceoryx*
sudo ldconfig
ldconfig -p | grep vsomeip
ldconfig -p | grep iceoryx
```

### 13.2 Service Startup Errors

#### autosar-iox-roudi fails to start

```bash
sudo journalctl -u autosar-iox-roudi.service --no-pager -n 50

# Remove stale iceoryx shared memory
sudo rm -rf /dev/shm/iceoryx_mgmt
sudo rm -rf /tmp/roudi_*

sudo systemctl restart autosar-iox-roudi.service
```

#### autosar-vsomeip-routing fails to start

```bash
sudo journalctl -u autosar-vsomeip-routing.service --no-pager -n 50
cat /etc/default/autosar-vsomeip-routing
ls /opt/autosar_ap/etc/vsomeip-rpi.json
```

### 13.3 gPTP Issues

#### ptp4l does not sync

```bash
# Check if gPTP Grand Master is running
sudo tcpdump -i eth0 ether proto 0x88f7 -v

# Test network connectivity
ping <grand_master_IP>

# ptp4l debug log
sudo ptp4l -f /etc/linuxptp/automotive-slave.cfg -i eth0 -m -q 2>&1 | head -50
```

#### Large offset (when using software timestamps)

Software timestamps provide accuracy of approximately ±100μs — this is normal operation.
Use a PTP-capable NIC if higher precision is required.

### 13.4 CAN Bus Issues

#### CAN interface does not come UP

```bash
lsmod | grep -E "(mcp251|can|spi)"
dmesg | grep -iE "(mcp|can|spi)" | tail -20
cat /boot/firmware/config.txt | grep -E "(spi|can|mcp)"
ls /dev/spidev*
```

#### Cannot receive CAN frames

```bash
ip -details link show can0 | grep bitrate
candump -e can0  # dump including error frames
ip link show can0 | grep -i "error"
# BUSOFF state → restart needed
sudo ip link set down can0
sudo ip link set up can0
```

### 13.5 Logging and Debug

```bash
# All AUTOSAR service logs
sudo journalctl -u 'autosar-*' --since "10 min ago" --no-pager

# Real-time log for specific service
sudo journalctl -u autosar-platform-app.service -f

# Kernel messages (driver-related)
sudo dmesg -w | grep -iE "(can|spi|ptp|eth)"

# SOME/IP debug logging (vSomeIP)
export VSOMEIP_LOG_LEVEL=debug
/opt/autosar_ap/bin/adaptive_autosar
```

---

## Appendix A: Recommended PTP-Capable NICs for Raspberry Pi 4/5

| NIC | Chip | PTP Support | Connection | Notes |
|-----|------|-------------|------------|-------|
| Intel I210-T1 | I210 | Hardware | PCIe (Pi 5 HAT) | Highest accuracy |
| Intel I350-T2 | I350 | Hardware | PCIe (Pi 5 HAT) | 2-port |
| Realtek RTL8156 | RTL8156 | Software | USB 3.0 | General USB Ethernet |
| ASIX AX88179 | AX88179 | Software | USB 3.0 | Widely available |

> Raspberry Pi 4/5 built-in Ethernet (BCM54213) does not support PTP hardware timestamps.
> Using I210/I350 via the Pi 5 PCIe slot enables high-precision gPTP.

---

## Appendix B: Service List and Environment Variable Reference

| Service | Environment File | Key Variables |
|---------|-----------------|---------------|
| autosar-iox-roudi | (none) | — |
| autosar-vsomeip-routing | `/etc/default/autosar-vsomeip-routing` | `VSOMEIP_CONFIGURATION` |
| autosar-can-manager | `/etc/default/autosar-can-manager` | `AUTOSAR_CAN_IFNAME`, `AUTOSAR_CAN_BITRATE` |
| autosar-time-sync | `/etc/default/autosar-time-sync` | `AUTOSAR_TSYNC_POLL_MS` |
| autosar-persistency-guard | `/etc/default/autosar-persistency-guard` | `AUTOSAR_PERSISTENCY_DIR` |
| autosar-iam-policy | `/etc/default/autosar-iam-policy` | `AUTOSAR_IAM_POLICY_FILE` |
| autosar-network-manager | `/etc/default/autosar-network-manager` | `AUTOSAR_NM_CHANNELS`, `AUTOSAR_NM_TIMEOUT_MS` |
| autosar-platform-app | `/etc/default/autosar-platform-app` | `AUTOSAR_LOG_LEVEL` |
| autosar-exec-manager | `/etc/default/autosar-exec-manager` | `AUTOSAR_USER_APPS_BUILD_DIR` |
| autosar-watchdog | `/etc/default/autosar-watchdog` | `AUTOSAR_WATCHDOG_TIMEOUT_MS` |
| autosar-user-app-monitor | `/etc/default/autosar-user-app-monitor` | `AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS` |
| autosar-ptp-time-provider | `/etc/default/autosar-ptp-time-provider` | `AUTOSAR_PTP_DEVICE` |
| autosar-ntp-time-provider | `/etc/default/autosar-ntp-time-provider` | `AUTOSAR_NTP_POLL_MS` |
| autosar-sm-state | `/etc/default/autosar-sm-state` | `AUTOSAR_SM_STATE_FILE` |
| autosar-ucm | `/etc/default/autosar-ucm` | `AUTOSAR_UCM_STAGING_DIR` |
| autosar-ecu-full-stack | `/etc/default/autosar-ecu-full-stack` | `AUTOSAR_ECU_CAN_IF` |

---

## Appendix C: Extension Roadmap for Production In-Vehicle ECU

| Category | Feature to Add | Difficulty |
|----------|---------------|------------|
| Security | SecOC (CAN message authentication) | Medium |
| Security | X.509 certificate management, TLS | High |
| Security | Secure boot integration | High |
| Diagnostics | Full DoIP UDS service implementation | Medium |
| Diagnostics | Full OBD-II PID support | Low |
| Communication | CAN FD support | Medium |
| Communication | Automotive Ethernet (BroadR-Reach) | High |
| Reliability | ASIL B/D compliant watchdog | High |
| Reliability | Memory protection (MPU/MMU configuration) | High |
| Updates | OTA update campaign management | Medium |
| Updates | A/B partition support | Medium |
| Logging | DLT viewer integration | Low |
| E2E | E2E Profile 01/02/07 | Medium |
