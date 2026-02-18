# QNX 8.0 Cross-Build

このディレクトリには、QNX SDP 8.0 を使って本リポジトリをクロスコンパイルするためのスクリプトをまとめています。

## 対象環境

- ホスト: Linux (x86\_64)
- ターゲット: QNX 8.0 (`aarch64le` / `x86_64`)
- 前提: `QNX_HOST` と `QNX_TARGET` が設定済み (`qnxsdp-env.sh` を source)

---

## クイックスタート（一括ビルド）

> **注意**: ミドルウェアは `/opt/qnx` にインストールされます。事前に `sudo mkdir -p /opt/qnx && sudo chmod 777 /opt/qnx` を実行してください。

```bash
# QNX SDK 環境を読み込む
source ~/qnx800/qnxsdp-env.sh

# 全コンポーネントをビルド＆インストール
./qnx/scripts/build_all_qnx.sh install

# ビルド＋デプロイイメージ作成まで一括実行
./qnx/scripts/build_all_qnx.sh install --target-ip 192.168.1.100
```

---

## 手順詳細

### 1. 環境設定

```bash
source ~/qnx800/qnxsdp-env.sh
source qnx/env/qnx800.env.example   # 必要に応じてコピーして値を調整
```

### 2. ホストツール確認

```bash
./qnx/scripts/install_dependency.sh
```

- `apt` 等のパッケージマネージャは使用しません
- 必須コマンドの存在確認と、必要な場合はホストツールをビルドします

### 3. ミドルウェアをクロスビルド

```bash
# 全ミドルウェアを一括インストール
./qnx/scripts/build_libraries_qnx.sh all install

# 個別ビルド
./qnx/scripts/build_libraries_qnx.sh cyclonedds install --disable-shm
./qnx/scripts/build_libraries_qnx.sh vsomeip install
./qnx/scripts/build_libraries_qnx.sh all clean
```

インストール先: `/opt/qnx/` (boost → `third_party/`, iceoryx / cyclonedds / vsomeip)

### 4. AUTOSAR AP ランタイムをクロスビルド

```bash
./qnx/scripts/build_autosar_ap_qnx.sh install
```

インストール先: `/opt/qnx/autosar_ap/aarch64le/`

### 5. user\_apps をクロスビルド

```bash
./qnx/scripts/build_user_apps_qnx.sh build
```

### 6. 一括実行

```bash
./qnx/scripts/build_all_qnx.sh install
```

### メモリ不足エラーが出る場合

```bash
export AUTOSAR_QNX_JOBS=2
./qnx/scripts/build_libraries_qnx.sh all install
```

---

## 7. デプロイイメージ作成

ビルド成果物・依存共有ライブラリをまとめてイメージファイルにパッケージングします。

```bash
./qnx/scripts/create_qnx_deploy_image.sh --target-ip <QNXターゲットIP>
```

**生成物** (`out/qnx/deploy/`):

| ファイル | 説明 |
| --- | --- |
| `autosar_ap-aarch64le.tar.gz` | 展開型パッケージ（デフォルト） |
| `autosar_ap-aarch64le.ifs` | QNX IFS イメージ（`--no-tar` オプション時） |

**含まれるファイル**:

- `bin/`: AUTOSAR AP プラットフォームデーモン 14本 + ユーザーアプリ 13本
- `lib/`: vsomeip3 / CycloneDDS / iceoryx 共有ライブラリ一式
- `etc/vsomeip_routing.json`, `vsomeip_client.json`: vsomeip 設定
- `startup.sh`, `env.sh`: 起動・環境設定スクリプト

主なオプション:

| オプション | 説明 |
| --- | --- |
| `--target-ip <ip>` | vsomeip 設定に書き込む unicast IP |
| `--no-tar` | IFS イメージのみ作成（tar.gz なし） |
| `--no-ifs` | tar.gz のみ作成（mkifs 不要） |
| `--output-dir <dir>` | 出力先ディレクトリを変更 |

