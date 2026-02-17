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
- `deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md` (gPTP 詳細設定を含む日本語版総合セットアップマニュアル)

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
| `ara::core` | ~93 % | 実装あり (一部) | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, init/deinit, 初期化状態照会 API, `String`/`StringView`, `Vector`, `Map`/`UnorderedMap`, `Array`, `Span`, `SteadyClock`, `Variant` (C++14 互換型安全ユニオン)、`CoreErrorDomain`（SWS_CORE_10400、ID 0x8000000000000014）、例外階層 (`Exception`/`CoreException`/`InvalidArgumentException`/`InvalidMetaModelShortnameException`/`InvalidMetaModelPathException`) | ランタイムエラー伝播完全統合 |
| `ara::log` | ~92 % | 実装あり (一部) | `Logger`（全重要度レベル + `WithLevel` フィルタリング）、`LogStream`（bool/int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string/char*/ErrorCode/InstanceSpecifier/vector 演算子）、`LoggingFramework`、3 種シンク (console/file/network)、実行時 LogLevel 上書き/照会、DLT バックエンド (`LogMode::kDlt`) UDP/dlt-viewer 統合、`LogHex`/`LogBin` フォーマッタ | カスタムフォーマッタプラグイン |
| `ara::com` 共通 API | ~60 % | 実装あり (一部) | `Event`/`Method`/`Field` ラッパー、`ServiceProxyBase`/`ServiceSkeletonBase`、Subscribe/Unsubscribe、handler、`SamplePtr`、シリアライズフレームワーク、複数同時 `StartFindService`/handle 指定 `StopFindService`、Field 能力フラグ準拠動作 | 生成 API スタブ、communication group、SWS 全角ケース |
| `ara::com` SOME/IP | ~60 % | 実装あり (一部) | SOME/IP SD client/server、pub/sub、RPC client/server (vSomeIP backend) | すべての SOME/IP/AP オプション・エッジ動作は未対応 |
| `ara::com` DDS | ~40 % | 実装あり (一部) | Cyclone DDS wrapper (`ara::com::dds`) による pub/sub | QoS/運用プロファイルは部分対応 |
| `ara::com` ZeroCopy | ~40 % | 実装あり (一部) | iceoryx wrapper (`ara::com::zerocopy`) による pub/sub | バックエンド隠蔽実装。AUTOSAR 全標準化範囲を完全網羅するものではない |
| `ara::com` E2E | ~70 % | 実装あり (一部) | E2E Profile11 (`TryProtect`/`Check`/`TryForward`) + event decorator、E2E Profile 01 (CRC-8 SAE-J1850、DataID 設定可)、E2E Profile 02 (CRC-8H2F、長メッセージ向け)、E2E Profile 04 (CRC-32/AUTOSAR、0xF4ACFB13 反射、6 バイトヘッダ、最強保護)、E2E Profile 05 (CRC-16/ARC、0x8005 反射多項式、最大 4096 バイト、LE 2 バイト CRC ヘッダ) | E2E P07 は未実装 |
| `ara::exec` | ~60 % | 実装あり (一部) | `ExecutionClient`/`ExecutionServer`、`StateServer`、`StateClient`（ファンクショングループ状態 Set/遷移/エラー照会、SOME/IP RPC 経由）、`DeterministicClient`、`FunctionGroup`/`FunctionGroupState`、`SignalHandler`、`WorkerThread`、拡張 `ProcessWatchdog`（起動猶予/連続期限切れ/コールバッククールダウン/期限切れ回数）、実行状態変更コールバック | EM フルオーケストレーション |
| `ara::diag` | ~90 % | 実装あり (一部) | `Monitor`（デバウンス・FDC 照会）、`Event`、`Conversation`、`DTCInformation`、`Condition`、`OperationCycle`、`GenericUDSService`、`SecurityAccess`、`GenericRoutine`、`DataTransfer`、`DiagnosticSessionManager`（S3 タイマー、UDS セッション管理）、`OBD-II Service`（Mode 01/09、12 PID）、`DataIdentifierService`（UDS 0x22/0x2E、マルチ DID 読み取り、DID 単位コールバック）、`CommunicationControl`（UDS 0x28、通信タイプ別 Tx/Rx 制御）、`ControlDtcSetting`（UDS 0x85、DTC 更新 ON/OFF）、`ClearDiagnosticInformation`（UDS 0x14、グループ DTC クリア、0xFFFFFF=全 DTC クリア）、`EcuResetRequest`（UDS 0x11、ソフト/ハード/キーオフ リセット）、`InputOutputControl`（UDS 0x2F、ReturnToEcu/ResetToDefault/FreezeCurrentState/ShortTermAdjustment、状態読み戻し） | Diagnostic Manager フルオーケストレーション |
| `ara::phm` | ~80 % | 実装あり (一部) | `SupervisedEntity`、`HealthChannel`（`Offer`/`StopOffer` ライフサイクル）、`RecoveryAction`、`CheckpointCommunicator`、supervision helper、`AliveSupervision`（周期チェックポイント監視、min/max マージン、失敗/合格閾値、kOk/kFailed/kExpired 状態）、`DeadlineSupervision`（min/max デッドライン窓、失敗/合格閾値、kDeactivated/kOk/kFailed/kExpired 状態）、`LogicalSupervision`（チェックポイント順序グラフ、隣接検証遷移、失敗閾値、allowReset） | PHM オーケストレーションデーモン統合 |
| `ara::per` | ~75 % | 実装あり (一部) | `KeyValueStorage`、`FileStorage`、`ReadAccessor`（`Seek`/`GetCurrentPosition`）、`ReadWriteAccessor`（`Seek`/`GetCurrentPosition`）、`SharedHandle`/`UniqueHandle` ラッパー、`RecoverKeyValueStorage`/`ResetKeyValueStorage`、`RecoverFileStorage`/`ResetFileStorage`、`UpdatePersistency`（UCM スキーママイグレーションフック、SWS_PER_00456） | 冗長化/リカバリポリシーフルオーケストレーション、スキーマバージョン管理ランタイム |
| `ara::sm` | ~65 % | 実装あり (一部) | `TriggerIn`/`TriggerOut`/`TriggerInOut`、`Notifier`、`SmErrorDomain`、`MachineStateClient`（ライフサイクル状態管理）、`NetworkHandle`（通信モード制御）、`StateTransitionHandler`（ファンクショングループ遷移コールバック）、`DiagnosticStateHandler`（SM/Diag セッションブリッジ: セッション入出コールバック、S3 タイムアウト連動、プログラミングセッション分離）、SM 状態常駐デーモン | EM 連携の状態マネージャフルオーケストレーション |
| ARXML ツール | — | 実装あり (一部) | YAML -> ARXML、ARXML -> ara::com binding 生成 | リポジトリ対象スコープ中心で全 ARXML 網羅ではない |
| `ara::crypto` | ~55 % | 実装あり (一部) | エラードメイン、`ComputeDigest`（SHA-1/256/384/512）、`ComputeHmac`、`AesEncrypt`/`AesDecrypt`（AES-CBC、PKCS#7）、`GenerateSymmetricKey`、`GenerateRandomBytes`、`KeySlot`/`KeyStorageProvider`（スロットベース鍵管理、ファイルシステム永続化）、`GenerateRsaKeyPair`/`RsaSign`/`RsaVerify`/`RsaEncrypt`/`RsaDecrypt`（RSA 2048/4096、PKCS#1 v1.5 署名、OAEP 暗号化）、`GenerateEcKeyPair`/`EcdsaSign`/`EcdsaVerify`（ECDSA P-256/P-384）、`ParseX509Pem`/`ParseX509Der`/`VerifyX509Chain`（X.509 証明書パース・チェーン検証） | 完全 PKI スタック、HSM 連携、鍵導出関数 |
| `ara::nm` | ~65 % | 実装あり (一部) | `NetworkManager`（多チャネル NM 状態機械: BusSleep/PrepBusSleep/ReadySleep/NormalOperation/RepeatMessage）、`NetworkRequest`/`NetworkRelease`、`NmMessageIndication`、`Tick` 方式状態遷移、チャネル状態照会、状態変更コールバック、部分ネットワークフラグ、`NmCoordinator`（多チャネルバススリープ/ウェイク調整、AllChannels/Majority ポリシー、スリープ準備コールバック）、`NmTransportAdapter`（抽象 NM PDU トランスポートインタフェース） | AUTOSAR NM コーディネータデーモン（バス固有トランスポートアダプタ付き） |
| `ara::com::secoc` | ~40 % | 実装あり (一部) | `FreshnessManager`（PDU 単位モノトニックカウンタ、リプレイ攻撃防御、VerifyAndUpdate）、`SecOcPdu`（HMAC ベース MAC、切り捨てフレッシュネス/MAC 送信、Protect/Verify）、`SecOcErrorDomain` | 鍵プロビジョニング API、キャンペーンレベルフレッシュネス同期、ハードウェアセキュリティモジュール統合 |
| `ara::iam` | ~40 % | 実装あり (一部) | メモリ内 IAM ポリシー判定（subject/resource/action、ワイルドカード）、エラードメイン、`Grant`/`GrantManager`（時限付きパーミッショントークン、発行/失効/照会/パージ、ファイル永続化）、`PolicyVersionManager`（ポリシースナップショット/ロールバック、バージョン履歴） | プラットフォーム IAM 連携、属性ベースアクセス制御 (ABAC)、暗号化ポリシー署名 |
| `ara::ucm` | ~55 % | 実装あり (一部) | UCM エラードメイン、更新セッション管理 (`Prepare`/`Stage`/`Verify`/`Activate`/`Rollback`/`Cancel`)、Transfer API (`TransferStart`/`TransferData`/`TransferExit`)、SHA-256 検証、状態/進捗コールバック、クラスタ別バージョン管理とダウングレード拒否、`CampaignManager`（複数パッケージ更新オーケストレーション、パッケージ別状態管理、自動状態再計算、キャンペーンロールバック）、`UpdateHistory`（更新履歴永続ログ、クラスタ別履歴照会、ファイル永続化） | installer daemon、secure boot 連携、差分更新 |
| 時刻同期 (`ara::tsync`) | ~65 % | 実装あり (一部) | `TimeSyncClient`（参照時刻更新、同期時刻変換、オフセット/状態照会、状態変更通知コールバック）、`SynchronizedTimeBaseProvider` 抽象インタフェース、`PtpTimeBaseProvider`（ptp4l/gPTP PHC 統合、`/dev/ptpN`）、`NtpTimeBaseProvider`（chrony/ntpd 自動検出統合）、`TimeSyncServer`（バックグラウンドプロバイダポーリング、マルチコンシューマ配信、可用性コールバック、プロバイダ喪失リセット）、エラードメイン、PTP/NTP プロバイダ常駐デーモン | レート補正、TSP SWS 全プロファイル対応 |
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
