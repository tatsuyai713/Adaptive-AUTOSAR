# Adaptive-AUTOSAR (日本語)
![CI](https://github.com/tatsuyai713/Adaptive-AUTOSAR/actions/workflows/cmake.yml/badge.svg)

Linux 向けの教育用途 Adaptive AUTOSAR 風 API 実装です。

言語:
- English: `README.md`
- Japanese: `README.ja.md`

## 対応範囲とバージョン
- 本リポジトリでの ARXML スキーマ基準: `http://autosar.org/schema/r4.0` (`autosar_00050.xsd`)
- 本実装は Adaptive AUTOSAR 風 API/挙動のサブセットを対象とした教育用途実装です。
- 公式の AUTOSAR 適合認証を主張するものではありません。

## QuickStart (ビルド -> インストール -> ユーザーアプリビルド/実行)
以下の手順は **2026-02-15** に Docker 環境で実行確認済みです。

### 前提
- C++14 コンパイラ
- CMake >= 3.14
- Python3 + `PyYAML`
- OpenSSL 開発パッケージ (`libcrypto`)
- ミドルウェア (本リポジトリ既定パス):
  - vSomeIP: `/opt/vsomeip`
  - iceoryx: `/opt/iceoryx`
  - Cyclone DDS (+ idlc): `/opt/cyclonedds`

### 0) 依存ライブラリとミドルウェアを導入 (Linux / Raspberry Pi)
以下の同梱スクリプトを利用します:

```bash
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_middleware_stack.sh
```

1 コマンドでまとめて導入する場合:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

### QNX800 クロスビルド
QNX SDP 8.0 向けクロスコンパイル手順は以下を参照してください:
- `qnx/README.md`
- `qnx/env/qnx800.env.example`

### 1) AUTOSAR AP ランタイムをビルドしてインストール
まずは root 権限不要な `/tmp` 例:

```bash
./scripts/build_and_install_autosar_ap.sh \
  --prefix /tmp/autosar_ap \
  --build-dir build-install-autosar-ap
```

本番相当で `/opt` に入れる場合:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

ミドルウェアが未導入の場合は、ビルドスクリプトから先に導入できます:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh \
  --prefix /opt/autosar_ap \
  --install-middleware \
  --install-base-deps
```

`build_and_install_autosar_ap.sh` は既定でプラットフォーム実行バイナリ
(`adaptive_autosar`) もビルドします。ライブラリのみを意図する場合のみ
`--without-platform-app` を使ってください。

### 2) インストール済みランタイムのみ参照して user_apps をビルド

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt
```

### 3) 既定スモークアプリを実行

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt-run \
  --run
```

`--run` では以下を実行します:
- `autosar_user_minimal_runtime`
- `autosar_user_per_phm_demo`

### 4) 追加テンプレートを手動実行 (任意)

```bash
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_runtime_lifecycle
./build-user-apps-opt/src/apps/feature/can/autosar_user_tpl_can_socketcan_receiver --can-backend=mock
```

### Raspberry Pi ECU プロファイル (Linux + systemd)
Raspberry Pi Linux マシンをプロトタイプ ECU として動作させるための
ビルド/配備/検証スクリプトを同梱しています。

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware

sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
sudo ./scripts/install_rpi_ecu_services.sh --prefix /opt/autosar_ap --user-app-build-dir /opt/autosar_ap/user_apps_build --enable

./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

詳細ランブック:
- `deployment/rpi_ecu/README.md`

ユーザーアプリ起動の導線:
- `/etc/autosar/bringup.sh` (ユーザーが自分の起動コマンドを記述)
- `autosar-vsomeip-routing.service` (SOME/IP ルーティングマネージャを常駐起動)
- `autosar-time-sync.service` (時刻同期の常駐サポートデーモン)
- `autosar-persistency-guard.service` (永続化ストレージ同期の常駐ガードデーモン)
- `autosar-iam-policy.service` (IAM ポリシー常駐ローダデーモン)
- `autosar-can-manager.service` (SocketCAN インタフェース常駐管理デーモン)
- `autosar-platform-app.service` (プラットフォーム側の常駐プロセス群を先に起動)
- `autosar-exec-manager.service` (`bringup.sh` を起動する常駐サービス)
- `autosar-user-app-monitor.service` (登録済みユーザーアプリ/ハートビート/`ara::phm::HealthChannel` 状態を常時監視し、起動猶予・バックオフ・Deactivated停止制御付きで再起動リカバリを実行)
- `autosar-watchdog.service` (常駐ウォッチドッグ監視デーモン)
- `autosar-sm-state.service` (SM マシン状態/ネットワーク通信モード管理の常駐デーモン)
- `autosar-ntp-time-provider.service` (NTP 時刻同期プロバイダ常駐デーモン — chrony/ntpd 自動検出)
- `autosar-ptp-time-provider.service` (PTP/gPTP 時刻同期プロバイダ常駐デーモン — ptp4l PHC 統合)
- 登録台帳ファイル: `/run/autosar/user_apps_registry.csv`
- 監視ステータスファイル: `/run/autosar/user_app_monitor.status`
- PHM ヘルスファイル: `/run/autosar/phm/health/*.status`

### Vector/ETAS/EB 向け資産を移植して使う
本実装は、ベンダ実装で開発した C++ 資産を「ソース互換で再ビルド」して、
他 UNIT と通信確認する用途に利用できます。

移植手順:
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`

## 実装機能マトリクス (AUTOSAR AP 全体仕様に対する位置付け)
ステータス定義:
- `実装あり (一部)`: リポジトリ内に実装あり。ただし AUTOSAR 仕様全体としては部分対応。
- `未実装`: 現在のリポジトリ対象外。
- **カバレッジ %** は AUTOSAR AP SWS 仕様の各機能クラスタに対する主要標準 API クラス/メソッドの実装比率の概算です。あくまでも目安としてご参照ください。

本リポジトリの準拠ポリシー:
- AUTOSAR AP 標準に対応するAPIは、標準形のシグネチャを維持します。
- プラットフォーム側/ユーザーアプリ側のサンプル実装は、原則として標準名前空間 (`ara::<domain>::*`) を優先し、拡張 alias を既定では使用しません。
- 非標準機能は拡張として分離し（主に `ara::<domain>::extension`）、標準APIの置換としては扱いません。
- 拡張 API の入口ヘッダ:
  - `src/ara/com/extension/non_standard.h`
  - `src/ara/diag/extension/non_standard.h`
  - `src/ara/exec/extension/non_standard.h`
  - `src/ara/phm/extension/non_standard.h`

| AUTOSAR AP 領域 | カバレッジ | 状態 | このリポジトリで提供されるもの | 未対応/備考 |
| --- | :---: | --- | --- | --- |
| `ara::core` | ~50 % | 実装あり (一部) | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, init/deinit, 初期化状態照会 API | `Variant`, `String`/`StringView`, コンテナラッパー (`Vector`/`Map`/`Array`/`Span`), `SteadyClock`, `CoreErrorDomain`, 例外クラス階層 |
| `ara::log` | ~75 % | 実装あり (一部) | `Logger`（全重要度レベル + `WithLevel` ログレベルフィルタリング）、`LogStream`、`LoggingFramework`、3 種シンク (console/file/network)、実行時 LogLevel 上書き/照会 | DLT バックエンド統合、`LogHex`/`LogBin` フォーマッタ |
| `ara::com` 共通 API | ~60 % | 実装あり (一部) | `Event`/`Method`/`Field` ラッパー、`ServiceProxyBase`/`ServiceSkeletonBase`、Subscribe/Unsubscribe、handler、`SamplePtr`、シリアライズフレームワーク、複数同時 `StartFindService`/handle 指定 `StopFindService`、Field 能力フラグ準拠動作 | 生成 API スタブ、communication group、SWS 全角ケース |
| `ara::com` SOME/IP | ~60 % | 実装あり (一部) | SOME/IP SD client/server、pub/sub、RPC client/server (vSomeIP backend) | すべての SOME/IP/AP オプション・エッジ動作は未対応 |
| `ara::com` DDS | ~40 % | 実装あり (一部) | Cyclone DDS wrapper (`ara::com::dds`) による pub/sub | QoS/運用プロファイルは部分対応 |
| `ara::com` ZeroCopy | ~40 % | 実装あり (一部) | iceoryx wrapper (`ara::com::zerocopy`) による pub/sub | バックエンド隠蔽実装。AUTOSAR 全標準化範囲を完全網羅するものではない |
| `ara::com` E2E | ~30 % | 実装あり (一部) | E2E Profile11 (`TryProtect`/`Check`/`TryForward`) + event decorator | 他プロファイル (P01/P02/P04/P05/P07) は未実装 |
| `ara::exec` | ~55 % | 実装あり (一部) | `ExecutionClient`/`ExecutionServer`、`StateServer`、`DeterministicClient`、`FunctionGroup`/`FunctionGroupState`、`SignalHandler`、`WorkerThread`、拡張 `ProcessWatchdog`（起動猶予/連続期限切れ/コールバッククールダウン/期限切れ回数）、実行状態変更コールバック | EM フルオーケストレーション、`StateClient` 標準インタフェース |
| `ara::diag` | ~70 % | 実装あり (一部) | `Monitor`（デバウンス・FDC 照会・現在ステータス照会）、`Event`、`Conversation`、`DTCInformation`、`Condition`、`OperationCycle`、`GenericUDSService`、`SecurityAccess`、`GenericRoutine`、`DataTransfer` (Upload/Download)、カウンタ/タイマー方式デバウンス付き routing | Diagnostic Manager フルオーケストレーション、全 UDS サブファンクション |
| `ara::phm` | ~50 % | 実装あり (一部) | `SupervisedEntity`、`HealthChannel`（`Offer`/`StopOffer` ライフサイクル）、`RecoveryAction`、`CheckpointCommunicator`、supervision helper | `AliveSupervision`/`DeadlineSupervision`/`LogicalSupervision` 詳細 SWS API |
| `ara::per` | ~65 % | 実装あり (一部) | `KeyValueStorage`、`FileStorage`、`ReadAccessor`（`Seek`/`GetCurrentPosition`）、`ReadWriteAccessor`（`Seek`/`GetCurrentPosition`） | `SharedHandle`/`UniqueHandle` 標準ラッパー、冗長化/リカバリポリシー |
| `ara::sm` | ~55 % | 実装あり (一部) | `TriggerIn`/`TriggerOut`/`TriggerInOut`、`Notifier`、`SmErrorDomain`、`MachineStateClient`（ライフサイクル状態管理）、`NetworkHandle`（通信モード制御）、`StateTransitionHandler`（ファンクショングループ遷移コールバック）、SM 状態常駐デーモン | EM 連携の状態マネージャフルオーケストレーション、診断状態連動 |
| ARXML ツール | — | 実装あり (一部) | YAML -> ARXML、ARXML -> ara::com binding 生成 | リポジトリ対象スコープ中心で全 ARXML 網羅ではない |
| `ara::crypto` | ~15 % | 実装あり (一部) | エラードメイン、SHA-256 ダイジェスト、HMAC、AES 暗号化、鍵生成、乱数バイト生成 | 鍵管理/PKI スタック、`CryptoProvider`、X.509、SecOC |
| `ara::iam` | ~20 % | 実装あり (一部) | メモリ内 IAM ポリシー判定（subject/resource/action、ワイルドカード）、エラードメイン | ポリシー永続化、プラットフォーム IAM 連携、グラント管理 |
| `ara::ucm` | ~40 % | 実装あり (一部) | UCM エラードメイン、更新セッション管理 (`Prepare`/`Stage`/`Verify`/`Activate`/`Rollback`/`Cancel`)、Transfer API (`TransferStart`/`TransferData`/`TransferExit`)、SHA-256 検証、状態/進捗コールバック、クラスタ別バージョン管理とダウングレード拒否 | campaign 管理、installer daemon、secure boot 連携 |
| 時刻同期 (`ara::tsync`) | ~50 % | 実装あり (一部) | `TimeSyncClient`（参照時刻更新、同期時刻変換、オフセット/状態照会、状態変更通知コールバック）、`SynchronizedTimeBaseProvider` 抽象インタフェース、`PtpTimeBaseProvider`（ptp4l/gPTP PHC 統合、`/dev/ptpN`）、`NtpTimeBaseProvider`（chrony/ntpd 自動検出統合）、エラードメイン、PTP/NTP プロバイダ常駐デーモン | `TimeSyncServer`、レート補正、TSP SWS 全プロファイル対応 |
| Raspberry Pi ECU 配備プロファイル | — | 実装あり (一部) | ビルド/インストール統合スクリプト、SocketCAN セットアップ、systemd テンプレート、統合検証スクリプト、常駐デーモン (`vsomeip-routing`/`time-sync`/`persistency-guard`/`iam-policy`/`can-manager`/`user-app-monitor`/`watchdog`/`sm-state`/`ntp-time-provider`/`ptp-time-provider`) | Linux 上のプロトタイプ ECU 運用を対象。量産向け安全/セキュリティ強化は別途システム統合が必要 |

## user_apps テンプレート (インストール先: `/opt|/tmp/autosar_ap/user_apps`)
- Basic:
  - `autosar_user_minimal_runtime`
  - `autosar_user_exec_signal_template`
  - `autosar_user_per_phm_demo`
- Communication:
  - `autosar_user_com_someip_provider_template`
  - `autosar_user_com_someip_consumer_template`
  - `autosar_user_com_zerocopy_pub_template`
  - `autosar_user_com_zerocopy_sub_template`
  - `autosar_user_com_dds_pub_template`
  - `autosar_user_com_dds_sub_template`
- Feature:
  - `autosar_user_tpl_runtime_lifecycle`
  - `autosar_user_tpl_can_socketcan_receiver`
  - `autosar_user_tpl_ecu_full_stack`
  - `autosar_user_tpl_ecu_someip_source`

参照:
- `user_apps/README.md`
- `user_apps/tutorials/README.ja.md`
- `deployment/rpi_ecu/README.md`
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md` (Vector/ETAS/EB 向け資産の移植手順)
- `tools/host_tools/doip_diag_tester/README.ja.md` (Ubuntu 側 DoIP/DIAG host テスター手順)

## Host 側診断ツール (ECU 実機内アプリではない)
- バイナリ: `autosar_host_doip_diag_tester`
- ソース: `tools/host_tools/doip_diag_tester/doip_diag_tester_app.cpp`
- ドキュメント:
  - `tools/host_tools/doip_diag_tester/README.md`
  - `tools/host_tools/doip_diag_tester/README.ja.md`
- ビルド:
```bash
cmake -S tools/host_tools/doip_diag_tester -B build-host-doip-tester
cmake --build build-host-doip-tester -j"$(nproc)"
```

## ARXML / コード生成
### YAML -> ARXML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub.generated.arxml \
  --overwrite \
  --print-summary
```

### ARXML -> ara::com binding ヘッダ

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input /tmp/pubsub.generated.arxml \
  --output /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

詳細:
- `tools/arxml_generator/README.md`
- `tools/arxml_generator/YAML_MANUAL.ja.md`

## GitHub Actions の検証内容
ワークフロー: `.github/workflows/cmake.yml`

現在の CI では次を検証します:
1. 依存ミドルウェアのビルド/インストール (vSomeIP, iceoryx, Cyclone DDS)
2. 全バックエンド有効で configure/build (`build_tests=ON`, `AUTOSAR_AP_BUILD_SAMPLES=OFF`)
3. 単体テスト (`ctest`) 実行
4. ARXML 生成および ARXML ベースの binding 生成チェック
5. 分離インストールフロー (`build_and_install_autosar_ap.sh`)
6. インストール済みパッケージに対する外部 user_apps ビルド/実行 (`build_user_apps_from_opt.sh --run`)
7. Doxygen API ドキュメント生成 (`./scripts/generate_doxygen_docs.sh`) と artifact (`doxygen-html`) 保存
8. `main`/`master` への push 時に生成ドキュメントを GitHub Pages へ公開

## ドキュメント
- GitHub Pages API ドキュメント: `https://tatsuyai713.github.io/Adaptive-AUTOSAR/`
- ローカル/API ドキュメント生成: `./scripts/generate_doxygen_docs.sh` -> `docs/index.html` (`docs/api/index.html`)
- Wiki: https://github.com/langroodi/Adaptive-AUTOSAR/wiki
- コントリビューション: `CONTRIBUTING.md`