---

## 8. QNX ターゲットへのデプロイ（SSH 転送・マウント）

### 8-1. tar.gz を SCP 転送して展開（推奨）

```bash
./qnx/scripts/deploy_to_qnx_target.sh --host 192.168.1.100
```

スクリプトが以下を自動で実行します:

1. `out/qnx/deploy/autosar_ap-aarch64le.tar.gz` を SCP 転送
2. SSH 接続してターゲット上の `/autosar` に展開
3. vsomeip の unicast IP をターゲット IP に書き換え

### 8-2. IFS イメージを転送して devb-ram 経由でマウント

```bash
./qnx/scripts/deploy_to_qnx_target.sh \
  --host 192.168.1.100 \
  --image out/qnx/deploy/autosar_ap-aarch64le.ifs \
  --image-type ifs
```

ターゲット上での動作:

```sh
devb-ram unit=9 capacity=<sectors> &        # RAM ブロックデバイス作成
dd if=autosar_ap-aarch64le.ifs of=/dev/ram9 # IFS 書き込み
mount -t ifs /dev/ram9 /autosar             # マウント
```

### 8-3. デプロイオプション一覧

| オプション | 説明 | デフォルト |
| --- | --- | --- |
| `--host <ip>` | QNX ターゲットの IP またはホスト名 | 必須 |
| `--user <user>` | SSH ユーザー | `root` |
| `--port <port>` | SSH ポート | `22` |
| `--image <file>` | 転送するイメージファイル | 自動検出 |
| `--image-type tar\|ifs` | イメージ種別 | 自動判定 |
| `--remote-dir <path>` | ターゲット上の展開先 | `/autosar` |
| `--target-ip <ip>` | vsomeip unicast IP（省略時は `--host` の値） | — |
| `--dry-run` | コマンドを表示するだけで実行しない | — |

---

## 9. ターゲット上での起動

デプロイ後、SSH でターゲットにログインして以下を実行します:

```sh
# 全デーモンを起動
/autosar/startup.sh --all

# vsomeip ルーティングマネージャのみ起動
/autosar/startup.sh --routing-only

# 停止
/autosar/startup.sh --stop

# 手動でユーザーアプリを起動（env.sh で LD_LIBRARY_PATH を設定）
. /autosar/env.sh
VSOMEIP_CONFIGURATION=/autosar/etc/vsomeip_client.json \
  /autosar/bin/autosar_user_minimal_runtime
```

---

## 主なスクリプト

| スクリプト | 説明 |
| --- | --- |
| `qnx/scripts/build_libraries_qnx.sh` | Boost / iceoryx / CycloneDDS / vSomeIP ビルド |
| `qnx/scripts/build_autosar_ap_qnx.sh` | AUTOSAR AP ランタイムビルド |
| `qnx/scripts/build_user_apps_qnx.sh` | ユーザーアプリビルド |
| `qnx/scripts/build_all_qnx.sh` | 上記を順番に実行 |
| `qnx/scripts/create_qnx_deploy_image.sh` | デプロイイメージ（tar.gz / IFS）作成 |
| `qnx/scripts/deploy_to_qnx_target.sh` | SSH 転送・ターゲット上での展開/マウント |
| `qnx/cmake/toolchain_qnx800.cmake` | QNX 用 CMake toolchain |

## 出力先（デフォルト）

```text
/opt/qnx/                        ← ミドルウェアインストール先
  third_party/                   Boost 等
  iceoryx/
  cyclonedds/
  vsomeip/
  autosar_ap/<arch>/             AUTOSAR AP プラットフォームデーモン
out/qnx/
  work/build/user_apps-<arch>/   ユーザーアプリビルド成果物
  deploy/                        デプロイイメージ出力先
    autosar_ap-<arch>.tar.gz
    autosar_ap-<arch>.ifs
```

---

## QNX と Linux の違い（移植上の注意点）

