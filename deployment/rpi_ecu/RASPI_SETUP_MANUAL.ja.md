# Raspberry Pi 車載ECU セットアップマニュアル

本マニュアルは、Raspberry Pi を AUTOSAR Adaptive Platform ベースのプロトタイプ車載 ECU として
動作させるための手順を解説します。gPTP（IEEE 802.1AS）の詳細な設定方法を含め、
初めての方でも最初から最後まで手順通りに実施できるよう記述しています。

---

## 目次

1. [はじめに・動作確認済み構成](#1-はじめに動作確認済み構成)
2. [ハードウェア要件](#2-ハードウェア要件)
3. [Raspberry Pi OS のインストール](#3-raspberry-pi-os-のインストール)
4. [基本システム設定](#4-基本システム設定)
5. [gPTP (IEEE 802.1AS) 設定 — 詳細](#5-gptp-ieee-80211as-設定--詳細)
6. [CAN バスの設定](#6-can-バスの設定)
7. [ミドルウェアのインストール](#7-ミドルウェアのインストール)
8. [AUTOSAR AP のビルドとインストール](#8-autosar-ap-のビルドとインストール)
9. [systemd サービスの設定](#9-systemd-サービスの設定)
10. [サービスの起動と確認](#10-サービスの起動と確認)
11. [ユーザーアプリケーションの追加](#11-ユーザーアプリケーションの追加)
12. [動作確認コマンド集](#12-動作確認コマンド集)
13. [トラブルシューティング](#13-トラブルシューティング)

---

## 1. はじめに・動作確認済み構成

### このマニュアルで実現できること

| 機能 | 対応状況 |
|------|---------|
| SOME/IP サービス通信（Pub/Sub・RPC） | ✓ 動作確認済み |
| DDS Pub/Sub（Cyclone DDS） | ✓ 動作確認済み |
| iceoryx ゼロコピー通信 | ✓ 動作確認済み |
| CAN バス受信（SocketCAN） | ✓ 動作確認済み |
| gPTP/PTP ハードウェア時刻同期 | ✓ 動作確認済み（要 PTP 対応 NIC） |
| NTP 時刻同期 | ✓ 動作確認済み |
| Platform Health Monitoring (PHM) | ✓ 動作確認済み |
| 永続化ストレージ (Persistency) | ✓ 動作確認済み |
| 診断 UDS サービス | ✓ 動作確認済み |
| ネットワーク管理 (NM) | ✓ 動作確認済み |
| ソフトウェア更新 (UCM) | ✓ 動作確認済み |
| IAM ポリシー管理 | ✓ 動作確認済み |
| DoIP 診断通信 | ✓ 動作確認済み |
| QNX 8.0 クロスビルド | ✓ 別途 QNX README 参照 |

### 動作確認済みハードウェア

- **Raspberry Pi 4 Model B** (4GB / 8GB RAM 推奨)
- **Raspberry Pi 5** (8GB RAM 推奨)
- Raspberry Pi 3 Model B+ でも動作しますが、ビルド時間が長くなります

---

## 2. ハードウェア要件

### 必須

| 項目 | 推奨スペック |
|------|------------|
| SBC | Raspberry Pi 4B / 5 |
| RAM | 4GB 以上（ビルド込みで使う場合は 4GB 必須） |
| ストレージ | microSD 32GB 以上（Class 10 以上）または SSD |
| OS | Raspberry Pi OS (64-bit, Bookworm/Bullseye) |
| ネットワーク | 有線 Ethernet（gPTP 使用時は必須） |

### gPTP を使用する場合（追加要件）

| 項目 | 説明 |
|------|------|
| PTP 対応 NIC | Raspberry Pi 4/5 の内蔵 Ethernet は **PTP ハードウェアタイムスタンプ非対応** |
| 推奨 USB-Ethernet | AX88179 チップセット搭載アダプタ（例: UGREEN USB-C to Ethernet）|
| PCIe NIC（Pi 5） | Intel I210, I350 など PTP 対応 NIC（HAT 経由） |
| 代替手段 | ソフトウェアタイムスタンプ（精度は落ちるが動作可能） |

> **補足:** PTP ハードウェアタイムスタンプ非対応 NIC でも `linuxptp` は動作します。
> ソフトウェアタイムスタンプモードで gPTP 参加可能ですが、精度は ±100μs 程度になります。
> 車載要件（±1μs）が必要な場合は PTP 対応 NIC が必要です。

### CAN バスを使用する場合（追加要件）

| 項目 | 選択肢 |
|------|-------|
| CAN HAT | Waveshare RS485 CAN HAT / PiCAN2 / UCAN Board |
| USB-CAN | PCAN-USB / CANDLELIGHT / socketcan_slcan |
| ビットレート | 500 kbps / 1 Mbps（用途に合わせて選択） |
| CAN FD | 対応 HAT が必要（MCP2517FD / TCAN4550 搭載品） |

---

## 3. Raspberry Pi OS のインストール

### 3.1 Raspberry Pi Imager のダウンロードとインストール

PC（Windows/macOS/Linux）に Raspberry Pi Imager をインストールします。

```
https://www.raspberrypi.com/software/
```

### 3.2 OS イメージの書き込み

1. Raspberry Pi Imager を起動
2. **OS を選択** → `Raspberry Pi OS (other)` → `Raspberry Pi OS Lite (64-bit)` を選択
   - GUI は不要。Lite 版でビルドから実行まで全て完結します
3. **ストレージを選択** → microSD カードを選択
4. 歯車アイコン（⚙）をクリックして詳細設定を開く：
   - ホスト名: `raspi-ecu` などに設定
   - SSH 有効化: 有効
   - ユーザー名/パスワード: 任意（例: `pi` / `raspberry`）
   - WiFi: 必要に応じて設定（有線推奨）
   - ロケール: Asia/Tokyo
5. **書き込み** を実行

### 3.3 初回起動と SSH 接続

```bash
# PC から SSH 接続
ssh pi@raspi-ecu.local

# または IP アドレスで接続
ssh pi@192.168.x.x
```

---

## 4. 基本システム設定

### 4.1 システムアップデート

```bash
sudo apt update && sudo apt full-upgrade -y
sudo reboot
```

### 4.2 必要パッケージのインストール

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

### 4.3 スワップ領域の拡張（ビルド時のメモリ不足対策）

RAM が 4GB 未満の場合、またはビルド中にメモリ不足が発生する場合はスワップを拡張します。

```bash
# スワップサイズを確認
free -h

# スワップを 4GB に拡張
sudo dphys-swapfile swapoff
sudo sed -i 's/CONF_SWAPSIZE=.*/CONF_SWAPSIZE=4096/' /etc/dphys-swapfile
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
free -h  # 確認
```

### 4.4 Git リポジトリのクローン

```bash
# ホームディレクトリに配置
cd ~
git clone https://github.com/your-org/Adaptive-AUTOSAR.git
cd Adaptive-AUTOSAR
```

---

## 5. gPTP (IEEE 802.1AS) 設定 — 詳細

gPTP（Generalized Precision Time Protocol、IEEE 802.1AS）は車載 Ethernet 上での
高精度時刻同期プロトコルです。AUTOSAR Adaptive Platform では `ara::tsync` API を通じて
gPTP 時刻を取得します。

### 5.1 linuxptp のインストール

```bash
# パッケージからインストール（Raspberry Pi OS Bookworm では利用可能）
sudo apt install -y linuxptp

# バージョン確認
ptp4l --version
phc2sys --version
```

バージョンが古い場合や Bookworm 以外では、ソースからビルドします：

```bash
# ソースからビルド
sudo apt install -y libnl-3-dev libnl-genl-3-dev
git clone https://git.code.sf.net/p/linuxptp/code linuxptp
cd linuxptp
make -j$(nproc)
sudo make install
cd ..
```

### 5.2 NIC の PTP サポート確認

```bash
# 使用する NIC インターフェース名を確認
ip link show

# PTP ハードウェアタイムスタンプ対応確認
ethtool -T eth0
# または
ethtool -T enp3s0  # USB-Ethernet などの場合
```

出力例（ハードウェアタイムスタンプ対応の場合）：
```
Time stamping parameters for eth0:
Capabilities:
    hardware-transmit     (SOF_TIMESTAMPING_TX_HARDWARE)
    software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)
    hardware-receive      (SOF_TIMESTAMPING_RX_HARDWARE)
    software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)
    hardware-raw-clock    (SOF_TIMESTAMPING_RAW_HARDWARE)
PTP Hardware Clock: 0          # <--- この番号が /dev/ptp0 に対応
Hardware Transmit Timestamp Modes:
    off                   (HWTSTAMP_TX_OFF)
    on                    (HWTSTAMP_TX_ON)
Hardware Receive Filter Modes:
    none                  (HWTSTAMP_FILTER_NONE)
    all                   (HWTSTAMP_FILTER_ALL)
```

> **ハードウェアタイムスタンプ非対応の場合** (Raspberry Pi 4 内蔵 Ethernet の場合)
> `PTP Hardware Clock: none` と表示されます。ソフトウェアタイムスタンプモードで動作します。

### 5.3 gPTP 設定ファイルの作成

#### AUTOSAR 車載プロファイル向け ptp4l 設定

`/etc/linuxptp/automotive-master.cfg`（gPTP グランドマスター用）:

```ini
# /etc/linuxptp/automotive-master.cfg
# AUTOSAR automotive gPTP (IEEE 802.1AS) - Grand Master configuration
#
# This configuration follows the AUTOSAR Time Synchronization specification
# (AUTOSAR_SWS_TimeSynchronization) for in-vehicle Ethernet use.

[global]
# AUTOSAR gPTP automotive profile
transportSpecific           1          # gPTP (802.1AS) transport-specific field

# Clock identity (optional: set to NIC MAC or leave auto-detected)
# clockIdentity             001122.FFFF.334455

# Clock quality - Grand Master settings
clockClass                  6          # Locked to primary reference (GNSS/GPS ideal)
clockAccuracy               0x21       # within 100 ns (GPS-locked)
offsetScaledLogVariance     0x4E5D

# Priority settings
priority1                   128        # 0-255, lower = preferred master
priority2                   128        # tie-breaker

# Domain (AUTOSAR standard: domain 0)
domainNumber                0

# Logging intervals (log base-2 of seconds)
logAnnounceInterval         0          # 1 second
logSyncInterval             -3         # 125 ms (AUTOSAR requirement)
logMinDelayReqInterval      0          # 1 second
announceReceiptTimeout      3          # 3 intervals before timeout

# Clock servo
clock_servo                 linreg
pi_proportional_const       0.0
pi_integral_const           0.0

# Sync output
summary_interval            0          # Print sync stats every interval
time_stamping               hardware   # Use hardware timestamps (or 'software' if not available)
```

`/etc/linuxptp/automotive-slave.cfg`（gPTP スレーブ / ECU 用）:

```ini
# /etc/linuxptp/automotive-slave.cfg
# AUTOSAR automotive gPTP (IEEE 802.1AS) - Slave (ECU) configuration
#
# Use this on ECUs that synchronize to the gPTP Grand Master.

[global]
transportSpecific           1          # gPTP (802.1AS)

# Slave-only mode: do not become Grand Master
slaveOnly                   1

clockClass                  255        # Slave-only device
clockAccuracy               0xFE       # Unknown
offsetScaledLogVariance     0xFFFF

priority1                   255        # Never become master
priority2                   255

domainNumber                0

logAnnounceInterval         0
logSyncInterval             -3         # 125 ms sync interval
logMinDelayReqInterval      0
announceReceiptTimeout      3

clock_servo                 linreg
pi_proportional_const       0.0
pi_integral_const           0.0

summary_interval            0
time_stamping               hardware   # 'software' if no HW timestamps
```

各 NIC インターフェースのセクションを末尾に追加します（例: `eth0`）:

```ini
# インターフェース設定（ファイル末尾に追加）
[eth0]
delay_mechanism             P2P          # Peer-to-Peer delay (gPTP/802.1AS 必須)
network_transport           L2           # Ethernet layer 2 (gPTP 必須)
```

> **インターフェース名の確認:**
> `ip link show` で実際のインターフェース名を確認してください。
> Raspberry Pi 4 内蔵 Ethernet は通常 `eth0`。USB-Ethernet は `enx...` や `eth1` など。

### 5.4 ソフトウェアタイムスタンプモードでの設定（PTP 非対応 NIC 用）

Raspberry Pi 4 の内蔵 Ethernet など PTP ハードウェアタイムスタンプ非対応の場合:

```ini
# /etc/linuxptp/automotive-slave-sw.cfg
# ソフトウェアタイムスタンプモード版（精度: ±100μs 程度）

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
time_stamping               software    # ← ソフトウェアタイムスタンプ

[eth0]
delay_mechanism             P2P
network_transport           L2
```

### 5.5 ptp4l の systemd サービス設定

```bash
sudo mkdir -p /etc/linuxptp
```

`/etc/systemd/system/ptp4l-automotive.service` を作成:

```ini
[Unit]
Description=IEEE 802.1AS (gPTP) - ptp4l automotive profile
Documentation=man:ptp4l(8)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
# スレーブ（ECU）の場合: automotive-slave.cfg
# PTP 非対応 NIC の場合: automotive-slave-sw.cfg
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

# 状態確認
sudo systemctl status ptp4l-automotive.service
```

### 5.6 phc2sys — PTP ハードウェアクロックとシステムクロックの同期

ハードウェアタイムスタンプ対応 NIC がある場合、PTP ハードウェアクロック (PHC `/dev/ptp0`) を
システムクロックに同期させます。

```bash
# まず PHC デバイス番号を確認
ethtool -T eth0 | grep "PTP Hardware Clock"
# 例: PTP Hardware Clock: 0  → /dev/ptp0
```

`/etc/systemd/system/phc2sys-automotive.service` を作成:

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

# 状態確認
sudo systemctl status phc2sys-automotive.service
```

> **ソフトウェアタイムスタンプの場合:**
> phc2sys は不要です。ptp4l が直接 CLOCK_REALTIME を調整します。
> その場合は `ptp4l ... -s` オプションを使いません（ptp4l がシステムクロックと同期）。

### 5.7 gPTP 同期状態の確認

```bash
# ptp4l のログで同期状態を確認
sudo journalctl -u ptp4l-automotive.service -f

# 正常同期時の出力例:
# ptp4l[123.456]: rms 45 max 123 freq -456 +/- 78 delay  890 +/-  12
# ptp4l[123.456]: master offset       -45 s2 freq  +1234 path delay       890

# phc2sys のログ確認
sudo journalctl -u phc2sys-automotive.service -f

# 同期オフセット確認ツール
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
sudo pmc -u -b 0 'GET TIME_STATUS_NP'

# PHC と CLOCK_REALTIME のオフセット確認
phc_ctl /dev/ptp0 cmp
```

### 5.8 chrony との共存設定（NTP バックアップ）

gPTP が使えない環境や、フォールバックとして NTP も使いたい場合:

```bash
sudo apt install -y chrony
```

`/etc/chrony/chrony.conf` を編集:

```conf
# NTP サーバー（フォールバック用）
pool ntp.nict.jp iburst

# PTP ハードウェアクロックをソースとして追加（優先度高）
# /dev/ptp0 が存在する場合（ptp4l 動作中）
refclock PHC /dev/ptp0 poll 0 dpoll -2 offset 0 precision 1e-9 trust prefer

# ローカルクロック（最終フォールバック）
local stratum 10

# 時刻ステップ許可（起動時など大きなずれを修正）
makestep 0.1 3

# RTC 同期
rtcsync
```

```bash
sudo systemctl enable chrony
sudo systemctl restart chrony

# chrony 状態確認
chronyc sources -v
chronyc tracking
```

### 5.9 AUTOSAR PTP Time Provider デーモンの設定

PTP ハードウェアクロック (`/dev/ptp0`) を `ara::tsync::PtpTimeBaseProvider` 経由で
読み取るデーモンを設定します。

```bash
# 環境設定ファイルの編集
sudo nano /etc/default/autosar-ptp-time-provider
```

```bash
# /etc/default/autosar-ptp-time-provider
# PTP デバイスパス（ethtool -T eth0 で確認した番号に合わせる）
AUTOSAR_PTP_DEVICE=/dev/ptp0

# 同期チェック間隔（ミリ秒）
AUTOSAR_PTP_POLL_MS=100

# 同期状態ファイル
AUTOSAR_PTP_STATUS_FILE=/run/autosar/ptp_time_provider.status
```

```bash
sudo systemctl enable autosar-ptp-time-provider.service
sudo systemctl start autosar-ptp-time-provider.service
sudo systemctl status autosar-ptp-time-provider.service
```

---

## 6. CAN バスの設定

### 6.1 CAN HAT のセットアップ（MCP2515 系 SPI CAN HAT）

Raspberry Pi の SPI CAN HAT（MCP2515 / PiCAN2 / Waveshare CAN HAT 等）のセットアップ。

#### `/boot/config.txt`（または `/boot/firmware/config.txt`）に追加:

```bash
sudo nano /boot/firmware/config.txt
```

```ini
# SPI CAN HAT (MCP2515) - ボードに合わせて調整
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
# Waveshare CAN HAT の場合:
# dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25
# PiCAN2 の場合:
# dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25
```

```bash
sudo reboot
```

### 6.2 SocketCAN インターフェースの確認と設定

```bash
# CAN インターフェースの確認
ip link show type can
# 例: can0: <NOARP,ECHO> mtu 16 qdisc noop state DOWN...

# プロジェクトのスクリプトで設定（500 kbps）
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000

# 手動で設定する場合:
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0
ip link show can0  # UP になっていることを確認
```

### 6.3 仮想 CAN（CAN 実機なしでのテスト用）

CAN ハードウェアがない場合、仮想 CAN デバイスでテストできます。

```bash
# 仮想 CAN モジュールの有効化
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# プロジェクトスクリプト使用:
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan

# テスト（ターミナル2本使って送受信）
# 受信側:
candump vcan0 &
# 送信側:
cansend vcan0 123#DEADBEEF
```

### 6.4 CAN インターフェースの永続化

再起動後も自動で CAN インターフェースを設定するには:

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

または `/etc/rc.local` に追記（シンプルな方法）:

```bash
# /etc/rc.local（exit 0 の前に追加）
ip link set can0 type can bitrate 500000
ip link set up can0
```

---

## 7. ミドルウェアのインストール

### 7.1 一括インストール（推奨）

プロジェクトのスクリプトを使ってミドルウェアを一括インストールします。
**ビルドには 30〜90 分程度かかります**（Raspberry Pi 4 の場合）。

```bash
cd ~/Adaptive-AUTOSAR
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

このスクリプトは以下をインストールします：
- **vSomeIP** → `/opt/vsomeip`（SOME/IP ミドルウェア）
- **iceoryx** → `/opt/iceoryx`（ゼロコピー共有メモリ IPC）
- **Cyclone DDS** → `/opt/cyclonedds`（DDS ミドルウェア）

### 7.2 個別インストール（問題があった場合）

```bash
# 基本依存パッケージのみ
sudo ./scripts/install_dependency.sh

# vSomeIP のみ
sudo ./scripts/install_vsomeip.sh

# iceoryx のみ
sudo ./scripts/install_iceoryx.sh

# Cyclone DDS のみ
sudo ./scripts/install_cyclonedds.sh
```

### 7.3 インストール確認

```bash
ls /opt/vsomeip/lib/      # libvsomeip3.so が存在すること
ls /opt/iceoryx/lib/      # libiceoryx_posh.a 等が存在すること
ls /opt/cyclonedds/lib/   # libddsc.so が存在すること
```

---

## 8. AUTOSAR AP のビルドとインストール

### 8.1 統合ビルドスクリプト（推奨）

```bash
cd ~/Adaptive-AUTOSAR
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware
```

このスクリプトは以下を行います：
1. AUTOSAR AP ランタイムライブラリのビルドとインストール
2. ユーザーアプリケーションテンプレートのビルドとインストール

**ビルド時間の目安:**
- Raspberry Pi 4 (4GB): 約 15〜30 分
- Raspberry Pi 5 (8GB): 約 8〜15 分

### 8.2 インストール確認

```bash
ls /opt/autosar_ap/lib/         # AUTOSAR AP ライブラリ
ls /opt/autosar_ap/include/     # ヘッダーファイル
ls /opt/autosar_ap/bin/         # バイナリ（adaptive_autosar 等）
ls /opt/autosar_ap/user_apps_build/  # ユーザーアプリバイナリ
```

### 8.3 AUTOSAR AP ライブラリへの PATH 設定

```bash
# 共有ライブラリのパスを登録
echo '/opt/autosar_ap/lib' | sudo tee /etc/ld.so.conf.d/autosar_ap.conf
echo '/opt/vsomeip/lib' | sudo tee -a /etc/ld.so.conf.d/autosar_ap.conf
echo '/opt/iceoryx/lib' | sudo tee -a /etc/ld.so.conf.d/autosar_ap.conf
echo '/opt/cyclonedds/lib' | sudo tee -a /etc/ld.so.conf.d/autosar_ap.conf
sudo ldconfig
```

---

## 9. systemd サービスの設定

### 9.1 サービスのインストール

```bash
cd ~/Adaptive-AUTOSAR
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

### 9.2 サービスの依存関係と起動順序

```
network-online.target
         │
         ▼
autosar-iox-roudi.service        ← iceoryx RouDi（共有メモリ IPC）
         │
         ▼
autosar-vsomeip-routing.service  ← SOME/IP ルーティングマネージャ
autosar-can-manager.service      ← CAN インターフェース管理
         │
         ▼
autosar-time-sync.service        ← 時刻同期状態監視
autosar-persistency-guard.service ← 永続化ストレージ
autosar-iam-policy.service       ← IAM ポリシー管理
autosar-network-manager.service  ← NM バス状態管理
autosar-dlt.service              ← DLT ログ収集
         │
         ▼
autosar-platform-app.service     ← プラットフォームコア (EM/SM/PHM/Diag)
         │
         ▼
autosar-exec-manager.service     ← ユーザーアプリ起動 (bringup.sh)
         │
         ├── autosar-watchdog.service         ← ウォッチドッグ
         ├── autosar-user-app-monitor.service ← アプリ監視・再起動
         ├── autosar-ptp-time-provider.service ← PTP 時刻プロバイダ
         ├── autosar-ntp-time-provider.service ← NTP 時刻プロバイダ
         ├── autosar-sm-state.service         ← SM 状態ダウンン
         ├── autosar-ucm.service              ← ソフトウェア更新
         └── autosar-ecu-full-stack.service   ← サンプル ECU アプリ
```

### 9.3 環境設定ファイルの編集

各サービスの動作パラメータは `/etc/default/autosar-*` で設定します。

#### SOME/IP ルーティングマネージャ

```bash
sudo nano /etc/default/autosar-vsomeip-routing
```

```bash
# SOME/IP 設定ファイルパス
VSOMEIP_CONFIGURATION=/opt/autosar_ap/etc/vsomeip-rpi.json
# マルチキャストアドレス（LAN セグメントに合わせて変更）
VSOMEIP_APPLICATION_NAME=routingmanagerd
```

#### CAN インターフェース管理

```bash
sudo nano /etc/default/autosar-can-manager
```

```bash
# CAN インターフェース名（実機は can0、仮想は vcan0）
AUTOSAR_CAN_IFNAME=can0
AUTOSAR_CAN_BITRATE=500000
# バスウェイクアップトリガーディレクトリ
AUTOSAR_CAN_TRIGGER_DIR=/run/autosar/can_triggers
```

#### ネットワーク管理 (NM)

```bash
sudo nano /etc/default/autosar-network-manager
```

```bash
# 管理する CAN チャンネル名（カンマ区切り）
AUTOSAR_NM_CHANNELS=can0
AUTOSAR_NM_PERIOD_MS=100
AUTOSAR_NM_TIMEOUT_MS=5000
AUTOSAR_NM_REPEAT_MSG_TIME_MS=1500
AUTOSAR_NM_WAIT_BUS_SLEEP_MS=2000
AUTOSAR_NM_AUTO_REQUEST=true
AUTOSAR_NM_PARTIAL_NETWORKING=false
```

#### PTP 時刻プロバイダ

```bash
sudo nano /etc/default/autosar-ptp-time-provider
```

```bash
# PHC デバイスパス（ethtool -T eth0 で確認）
AUTOSAR_PTP_DEVICE=/dev/ptp0
AUTOSAR_PTP_POLL_MS=100
```

### 9.4 IAM ポリシーの設定

```bash
sudo nano /etc/autosar/iam_policy.csv
```

```csv
# subject,resource,action
# フォーマット: <アプリ名>,<リソース名>,<操作>
# ワイルドカード: * が使用可能
ecu_full_stack,vehicle_status,publish
ecu_full_stack,diagnostics,read
*,persistency,read
*,persistency,write
```

---

## 10. サービスの起動と確認

### 10.1 初回起動手順

サービスは依存関係の順に起動します。一括起動する場合:

```bash
# 1. iceoryx RouDi（最初に起動）
sudo systemctl start autosar-iox-roudi.service
sleep 2

# 2. SOME/IP ルーティングマネージャ
sudo systemctl start autosar-vsomeip-routing.service
sleep 2

# 3. CAN インターフェース管理
sudo systemctl start autosar-can-manager.service

# 4. コアサービス群
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-network-manager.service

# 5. プラットフォームアプリ
sudo systemctl start autosar-platform-app.service
sleep 3

# 6. 実行マネージャ（ユーザーアプリ起動）
sudo systemctl start autosar-exec-manager.service

# 7. 監視・補助サービス
sudo systemctl start autosar-watchdog.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-ptp-time-provider.service
sudo systemctl start autosar-ntp-time-provider.service
```

> **自動起動:** `--enable` オプションでインストールした場合は、再起動時に自動的に起動します。

### 10.2 サービス状態の一括確認

```bash
# 全 AUTOSAR サービスの状態確認
systemctl status 'autosar-*.service' --no-pager | grep -E '(●|Active:|●)'

# ログのリアルタイム確認
sudo journalctl -u 'autosar-*' -f

# 個別サービスの確認
sudo systemctl status autosar-vsomeip-routing.service --no-pager
sudo systemctl status autosar-platform-app.service --no-pager
sudo systemctl status autosar-exec-manager.service --no-pager
sudo systemctl status autosar-watchdog.service --no-pager
sudo systemctl status autosar-user-app-monitor.service --no-pager
```

### 10.3 サンプル ECU アプリ（ecu-full-stack）の起動

自分のアプリケーションを追加する前にサンプルで動作確認できます。

```bash
# ecu-full-stack サンプルの起動
sudo systemctl start autosar-ecu-full-stack.service
sudo systemctl status autosar-ecu-full-stack.service --no-pager

# ログ確認
sudo journalctl -u autosar-ecu-full-stack.service -f
```

---

## 11. ユーザーアプリケーションの追加

### 11.1 bringup.sh の編集

ECU 起動時に実行したいアプリケーションを `bringup.sh` に追加します。

```bash
sudo nano /etc/autosar/bringup.sh
```

```bash
#!/usr/bin/env bash
# /etc/autosar/bringup.sh
# Adaptive AUTOSAR ECU bringup script
# このファイルを編集してユーザーアプリを追加してください。

APP_DIR="${AUTOSAR_USER_APPS_BUILD_DIR:-/opt/autosar_ap/user_apps_build}"

# === ユーザーアプリ追加例 ===

# 基本的な起動（PID 監視のみ）
launch_app "my_app" "${APP_DIR}/my_application" --param1 value1

# ハートビート監視付き起動（定期ファイル更新で生存確認）
launch_app_with_heartbeat \
  "my_hb_app" \
  "/run/autosar/my_hb_app.hb" \
  5000 \
  "${APP_DIR}/my_hb_application"

# フル管理モード（PHM 統合・自動再起動ポリシー）
launch_app_managed \
  "vehicle_monitor" \
  "/vehicle/monitor/1" \
  "/run/autosar/vehicle_monitor.hb" \
  3000 \
  5 \
  60000 \
  "${APP_DIR}/vehicle_monitor_app"

# === SOME/IP サービスの起動例 ===
launch_app "someip_provider" \
  "${APP_DIR}/autosar_user_com_someip_provider_template"

# === DDS パブリッシャーの起動例 ===
launch_app "dds_publisher" \
  "${APP_DIR}/autosar_user_com_dds_pub_template"
```

### 11.2 アプリ登録後のサービス再起動

```bash
sudo systemctl restart autosar-exec-manager.service
sudo systemctl status autosar-exec-manager.service --no-pager

# ユーザーアプリレジストリ確認
cat /run/autosar/user_apps_registry.csv
```

### 11.3 ユーザーアプリのビルド方法

インストール済みランタイムに対してビルドする場合:

```bash
# 外部プロジェクトとしてビルド
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir user_apps \
  --build-dir build-user-apps

# ビルドしてそのまま実行
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir user_apps \
  --build-dir build-user-apps \
  --run
```

---

## 12. 動作確認コマンド集

### 12.1 全体の動作確認スクリプト

```bash
# プロファイル全体の検証
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary

# 物理 CAN を使う場合
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0 \
  --require-platform-binary
```

### 12.2 通信機能の個別確認

```bash
# SOME/IP 通信確認
./tools/sample_runner/verify_ara_com_someip_transport.sh

# iceoryx ゼロコピー通信確認
./tools/sample_runner/verify_ara_com_zerocopy_transport.sh

# DDS 通信確認
./tools/sample_runner/verify_ara_com_dds_transport.sh

# ECU SOME/IP + DDS 連携確認
./tools/sample_runner/verify_ecu_someip_dds.sh

# CAN → DDS ブリッジ確認
./tools/sample_runner/verify_ecu_can_to_dds.sh
```

### 12.3 状態ファイルの確認

```bash
# ユーザーアプリ監視状態
cat /run/autosar/user_app_monitor.status

# PHM ヘルス状態
ls /run/autosar/phm/health/
cat /run/autosar/phm/health/*.status

# PTP 時刻同期状態
cat /run/autosar/ptp_time_provider.status

# NM ネットワーク状態
cat /run/autosar/network_manager.status

# ネットワークウェイクアップトリガー
ls /run/autosar/nm_triggers/
```

### 12.4 gPTP 同期状態の詳細確認

```bash
# ptp4l の現在の同期オフセット確認
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'

# グランドマスター情報
sudo pmc -u -b 0 'GET GRANDMASTER_SETTINGS_NP'

# ポートの状態（SLAVE/MASTER/LISTENING）
sudo pmc -u -b 0 'GET PORT_DATA_SET'

# 時刻ステータス
sudo pmc -u -b 0 'GET TIME_STATUS_NP'

# リアルタイムオフセット確認
sudo journalctl -u ptp4l-automotive.service --since "1 min ago" --no-pager | tail -20
```

### 12.5 CAN バスの確認

```bash
# CAN メッセージのダンプ（全フレーム）
candump can0

# 特定 ID のみ表示（例: 0x123）
candump can0 123:7FF

# CAN 統計情報
ip -details link show can0

# CAN エラーカウンタ確認
ip -s link show can0

# テストフレーム送信
cansend can0 123#DEADBEEF
```

---

## 13. トラブルシューティング

### 13.1 ビルドエラー

#### メモリ不足でビルドが失敗する

```bash
# 並列ビルド数を減らす
# scripts/build_and_install_autosar_ap.sh を編集:
# make -j$(nproc) → make -j2

# または環境変数で指定
MAKEFLAGS="-j2" sudo ./scripts/build_and_install_rpi_ecu_profile.sh ...
```

#### ミドルウェアが見つからない

```bash
# ライブラリパスを確認
ls /opt/vsomeip/lib/libvsomeip3*
ls /opt/iceoryx/lib/libiceoryx*

# ldconfig の再実行
sudo ldconfig
ldconfig -p | grep vsomeip
ldconfig -p | grep iceoryx
```

### 13.2 サービス起動エラー

#### autosar-iox-roudi が起動しない

```bash
sudo journalctl -u autosar-iox-roudi.service --no-pager -n 50

# iceoryx の共有メモリ残骸を削除
sudo rm -rf /dev/shm/iceoryx_mgmt
sudo rm -rf /tmp/roudi_*

# 再起動
sudo systemctl restart autosar-iox-roudi.service
```

#### autosar-vsomeip-routing が起動しない

```bash
sudo journalctl -u autosar-vsomeip-routing.service --no-pager -n 50

# 設定ファイル確認
cat /etc/default/autosar-vsomeip-routing
ls /opt/autosar_ap/etc/vsomeip-rpi.json

# VSOMEIP_CONFIGURATION のパスが正しいか確認
```

### 13.3 gPTP の問題

#### ptp4l が同期しない

```bash
# 接続先の gPTP グランドマスターが動作しているか確認
sudo tcpdump -i eth0 ether proto 0x88f7 -v

# ネットワークの疎通確認
ping <grand_master_IP>

# ptp4l のデバッグログ
sudo ptp4l -f /etc/linuxptp/automotive-slave.cfg -i eth0 -m -q 2>&1 | head -50
```

#### オフセットが大きい（ソフトウェアタイムスタンプ使用時）

ソフトウェアタイムスタンプでは精度が ±100μs 程度になります。
これは正常動作です。高精度が必要な場合は PTP 対応 NIC を使用してください。

```bash
# 現在のオフセット確認
sudo journalctl -u ptp4l-automotive.service --no-pager -n 10 | grep "master offset"
# 例: master offset   -1234 s2 freq  +5678 path delay    9012
# → s2 は同期済み（locked）を意味する
```

### 13.4 CAN バスの問題

#### CAN インターフェースが UP にならない

```bash
# カーネルモジュールの確認
lsmod | grep -E "(mcp251|can|spi)"
dmesg | grep -iE "(mcp|can|spi)" | tail -20

# /boot/firmware/config.txt の dtoverlay 設定を再確認
cat /boot/firmware/config.txt | grep -E "(spi|can|mcp)"

# SPI が有効かどうか
ls /dev/spidev*
```

#### CAN フレームを受信できない

```bash
# ビットレートの確認（送受信側が一致しているか）
ip -details link show can0 | grep bitrate

# エラーフレームのダンプ（エラーを含む全フレーム）
candump -e can0

# バスオフ状態の確認
ip link show can0 | grep -i "error"
# BUSOFF → バスオフ状態、再起動が必要
sudo ip link set down can0
sudo ip link set up can0
```

### 13.5 ara::tsync / PTP の問題

```bash
# PTP デバイスの存在確認
ls /dev/ptp*
# 存在しない場合: PTP 非対応 NIC → ソフトウェアタイムスタンプを使用

# PHC 時刻の直接確認
phc_ctl /dev/ptp0 get

# PHC とシステムクロックのオフセット
phc_ctl /dev/ptp0 cmp

# AUTOSAR PTP Time Provider の状態
sudo systemctl status autosar-ptp-time-provider.service
cat /run/autosar/ptp_time_provider.status
```

### 13.6 ログとデバッグ

```bash
# 全 AUTOSAR サービスのログ
sudo journalctl -u 'autosar-*' --since "10 min ago" --no-pager

# 特定サービスのリアルタイムログ
sudo journalctl -u autosar-platform-app.service -f

# カーネルメッセージ（ドライバ関連）
sudo dmesg -w | grep -iE "(can|spi|ptp|eth)"

# SOME/IP デバッグログ（vSomeIP）
export VSOMEIP_LOG_LEVEL=debug
/opt/autosar_ap/bin/adaptive_autosar
```

---

## 付録 A: Raspberry Pi 4/5 での PTP 対応 NIC 推奨リスト

| NIC | チップ | PTP 対応 | 接続 | 備考 |
|-----|--------|---------|------|------|
| Intel I210-T1 | I210 | ハードウェア | PCIe (Pi 5 HAT) | 最高精度 |
| Intel I350-T2 | I350 | ハードウェア | PCIe (Pi 5 HAT) | 2ポート |
| Realtek RTL8156 | RTL8156 | ソフト | USB 3.0 | 汎用 USB-Ethernet |
| ASIX AX88179 | AX88179 | ソフト | USB 3.0 | 広く流通 |

> Raspberry Pi 4/5 の内蔵 Ethernet (BCM54213) は PTP ハードウェアタイムスタンプ非対応。
> Pi 5 の PCIe スロット経由で I210/I350 を使用すると高精度 gPTP が実現可能。

---

## 付録 B: サービス一覧と環境変数リファレンス

| サービス | 環境ファイル | 主要変数 |
|--------|------------|---------|
| autosar-iox-roudi | (なし) | — |
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

## 付録 C: 車載 ECU としての拡張ロードマップ

このプラットフォームをベースに本格的な車載 ECU に仕上げるための追加検討事項:

| カテゴリ | 追加すべき機能 | 難易度 |
|---------|--------------|-------|
| セキュリティ | SecOC（CAN メッセージ認証）| 中 |
| セキュリティ | X.509 証明書管理・TLS | 高 |
| セキュリティ | セキュアブート統合 | 高 |
| 診断 | DoIP 全 UDS サービス実装 | 中 |
| 診断 | OBD-II PIDs 完全対応 | 低 |
| 通信 | CAN FD サポート | 中 |
| 通信 | Automotive Ethernet (BroadR-Reach) | 高 |
| 信頼性 | ASIL B/D 準拠ウォッチドッグ | 高 |
| 信頼性 | メモリ保護 (MPU/MMU 設定) | 高 |
| 更新 | OTA 更新キャンペーン管理 | 中 |
| 更新 | A/B パーティション対応 | 中 |
| ログ | DLT ビューア連携 | 低 |
| E2E | E2E Profile 01/02/07 | 中 |
