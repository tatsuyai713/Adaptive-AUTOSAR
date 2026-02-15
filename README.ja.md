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
以下は次のリポジトリの導入フローをベースに本リポジトリへ取り込んだスクリプトです:
`https://github.com/tatsuyai713/lwrcl/tree/main/scripts`

```bash
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_middleware_stack.sh
```

互換エイリアスとして次も利用できます:
- `./scripts/install_dependemcy.sh`
- `./scripts/install_dependencies.sh`

1 コマンドでまとめて導入する場合:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

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
- `autosar-watchdog.service` (常駐ウォッチドッグ監視デーモン)

### Vector/ETAS/EB 向け資産を移植して使う
本実装は、ベンダ実装で開発した C++ 資産を「ソース互換で再ビルド」して、
他 UNIT と通信確認する用途に利用できます。

移植手順:
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`

## 実装機能マトリクス (AUTOSAR AP 全体仕様に対する位置付け)
ステータス定義:
- `実装あり (一部)`: リポジトリ内に実装あり。ただし AUTOSAR 仕様全体としては部分対応。
- `未実装`: 現在のリポジトリ対象外。

| AUTOSAR AP 領域 | 状態 | このリポジトリで提供されるもの | 未対応/備考 |
| --- | --- | --- | --- |
| `ara::core` | 実装あり (一部) | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, init/deinit, 初期化状態照会 API | 標準 API 全面対応ではない |
| `ara::log` | 実装あり (一部) | Logging framework, logger, sink (console/file), 実行時 LogLevel 上書き/照会 API | 商用実装比で設定/機能は限定的 |
| `ara::com` 共通 API | 実装あり (一部) | Event/Method/Field, Proxy/Skeleton base, Subscribe/Unsubscribe, handler, sample 抽象, 複数同時 `StartFindService`/handle 指定 `StopFindService`, Field 能力フラグ準拠動作 | 生成 API と SWS の全角ケースは未網羅 |
| `ara::com` SOME/IP | 実装あり (一部) | vSomeIP backend による SD, pub/sub, RPC | すべての SOME/IP/AP オプションは未対応 |
| `ara::com` DDS | 実装あり (一部) | Cyclone DDS wrapper (`ara::com::dds`) | QoS/運用プロファイルは部分対応 |
| `ara::com` ZeroCopy | 実装あり (一部) | iceoryx wrapper (`ara::com::zerocopy`) | バックエンド隠蔽実装。AUTOSAR 全標準化範囲を完全網羅するものではない |
| `ara::com` E2E | 実装あり (一部) | E2E Profile11 + event decorator | 他プロファイルは未実装 |
| `ara::exec` | 実装あり (一部) | Execution/State client-server helper, signal handler, worker thread, 実行状態変更コールバック API | EM 全機能を網羅するものではない |
| `ara::diag` | 実装あり (一部) | UDS/DoIP 系コンポーネント、routing/debouncing helper、Monitor の FDC しきい値到達アクション対応 | Diagnostic Manager 全仕様の網羅ではない |
| `ara::phm` | 実装あり (一部) | Health channel, supervision primitives | PHM 全体統合仕様は部分対応 |
| `ara::per` | 実装あり (一部) | Key-value/File storage API | 実運用向けポリシーは部分対応 |
| `ara::sm` | 実装あり (一部) | state/trigger utility | AUTOSAR SM FC 全機能ではない |
| ARXML ツール | 実装あり (一部) | YAML -> ARXML、ARXML -> ara::com binding 生成 | リポジトリ対象スコープ中心で全 ARXML 網羅ではない |
| `ara::crypto` | 実装あり (一部) | エラードメイン、SHA-256 ダイジェスト API、乱数バイト生成 API | 最小プリミティブのみ（鍵管理/PKI は未実装） |
| `ara::iam` | 実装あり (一部) | メモリ内 IAM ポリシー判定（subject/resource/action、ワイルドカード）、エラードメイン | ポリシー永続化やプラットフォーム IAM 連携は未実装 |
| `ara::ucm` | 実装あり (一部) | UCM エラードメイン、更新セッション管理 (`Prepare/Stage/Verify/Activate/Rollback/Cancel`)、SHA-256 検証、状態/進捗コールバック、クラスタ別バージョン管理とダウングレード拒否 | 簡易更新モデル（installer daemon/campaign 管理/secure boot 連携は未実装） |
| 時刻同期 (`ara::tsync`) | 実装あり (一部) | 参照時刻更新、同期時刻変換、オフセット/状態 API、エラードメイン | NTP/PTP デーモンとの統合は未実装 |
| Raspberry Pi ECU 配備プロファイル | 実装あり (一部) | ビルド/インストール統合スクリプト、SocketCAN セットアップ、systemd テンプレート、統合検証スクリプト、常駐デーモン (`vsomeip-routing`/`time-sync`/`persistency-guard`/`iam-policy`/`can-manager`/`watchdog`) | Linux 上のプロトタイプ ECU 運用を対象。量産向け安全/セキュリティ強化は別途システム統合が必要 |

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
  - `autosar_user_com_doip_diag_tester`
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
- `user_apps/tutorials/11_doip_diag_tester.ja.md` (Ubuntu 側 DoIP/DIAG テスター手順)

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