QNX は POSIX 準拠の RTOS ですが、Linux とはカーネルアーキテクチャが根本的に異なり、
移植時にいくつかの非互換があります。

### OS アーキテクチャの違い

| 観点 | Linux | QNX |
| --- | --- | --- |
| カーネル構造 | モノリシックカーネル | マイクロカーネル（Neutrino） |
| ドライバ位置 | カーネル空間 | ユーザー空間プロセス（`devb-*`, `io-pkt` 等） |
| ネットワークスタック | カーネル内蔵 | `io-pkt` ユーザープロセス |
| ファイルシステム | VFS + ext4/xfs 等 | `io-blk` + QNX6 fs、または `fs-*` プロセス |
| プロセス間通信 | ソケット / パイプ / 共有メモリ | メッセージパッシング（`MsgSend/MsgReceive`） |
| リアルタイム性 | PREEMPT\_RT パッチで近似 | 設計段階から決定論的スケジューリング |

### ライブラリ・ヘッダの違い

本プロジェクトで実際に対処した非互換を以下にまとめます。

#### pthread（スレッド）

| 項目 | Linux | QNX 8.0 |
| --- | --- | --- |
| pthread の場所 | `libpthread.so`（独立ライブラリ） | **libc に統合**（`libpthread.a` 不存在） |
| リンクフラグ | `-lpthread` | 不要（ただし cmake が自動付与する） |
| 対処 | — | 空の `libpthread.a` スタブを自動生成 |

#### ソケット・ネットワーク

| ヘッダ / 機能 | Linux | QNX 8.0 | 対処 |
| --- | --- | --- | --- |
| `sys/socket.h` | libc 内包 | `libsocket` が必要（`-lsocket`） | toolchain に `-lsocket` を追加 |
| `SO_BINDTODEVICE` | 対応 | **未対応** | vsomeip のスタブ実装（常に `false`） |
| `sys/sockmsg.h` | なし | QNX 7 まで存在、**8.0 で削除** | `accept4()` ネイティブ実装に置き換え |
| `epoll` (`sys/epoll.h`) | 対応 | **なし** | `poll()` ベースの代替実装 |

#### シグナル

| 機能 | Linux | QNX 8.0 | 対処 |
| --- | --- | --- | --- |
| `sys/signalfd.h` | 対応 | **なし**（Linux 独自） | `pipe()` ベースの停止シグナル実装 |
| `SA_RESTART` | 対応 | **未実装**（ヘッダにコメントアウト） | `-DSA_RESTART=0x0040` をコンパイルフラグに追加 |
| `SIGRTMIN` / `SIGRTMAX` | 対応 | 制限あり | vsomeip の ENABLE\_SIGNAL\_HANDLING=OFF で回避 |

#### シリアル通信

| 機能 | Linux | QNX 8.0 | 対処 |
| --- | --- | --- | --- |
| `asm/termbits.h` | あり | **なし** | `<termios.h>` に変更 |
| `CRTSCTS` (ハードウェアフロー制御) | あり | **なし** | `IHFLOW \| OHFLOW` で代替 |

#### 型定義

| 型 | Linux (`sys/types.h`) | QNX 8.0 | 対処 |
| --- | --- | --- | --- |
| `u_int8_t`, `u_int32_t` 等 | BSD 互換型あり | **なし** | `uint8_t`, `uint32_t`（stdint.h）に置き換え |

### ビルドツールの違い

| 項目 | Linux (aarch64 クロス) | QNX aarch64le |
| --- | --- | --- |
| コンパイラ | `aarch64-linux-gnu-g++` | `ntoaarch64-g++` ※ `le`/`be` サフィックスなし |
| アーカイバ | `aarch64-linux-gnu-ar` | `ntoaarch64-ar` |
| 動的リンカ | `/lib/ld-linux-aarch64.so.1` | `/usr/lib/ldqnx-64.so.2` |
| マクロ | `__linux__` | `__QNX__`, `__QNXNTO__` |
| ターゲット OS 名 (cmake) | `Linux` | `QNX` |

