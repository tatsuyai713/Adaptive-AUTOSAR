# Adaptive-AUTOSAR (日本語)
![CI](https://github.com/tatsuyai713/Adaptive-AUTOSAR/actions/workflows/cmake.yml/badge.svg)

Linux 向けの教育用途 Adaptive AUTOSAR 風 API 実装です。

言語:
- English: `README.md`
- Japanese: `README.ja.md`

## 概要

本リポジトリは、Linux 環境で動作する AUTOSAR Adaptive Platform API の実装を提供します。以下の 2 つの用途で利用できます。

1. **Linux シミュレーション環境** — デスクトップ Linux や Docker 上で、実機 ECU なしに AUTOSAR Adaptive アプリケーションを開発・テスト
2. **ECU デプロイ** — Raspberry Pi（または QNX ターゲット）をプロトタイプ ECU として使用。systemd サービス管理、SocketCAN、時刻同期などを含む

### このリポジトリでできること

- **15 の `ara::*` モジュール**を完全実装（R24-11 ベースライン、SWS API カバレッジ 100%）
- **3 種の通信トランスポート**: SOME/IP (vSomeIP)、DDS (CycloneDDS)、ゼロコピー IPC (iceoryx) — 実行時に切り替え可能
- **E2E 保護**: Profile 01〜07 および 11、状態機械とサンプル単位ステータス付き
- **SecOC**: フレッシュネス管理、HMAC ベース MAC、鍵ローテーション
- **診断**: UDS サービスフルスタック (0x10〜0x3E)、OBD-II、DoIP ホストテスター
- **ARXML ツールチェーン**: YAML → ARXML → C++ proxy/skeleton/binding ヘッダ生成
- **ユーザーアプリテンプレート**: 13 種のテンプレートとステップバイステップチュートリアル
- **ベンダ資産移植**: Vector/ETAS/EB で開発した C++ 資産をソースレベルで再ビルド可能

### 対応範囲とバージョン

- ARXML スキーマ基準: `http://autosar.org/schema/r4.0` (`autosar_00050.xsd`)
- 教育用途実装です。公式の AUTOSAR 適合認証を主張するものではありません。
- 非標準機能は `ara::<domain>::extension` 配下に拡張として提供し、標準 API の置換としては扱いません。

---

## 前提条件

- C++14 コンパイラ
- CMake >= 3.14
- Python3 + `PyYAML`
- OpenSSL 開発パッケージ (`libcrypto`)

ミドルウェア（同梱スクリプトで自動ビルド）:

| ライブラリ | デフォルトパス | 備考 |
|---|---|---|
| iceoryx v2.0.6 | `/opt/iceoryx` | コンテナ環境向け ACL パッチ適用済み |
| CycloneDDS 0.10.5 | `/opt/cyclonedds` | iceoryx 経由で SHM 自動有効化 |
| vsomeip 3.4.10 + CDR | `/opt/vsomeip` | CycloneDDS-CXX から CDR 抽出; 実行時 DDS ランタイム不要 |

---

## クイックスタート: Linux シミュレーション環境

Linux マシンまたは Docker コンテナ上で AUTOSAR AP アプリケーションをビルド・インストール・実行する手順です。

### 1. 依存ライブラリとミドルウェアの導入

```bash
# 1 コマンドで一括導入（システムパッケージ + 全ミドルウェア）
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

または段階的に:

```bash
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_middleware_stack.sh
```

### 2. AUTOSAR AP ランタイムのビルドとインストール

```bash
# /tmp にクイックインストール（root 権限不要）
./scripts/build_and_install_autosar_ap.sh \
  --prefix /tmp/autosar_ap \
  --build-dir build-install-autosar-ap
```

本番相当のレイアウト:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

ミドルウェア未導入の場合はビルドスクリプトから一括導入可能:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh \
  --prefix /opt/autosar_ap \
  --install-middleware \
  --install-base-deps
```

### 3. ユーザーアプリケーションのビルド

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt
```

### 4. スモークテストの実行

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt-run \
  --run
```

`autosar_user_minimal_runtime` と `autosar_user_per_phm_demo` が実行されます。

### 5. 通信トランスポート切り替え Pub/Sub の確認（DDS / iceoryx / SOME/IP）

3 種のトランスポートプロファイルをまとめてスモーク確認:

```bash
./scripts/build_switchable_pubsub_sample.sh --run-smoke
```

同一バイナリで手動切り替え:

```bash
# DDS プロファイル
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub

# iceoryx プロファイル
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_iceoryx.yaml
iox-roudi &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub

# SOME/IP プロファイル
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=/opt/autosar_ap/configuration/vsomeip-rpi.json
/opt/autosar_ap/bin/autosar_vsomeip_routing_manager &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub
```

---

## ECU デプロイ: Raspberry Pi

Raspberry Pi をプロトタイプ ECU として動作させるための、systemd サービス管理を含むデプロイプロファイルを同梱しています。

### デプロイされるもの

ECU プロファイルは AUTOSAR AP ランタイムを構成する 14 の systemd サービスをインストールします:

| サービス | 役割 |
|---|---|
| `autosar-platform-app` | プラットフォーム側常駐プロセス群 |
| `autosar-exec-manager` | ユーザーアプリ起動 (`bringup.sh`) |
| `autosar-vsomeip-routing` | SOME/IP ルーティングマネージャ |
| `autosar-can-manager` | SocketCAN インタフェース管理 |
| `autosar-time-sync` | 時刻同期サポート |
| `autosar-ntp-time-provider` | NTP プロバイダ (chrony/ntpd 自動検出) |
| `autosar-ptp-time-provider` | PTP/gPTP プロバイダ (ptp4l PHC) |
| `autosar-persistency-guard` | 永続化ストレージ同期ガード |
| `autosar-iam-policy` | IAM ポリシーローダ |
| `autosar-user-app-monitor` | ユーザーアプリ監視・再起動リカバリ |
| `autosar-watchdog` | ウォッチドッグ監視 |
| `autosar-sm-state` | マシン状態/ネットワークモード管理 |

ユーザーアプリは `/etc/autosar/bringup.sh` 経由で起動されます。

### デプロイ手順

```bash
# 1. ランタイム + ユーザーアプリのビルドとミドルウェア導入
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware

# 2. SocketCAN インタフェースのセットアップ
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000

# 3. systemd サービスのインストールと有効化
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable

# 4. デプロイの検証
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

詳細ガイド:
- [`deployment/rpi_ecu/README.md`](deployment/rpi_ecu/README.md)
- [`deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md`](deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md) (gPTP 設定を含む日本語版)

---

## QNX 8.0 クロスビルド

QNX SDP 8.0 ターゲット向けに、全ミドルウェアおよび AUTOSAR AP ランタイムのクロスコンパイルスクリプトを提供しています。

```bash
source ~/qnx800/qnxsdp-env.sh
sudo ./scripts/install_middleware_stack_qnx.sh --arch aarch64le --enable-shm
```

詳細は [`qnx/README.md`](qnx/README.md) を参照してください。

---

## ユーザーアプリテンプレート

テンプレートは `<prefix>/user_apps` にインストールされ、基本・通信・フルスタックの各ユースケースをカバーします。

| カテゴリ | テンプレート | 説明 |
|---|---|---|
| Basic | `autosar_user_minimal_runtime` | 最小限のランタイム init/deinit |
| Basic | `autosar_user_exec_signal_template` | シグナルハンドリング |
| Basic | `autosar_user_per_phm_demo` | 永続化 + ヘルスモニタリング |
| Communication | `autosar_user_com_someip_provider_template` | SOME/IP パブリッシャ |
| Communication | `autosar_user_com_someip_consumer_template` | SOME/IP サブスクライバ |
| Communication | `autosar_user_com_zerocopy_pub_template` | iceoryx ゼロコピーパブリッシャ |
| Communication | `autosar_user_com_zerocopy_sub_template` | iceoryx ゼロコピーサブスクライバ |
| Communication | `autosar_user_com_dds_pub_template` | DDS パブリッシャ |
| Communication | `autosar_user_com_dds_sub_template` | DDS サブスクライバ |
| Feature | `autosar_user_tpl_runtime_lifecycle` | ランタイムライフサイクル管理 |
| Feature | `autosar_user_tpl_can_socketcan_receiver` | SocketCAN メッセージデコード |
| Feature | `autosar_user_tpl_ecu_full_stack` | フル ECU スタック統合 |
| Feature | `autosar_user_tpl_ecu_someip_source` | SOME/IP ソース ECU |

チュートリアル: [`user_apps/tutorials/`](user_apps/tutorials/)

### ベンダ資産の移植

Vector/ETAS/EB で開発した C++ 資産をソース互換で再ビルドできます。手順は [`user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`](user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md) を参照してください。

---

## ARXML / コード生成

### YAML → ARXML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub.generated.arxml \
  --overwrite --print-summary
```

### ARXML → ara::com binding ヘッダ

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input /tmp/pubsub.generated.arxml \
  --output /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

ガイド: [`tools/arxml_generator/README.md`](tools/arxml_generator/README.md) | [`tools/arxml_generator/YAML_MANUAL.ja.md`](tools/arxml_generator/YAML_MANUAL.ja.md)

---

## ホスト側診断ツール

ホスト PC 上で動作する DoIP/UDS 診断テスターです（ECU 実機内アプリではありません）。

```bash
cmake -S tools/host_tools/doip_diag_tester -B build-host-doip-tester
cmake --build build-host-doip-tester -j"$(nproc)"
```

ドキュメント: [`tools/host_tools/doip_diag_tester/README.ja.md`](tools/host_tools/doip_diag_tester/README.ja.md)

---

## 実装機能マトリクス

リリースベースライン: `R24-11`。全 15 コア `ara::*` モジュールは **SWS API カバレッジ 100%** に到達しています。

ハードウェア依存機能（HSM、生体認証センサー、CAN-TP/FlexRay、TPM セキュアブート等）は、モック/シミュレーションバックエンドによるソフトウェア抽象化として提供しています。

| モジュール | カバレッジ | 主な機能 |
| --- | :---: | --- |
| `ara::core` | 100% | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, `String/StringView` (C++14 ポリフィル), `Variant`, `Span`, `Byte`, モナディックチェーン (`AndThen/OrElse/Map/MapError`), 例外階層, `AbortHandler` |
| `ara::log` | 100% | `Logger` (重要度フィルタリング), `LogStream` 演算子, 3 種シンク (console/file/network), DLT バックエンド, `AsyncLogSink` (リングバッファ), `FormatterPlugin` (JSON/カスタム) |
| `ara::com` 共通 | 100% | Proxy/Skeleton Event/Method/Field ラッパー, `FindService`/`StartFindService`, シリアライズフレームワーク (CDR 等), fire-and-forget メソッド, `CommunicationGroup`, `RawDataStream`, `InstanceIdentifier`, `ServiceVersion`, QoS, `EventCacheUpdatePolicy`, `FilterConfig`, `Transformer` チェーン, `SampleInfo`/`E2ESampleStatus` |
| `ara::com` SOME/IP | 100% | SD client/server, pub/sub, RPC, SOME/IP-TP セグメンテーション/リアセンブリ, Magic Cookie, `SessionHandler`, IPv6 エンドポイント, `SdNetworkConfig` |
| `ara::com` DDS | 100% | CycloneDDS pub/sub, メソッドバインディング (request/reply), QoS 設定, `DdsParticipantFactory`, コンテンツフィルタ, 拡張 QoS (ownership/partition/resource-limits) |
| `ara::com` ZeroCopy | 100% | iceoryx pub/sub, メソッドバインディング, ゼロコピー Loan/Publish, `QueueOverflowPolicy`, `ZeroCopyServiceDiscovery`, `PortIntrospection` |
| `ara::com` E2E | 100% | Profile 01〜07 および 11, event/method デコレータ, `E2EStateMachine` (ウィンドウ方式, 有効/無効制御), サンプル単位ステータス伝搬 |
| `ara::com` SecOC | 100% | `FreshnessManager` (モノトニックカウンタ, 永続化), `SecOcPdu` (HMAC MAC), `SecOcKeyManager`, 一括保護/検証, `FreshnessSyncManager` (ECU 間同期) |
| `ara::exec` | 100% | `ExecutionClient/Server`, `StateClient/Server`, `DeterministicClient`, `FunctionGroup`, `ProcessWatchdog`, `ExecutionManager` (fork/exec ライフサイクル), `ManifestLoader` (ARXML), `ApplicationClient`, QNX 移植性 |
| `ara::diag` | 100% | `Monitor` (デバウンス), UDS サービス (0x10/0x11/0x14/0x22/0x27/0x28/0x2E/0x2F/0x31/0x34〜0x37/0x3E/0x85), OBD-II (Mode 01/09), `EventMemory`, `DiagnosticManager` (P2/P2* タイミング) |
| `ara::phm` | 100% | `SupervisedEntity`, `HealthChannel`, `RecoveryAction`, `AliveSupervision`, `DeadlineSupervision`, `LogicalSupervision`, `PhmOrchestrator` |
| `ara::per` | 100% | `KeyValueStorage`, `FileStorage`, `ReadAccessor`/`ReadWriteAccessor`, recover/reset, バッチ操作, 変更オブザーバ, `RedundantStorage`, `UpdatePersistency` (UCM マイグレーション) |
| `ara::sm` | 100% | `MachineStateClient` (shutdown/restart), `NetworkHandle`, `FunctionGroupStateMachine` (ガード/タイムアウト/履歴), `PowerModeManager`, `DiagnosticStateHandler`, `UpdateRequestHandler` |
| `ara::crypto` | 100% | SHA-1/256/384/512, HMAC, AES-CBC/GCM/CTR, ChaCha20, PBKDF2/HKDF, RSA 2048/4096, ECDSA P-256/P-384, ECDH, X.509 チェーン検証, CRL 失効確認, ストリーミングコンテキスト, `KeySlot`/`KeyStorageProvider`, `CryptoServiceProvider` |
| `ara::nm` | 100% | マルチチャネル NM 状態機械 (5 状態), `NmCoordinator`, `NmPdu` シリアライズ, `CanTpNmAdapter`/`FlexRayNmAdapter` |
| `ara::iam` | 100% | RBAC (`RoleManager`), ABAC (`AbacPolicyEngine`), `Grant`/`GrantManager`, `PolicySigner` (ECDSA), `PasswordStore`, `IdentityManager`, `AuditTrail`, `CapabilityManager` |
| `ara::ucm` | 100% | 更新セッションライフサイクル, Transfer API, SHA-256 検証, `CampaignManager` (マルチパッケージ), `UpdateHistory`, `DependencyChecker`, `SecureBootManager` |
| `ara::tsync` | 100% | `TimeSyncClient` (ドリフト補正, 品質レベル), `PtpTimeBaseProvider` (ptp4l/gPTP), `NtpTimeBaseProvider` (chrony/ntpd), `TimeSyncServer`, `RateCorrector` |
| ARXML ツール | — | YAML → ARXML、ARXML → ara::com binding ヘッダ生成 |
| RPi ECU プロファイル | — | systemd デプロイ、14 常駐デーモン、SocketCAN、時刻同期、ヘルスモニタリング |

### AUTOSAR AP リリースプロファイル

既定: `R24-11`。CMake で設定可能:

```bash
cmake -S . -B build -DAUTOSAR_AP_RELEASE_PROFILE=R24-11
```

`ara::core::ApReleaseInfo` で実行時に参照可能。

---

## ドキュメント

ドキュメント一覧: [`docs/README.ja.md`](docs/README.ja.md) | [`docs/README.md`](docs/README.md)

| テーマ | ドキュメント |
| --- | --- |
| Raspberry Pi ECU 配備 | [`deployment/rpi_ecu/README.md`](deployment/rpi_ecu/README.md) |
| Raspberry Pi gPTP 設定 (日本語) | [`deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md`](deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md) |
| QNX 8.0 クロスコンパイル | [`qnx/README.md`](qnx/README.md) |
| ユーザーアプリのビルドシステム | [`user_apps/README.md`](user_apps/README.md) |
| チュートリアル (ステップバイステップ) | [`user_apps/tutorials/README.ja.md`](user_apps/tutorials/README.ja.md) |
| ARXML ジェネレータガイド | [`tools/arxml_generator/README.md`](tools/arxml_generator/README.md) |
| ホスト DoIP/UDS テスタ | [`tools/host_tools/doip_diag_tester/README.ja.md`](tools/host_tools/doip_diag_tester/README.ja.md) |
| API リファレンス (オンライン) | <https://tatsuyai713.github.io/Adaptive-AUTOSAR/> |
| API リファレンス (ローカル生成) | `./scripts/generate_doxygen_docs.sh` |
| コントリビューションガイド | [`CONTRIBUTING.md`](CONTRIBUTING.md) |

## CI/CD

ワークフロー: [`.github/workflows/cmake.yml`](.github/workflows/cmake.yml)

CI パイプラインの検証内容: ミドルウェアビルド/インストール → テスト付きフルビルド → `ctest` → ARXML 生成 → 分離インストール → 外部ユーザーアプリビルド/実行 → Doxygen 生成 → GitHub Pages 公開。