#### Boost b2 ビルドの注意点

```bash
# NG: qcc toolset は b2 5.x でセグメントフォルトする
./b2 toolset=qcc target-os=qnxnto ...    # クラッシュ

# OK: gcc-ntoaarch64 として登録する
using gcc : ntoaarch64 : ntoaarch64-g++ ;
./b2 toolset=gcc-ntoaarch64 target-os=qnxnto ...
```

また、vsomeip は共有ライブラリ（`.so`）にリンクするため、Boost は **`-fPIC` 付き** でビルドが必要です。

### ファイルシステム・パスの違い

| 項目 | Linux | QNX |
| --- | --- | --- |
| `/proc` | 豊富（`/proc/cpuinfo` 等） | 一部のみ（`/proc/<pid>/` 等） |
| `/dev/loop*` | あり（ループデバイス） | **なし** → `devb-ram` で代替 |
| `/dev/null`, `/dev/zero` | あり | あり |
| 設定ファイル置き場 | `/etc/` | `/etc/` または `/tmp/` |
| ランタイムデータ | `/var/run/` | `/dev/shmem/` または `/tmp/` |

### vsomeip 動作上の制限

| 機能 | Linux | QNX 8.0 (本プロジェクト) |
| --- | --- | --- |
| ネットワーク I/F へのバインド (`SO_BINDTODEVICE`) | 対応 | **無効**（スタブ） |
| SOME/IP TCP/UDP 通信 | ○ | ○ |
| Service Discovery（マルチキャスト） | ○ | ○（`io-pkt` 必要） |
| UNIX ドメインソケット（ローカル IPC） | ○ | ○ |
| DLT ログ | ○ | **無効**（`DISABLE_DLT=ON`） |
| SIGTERM ハンドラ | ○ | **無効**（`ENABLE_SIGNAL_HANDLING=OFF`） |

---

## AUTOSAR AP および各ミドルウェアの機能的差異

QNX ターゲット向けビルドでは、POSIX 非互換・OS アーキテクチャの違いにより、
Linux 版と比べていくつかの機能が無効化・制約されています。

---

### vsomeip（SOME/IP ミドルウェア）

vsomeip は QNX 8.0 向けにクロスビルド・リンクが成功しており、基本的な SOME/IP 通信は動作します。
ただし以下の機能が無効化または制限されています。

#### ビルド時フラグ

| cmake フラグ | QNX での設定 | 理由 |
| --- | --- | --- |
| `ENABLE_SIGNAL_HANDLING` | **OFF** | `sys/signalfd.h` が QNX に存在しないため |
| `DISABLE_DLT` | **ON** | QNX には DLT デーモンが存在しないため |
| `ENABLE_SHM` | OFF（デフォルト） | iceoryx SHM transport は別途 RouDi 起動が必要 |

#### 機能差異の詳細

| 機能 | Linux | QNX（本プロジェクト） | 影響 |
| --- | --- | --- | --- |
| **NIC バインド** (`SO_BINDTODEVICE`) | 動作 | **スタブ（常に false）** | 複数 NIC 環境で特定 IF への binding が不可。unicast IP での通信は可能 |
| **SIGTERM / SIGINT 自動ハンドル** | routing manager が自動処理 | **無効**（`ENABLE_SIGNAL_HANDLING=OFF`） | アプリ側で終了処理を実装する必要あり |
| **DLT ログ** | vsomeip → DLT デーモン | **無効**（コンソールログのみ） | vsomeip ログは stdout/stderr に出力 |
| **SOME/IP TCP/UDP 通信** | 動作 | **動作** | 差異なし |
| **Service Discovery（マルチキャスト）** | 動作 | **動作**（`io-pkt` 起動が必要） | ネットワークドライバが userspace プロセス |
| **UNIX ドメインソケット（プロセス間 IPC）** | 動作 | **動作** | vsomeip クライアント ↔ routing manager 間の通信 |
| **SHM ローカルトランスポート** | 動作 | **未確認**（RouDi 必要） | iceoryx 連携は別途検証が必要 |

#### vsomeip 設定上の注意

- QNX では `unicast` フィールドに **ターゲットの実 IP アドレス** を明示的に指定する必要があります（`0.0.0.0` 等は使用不可）
- デプロイイメージ作成時に `--target-ip` で指定した IP が `vsomeip_routing.json` および `vsomeip_client.json` に自動的に書き込まれます
- ルーティングマネージャ名は `autosar_vsomeip_routing_manager` で固定（`startup.sh` でも同様）

---

### CycloneDDS

CycloneDDS は QNX 向けに UDP ネットワーク通信モードでビルドされています。

| 機能 | Linux | QNX（本プロジェクト） | 状態 |
| --- | --- | --- | --- |
| **UDP マルチキャスト Discovery** | 動作 | **動作**（`io-pkt` 起動が必要） | DDS Participant Discovery が可能 |
| **UDP ユニキャスト通信** | 動作 | **動作** | Pub/Sub が可能 |
| **iceoryx SHM トランスポート** | 動作（RouDi 必要） | **制限あり** | `ddsi_shm_transport.h` の iceoryx 依存ヘッダがバージョン付き include パスを要求 |
| **TCP チャネル** | 動作 | 動作 | QNX ソケット実装経由 |
| **idlc（IDL コンパイラ）** | ホストビルド版を使用 | ホストビルド版を使用 | クロスビルド不要（コード生成はホストで実行） |

#### iceoryx SHM トランスポート利用時の注意

`-DENABLE_SHM=ON` でビルドした場合、`ddsi_shm_transport.h` が `iceoryx_binding_c/chunk.h` を
include します。このヘッダは iceoryx の **バージョン付き include ディレクトリ**
（例: `/opt/qnx/iceoryx/include/iceoryx/v2.0.6/`）に存在するため、
ビルドスクリプトが自動的に `-I` フラグを追加します。

---

### iceoryx（Shared Memory IPC）

iceoryx は QNX 向けにビルド・インストール済みですが、runtime に RouDi デーモンの起動が必要です。

| 項目 | Linux | QNX | 説明 |
| --- | --- | --- | --- |
| **共有メモリパス** | `/dev/shm/` | `/dev/shmem/` | QNX の POSIX 共有メモリ実装 |
| **RouDi デーモン** | 必要 | 必要（同様） | `bin/autosar_iceoryx_roudi` として同梱予定 |
| **include パス** | `include/iceoryx/` | `include/iceoryx/v2.0.6/` | バージョン付きサブディレクトリに配置 |
| **ゼロコピー IPC** | 動作 | 動作 | mmap ベースの SHM はQNX でも利用可能 |

---

### ara::com（AUTOSAR Communication API）

`ara::com` は vsomeip / iceoryx / CycloneDDS のいずれかをトランスポートバックエンドとして使用します。
QNX 向けビルドでは以下のバックエンドが利用可能です。

| バックエンド | QNX でのビルド | 動作確認 | 備考 |
| --- | --- | --- | --- |
| **vsomeip（SOME/IP）** | **○** | ○（routing manager 起動が必要） | SO_BINDTODEVICE 制限あり |
| **iceoryx（SHM）** | **○** | RouDi 起動が必要 | プロセス間通信での使用 |
| **CycloneDDS（DDS）** | **○** | ○（io-pkt 起動が必要） | UDP/マルチキャスト Discovery |

---

### AUTOSAR AP プラットフォームデーモン

以下の 14 本のデーモンが `/opt/qnx/autosar_ap/aarch64le/bin/` にビルド・インストールされています。

| デーモン | 機能 | QNX での状態 |
| --- | --- | --- |
| `adaptive_autosar` | AUTOSAR AP メインプロセス（SM/Exec 統合） | ○ |
| `autosar_vsomeip_routing_manager` | vsomeip Routing Manager | ○（最初に起動が必要） |
| `autosar_dlt_daemon` | DLT（Diagnostic Log and Trace）デーモン | △（QNX DLT 受信端未確認） |
| `autosar_watchdog_supervisor` | ウォッチドッグ監視 | ○ |
| `autosar_network_manager` | ネットワーク管理（NM モジュール） | ○ |
| `autosar_time_sync_daemon` | 時刻同期（NTP/PTP） | ○ |
| `autosar_sm_state_daemon` | State Management 状態通知 | ○ |
| `autosar_iam_policy_loader` | Identity and Access Management ポリシーロード | ○ |
| `autosar_persistency_guard` | パーシステンシーデータ保護 | ○ |
| `autosar_ucm_daemon` | Update and Config Management | ○ |
| `autosar_user_app_monitor` | ユーザーアプリ監視 | ○ |
| その他（tools 類） | ログビューア・診断ツール等 | 用途に応じて使用 |

#### 推奨起動順序

```sh
# 1. vsomeip ルーティングマネージャ（他の全プロセスが依存）
VSOMEIP_CONFIGURATION=/autosar/etc/vsomeip_routing.json \
  /autosar/bin/autosar_vsomeip_routing_manager &
sleep 1

# 2. プラットフォームサービス群（順不同）
/autosar/bin/autosar_dlt_daemon &
/autosar/bin/autosar_watchdog_supervisor &
/autosar/bin/autosar_network_manager &
/autosar/bin/autosar_time_sync_daemon &
/autosar/bin/autosar_sm_state_daemon &

# 3. メインプロセス
/autosar/bin/adaptive_autosar &
```

`startup.sh --all` はこの順序を自動的に実行します。

---

### Linux ネイティブ版との機能差異まとめ

| 機能領域 | Linux ネイティブ | QNX ターゲット |
| --- | --- | --- |
| SOME/IP 通信（vsomeip） | 完全動作 | **基本動作**（SO_BINDTODEVICE・DLT・シグナルハンドラ無効） |
| DDS 通信（CycloneDDS） | 完全動作 | **基本動作**（UDP ユニキャスト・マルチキャスト） |
| SHM IPC（iceoryx） | 完全動作（RouDi 必要） | **動作**（`/dev/shmem` 使用、RouDi 必要） |
| ara::com Pub/Sub | 完全動作 | **動作**（バックエンド選択可能） |
| DLT ログ収集 | 対応 | **コンソール出力のみ**（DLT デーモンは同梱・動作未確認） |
| シグナル処理（SIGTERM 等） | 自動 | **アプリ側で実装が必要** |
| マルチキャスト SD | 対応 | **対応**（`io-pkt` userspace ドライバ必要） |

---

## 既知の制限事項まとめ

| 項目 | 内容 | 対処方法 |
| --- | --- | --- |
| `SO_BINDTODEVICE` | QNX 非対応 | スタブ実装（常に `false`） |
| `SA_RESTART` | ヘッダで未定義 | `-DSA_RESTART=0x0040` をコンパイルフラグに追加 |
| `libpthread` | QNX 8.0 では libc に統合 | 空の `.a` スタブを自動生成 |
| `sys/signalfd.h` | Linux 固有 | `pipe()` ベースの代替実装 |
| `sys/sockmsg.h` | QNX 8.0 で削除 | ネイティブ `accept4()` に置き換え |
| `CRTSCTS` | QNX では `IHFLOW`/`OHFLOW` | 条件コンパイルで代替 |
| `epoll` | Linux 固有 | `poll()` ベースの代替実装 |
| `u_int*_t` 型 | QNX には BSD 互換型なし | `uint*_t`（stdint.h）に置き換え |
| Boost b2 + qcc | b2 5.x でセグメントフォルト | `gcc-ntoaarch64` toolset を使用 |
