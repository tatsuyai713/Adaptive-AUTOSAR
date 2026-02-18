# Adaptive-AUTOSAR ドキュメントハブ

このファイルは Adaptive-AUTOSAR プロジェクトのドキュメント一覧です。
プロジェクト全体の概要については [ルート README](../README.ja.md) を参照してください。

Language: [English](README.md)

---

## クイックナビゲーション

| やりたいこと | 参照先 |
| --- | --- |
| 初めてビルド・実行する | [クイックスタート](../README.ja.md#quickstart-ビルド---インストール---ユーザーアプリビルド実行) |
| Raspberry Pi に ECU として配備する | [RPi ECU ガイド](../deployment/rpi_ecu/README.md) |
| QNX 8.0 向けクロスコンパイルする | [QNX ガイド](../qnx/README.md) |
| 自分の AUTOSAR アプリを作る | [User Apps](../user_apps/README.md) |
| チュートリアルで順番に学ぶ | [チュートリアル一覧](../user_apps/tutorials/README.ja.md) |
| ARXML / C++ バインディングを生成する | [ARXML ジェネレータ](../tools/arxml_generator/README.md) |
| ホスト PC から診断テストを行う | [DoIP テスタ](../tools/host_tools/doip_diag_tester/README.ja.md) |
| API リファレンスを参照する | [API ドキュメント (オンライン)](https://tatsuyai713.github.io/Adaptive-AUTOSAR/) |

---

## ドキュメント一覧

| ファイル | 言語 | 内容 |
| --- | :---: | --- |
| [README.md](../README.md) | EN | プロジェクト概要・クイックスタート・機能マトリクス |
| [README.ja.md](../README.ja.md) | JA | プロジェクト概要（日本語） |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | EN | コントリビューションガイド |
| [CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md) | EN | コミュニティ行動規範 |
| **docs/** | | |
| [docs/README.md](README.md) | EN | ドキュメントハブ（英語） |
| [docs/README.ja.md](README.ja.md) | JA | ドキュメントハブ（このファイル） |
| **user_apps/** | | |
| [user_apps/README.md](../user_apps/README.md) | EN | ユーザーアプリのビルドシステムとターゲット一覧 |
| [user_apps/tutorials/README.md](../user_apps/tutorials/README.md) | EN | チュートリアル一覧（英語） |
| [user_apps/tutorials/README.ja.md](../user_apps/tutorials/README.ja.md) | JA | チュートリアル一覧（日本語） |
| **deployment/** | | |
| [deployment/rpi_ecu/README.md](../deployment/rpi_ecu/README.md) | EN | Raspberry Pi ECU 配備ランブック |
| [deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md](../deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md) | JA | RPi gPTP/PTP 詳細セットアップ（日本語） |
| **qnx/** | | |
| [qnx/README.md](../qnx/README.md) | JA | QNX 8.0 クロスコンパイルガイド |
| **tools/** | | |
| [tools/arxml_generator/README.md](../tools/arxml_generator/README.md) | EN | ARXML ジェネレータガイド |
| [tools/arxml_generator/YAML_MANUAL.ja.md](../tools/arxml_generator/YAML_MANUAL.ja.md) | JA | YAML 仕様書（日本語） |
| [tools/host_tools/doip_diag_tester/README.md](../tools/host_tools/doip_diag_tester/README.md) | EN | ホスト側 DoIP/UDS 診断テスタ |
| [tools/host_tools/doip_diag_tester/README.ja.md](../tools/host_tools/doip_diag_tester/README.ja.md) | JA | DoIP テスタガイド（日本語） |

---

## チュートリアル（ステップバイステップ）

すべてのチュートリアルは [`user_apps/tutorials/`](../user_apps/tutorials/) 以下にあります。
完全な一覧は [tutorials/README.ja.md](../user_apps/tutorials/README.ja.md) を参照してください。

| # | テーマ | ファイル |
| --- | --- | --- |
| 01 | ランタイムの初期化・終了ライフサイクル | [01_runtime_lifecycle.ja.md](../user_apps/tutorials/01_runtime_lifecycle.ja.md) |
| 02 | 実行管理とシグナルハンドリング | [02_exec_signal.ja.md](../user_apps/tutorials/02_exec_signal.ja.md) |
| 03 | 永続化ストレージとヘルスモニタリング | [03_per_phm.ja.md](../user_apps/tutorials/03_per_phm.ja.md) |
| 04 | SOME/IP Pub/Sub 通信 | [04_someip_pubsub.ja.md](../user_apps/tutorials/04_someip_pubsub.ja.md) |
| 05 | ゼロコピー IPC（iceoryx） | [05_zerocopy_pubsub.ja.md](../user_apps/tutorials/05_zerocopy_pubsub.ja.md) |
| 06 | DDS Pub/Sub（CycloneDDS） | [06_dds_pubsub.ja.md](../user_apps/tutorials/06_dds_pubsub.ja.md) |
| 07 | SocketCAN メッセージ受信・デコード | [07_socketcan_decode.ja.md](../user_apps/tutorials/07_socketcan_decode.ja.md) |
| 08 | フル ECU スタック実装 | [08_ecu_full_stack.ja.md](../user_apps/tutorials/08_ecu_full_stack.ja.md) |
| 09 | Raspberry Pi ECU 配備 | [09_rpi_ecu_deployment.ja.md](../user_apps/tutorials/09_rpi_ecu_deployment.ja.md) |
| 10 | Vector/ETAS/EB 向け資産の移植 | [10_vendor_autosar_asset_porting.ja.md](../user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md) |

---

## プロジェクト構成

```text
Adaptive-AUTOSAR/
├── src/                    # AUTOSAR AP コア実装
│   ├── ara/                # AUTOSAR Adaptive Platform API 群
│   │   ├── core/           # 基盤型（Result, Optional, Future, ...）
│   │   ├── com/            # 通信（SOME/IP, DDS, ZeroCopy, E2E, SecOC）
│   │   ├── log/            # ロギング（console / file / network / DLT）
│   │   ├── exec/           # 実行管理
│   │   ├── diag/           # 診断（UDS サービス, DoIP）
│   │   ├── phm/            # プラットフォームヘルスマネジメント
│   │   ├── per/            # 永続化ストレージ（KVS / ファイル）
│   │   ├── sm/             # 状態管理
│   │   ├── tsync/          # 時刻同期（NTP / PTP）
│   │   ├── ucm/            # 更新・設定管理
│   │   ├── iam/            # ID・アクセス管理
│   │   ├── crypto/         # 暗号（SHA, HMAC, AES, RSA, ECDSA）
│   │   ├── nm/             # ネットワーク管理
│   │   └── secoc/          # セキュア車載通信
│   ├── application/        # プラットフォーム側デーモン・ヘルパー
│   └── arxml/              # ARXML スキーマ解析ライブラリ
│
├── test/                   # 単体テスト（src/ と対応した構成）
│
├── user_apps/              # ユーザーアプリのテンプレートとチュートリアル
│   ├── src/apps/
│   │   ├── basic/          # 最小ランタイム・シグナル・PER/PHM デモ
│   │   ├── communication/  # SOME/IP, DDS, ZeroCopy テンプレート
│   │   └── feature/        # ライフサイクル・SocketCAN・フル ECU スタック
│   ├── src/features/       # 上位アプリが使う共有フィーチャーモジュール
│   ├── idl/                # DDS IDL 定義（CycloneDDS）
│   └── tutorials/          # チュートリアル 01〜10
│
├── scripts/                # Linux 向けビルド・インストール・配備スクリプト
│   ├── install_dependency.sh
│   ├── install_middleware_stack.sh   # vSomeIP + iceoryx + CycloneDDS
│   ├── build_and_install_autosar_ap.sh
│   ├── build_user_apps_from_opt.sh
│   ├── build_and_install_rpi_ecu_profile.sh
│   ├── install_rpi_ecu_services.sh
│   ├── setup_socketcan_interface.sh
│   ├── verify_rpi_ecu_profile.sh
│   └── generate_doxygen_docs.sh
│
├── qnx/                    # QNX SDP 8.0 クロスコンパイル対応
│   ├── cmake/              # QNX ツールチェーンファイル
│   ├── env/                # QNX 環境変数テンプレート
│   ├── patches/            # ソース互換性パッチ（Python）
│   └── scripts/            # QNX クロスビルドスクリプト群
│       ├── build_all_qnx.sh          # 一括ビルドエントリーポイント
│       ├── build_libraries_qnx.sh    # ミドルウェアクロスビルド
│       ├── build_autosar_ap_qnx.sh   # AUTOSAR AP クロスビルド
│       ├── build_user_apps_qnx.sh    # ユーザーアプリクロスビルド
│       ├── create_qnx_deploy_image.sh
│       └── deploy_to_qnx_target.sh
│
├── tools/                  # 開発・診断ツール
│   ├── arxml_generator/    # YAML -> ARXML -> C++ バインディング生成
│   ├── host_tools/
│   │   └── doip_diag_tester/   # ホスト側 DoIP/UDS 診断テスタ
│   └── sample_runner/      # 通信トランスポート検証スクリプト群
│
├── deployment/             # 配備プロファイルとランタイム設定
│   └── rpi_ecu/            # Raspberry Pi プロトタイプ ECU プロファイル
│       ├── systemd/        # systemd サービスユニット（17 ファイル）
│       ├── bin/            # サービス別ラッパーシェルスクリプト
│       ├── env/            # サービス別環境変数テンプレート
│       └── bringup/        # ユーザーアプリ起動スクリプトテンプレート
│
├── configuration/          # ARXML マニフェストと vSomeIP JSON 設定
├── cmake/                  # CMake パッケージ設定テンプレート
├── docs/                   # ドキュメントハブ + Doxygen 出力
│   ├── README.md           # ドキュメントハブ（英語）
│   ├── README.ja.md        # ドキュメントハブ（このファイル）
│   ├── doxygen.conf        # Doxygen 設定
│   ├── api/                # 生成された API ドキュメント（doxygen 実行後）
│   └── simulation_flow_diagram.png
│
├── .github/workflows/      # CI/CD（GitHub Actions）
├── CMakeLists.txt          # メインビルド設定
├── README.md               # プロジェクト概要（英語）
└── README.ja.md            # プロジェクト概要（日本語）
```

---

## スクリプト一覧

### Linux ビルド・配備スクリプト（`scripts/`）

| スクリプト | 用途 |
| --- | --- |
| `install_dependency.sh` | システムレベルのビルド依存関係をインストール |
| `install_middleware_stack.sh` | vSomeIP, iceoryx, CycloneDDS をビルド・インストール |
| `install_vsomeip.sh` | vSomeIP のみインストール |
| `install_iceoryx.sh` | iceoryx のみインストール |
| `install_cyclonedds.sh` | CycloneDDS のみインストール |
| `build_and_install_autosar_ap.sh` | AUTOSAR AP ランタイムをビルドしてインストール |
| `build_user_apps_from_opt.sh` | インストール済みランタイムに対して user_apps をビルド |
| `build_and_install_rpi_ecu_profile.sh` | Raspberry Pi ECU プロファイルを一括ビルド・インストール |
| `install_rpi_ecu_services.sh` | systemd サービスをインストール・有効化 |
| `setup_socketcan_interface.sh` | SocketCAN インタフェースを設定 |
| `verify_rpi_ecu_profile.sh` | ECU プロファイルの動作を検証 |
| `generate_doxygen_docs.sh` | Doxygen API ドキュメントを生成 |

### QNX クロスビルドスクリプト（`qnx/scripts/`）

| スクリプト | 用途 |
| --- | --- |
| `build_all_qnx.sh` | 一括ビルド（ミドルウェア + ランタイム + ユーザーアプリ） |
| `build_libraries_qnx.sh` | ミドルウェアライブラリを全部クロスビルド |
| `build_boost_qnx.sh` | Boost をクロスビルド（スタティック） |
| `build_vsomeip_qnx.sh` | vSomeIP をクロスビルド |
| `build_cyclonedds_qnx.sh` | CycloneDDS をクロスビルド |
| `build_iceoryx_qnx.sh` | iceoryx をクロスビルド |
| `build_autosar_ap_qnx.sh` | AUTOSAR AP ランタイムをクロスビルド |
| `build_user_apps_qnx.sh` | ユーザーアプリをクロスビルド |
| `build_host_tools.sh` | ホスト側ツールをビルド |
| `create_qnx_deploy_image.sh` | 配備イメージを作成（tar.gz / IFS） |
| `deploy_to_qnx_target.sh` | QNX ターゲットへ SSH 経由でデプロイ |
| `install_dependency.sh` | QNX ホストツールの前提条件を確認 |

### 検証スクリプト（`tools/sample_runner/`）

| スクリプト | 検証内容 |
| --- | --- |
| `verify_ara_com_someip_transport.sh` | SOME/IP Pub/Sub 通信 |
| `verify_ara_com_dds_transport.sh` | DDS Pub/Sub 通信 |
| `verify_ara_com_zerocopy_transport.sh` | ゼロコピー IPC 通信 |
| `verify_ara_com_async_bsd_transport.sh` | 非同期 BSD ソケットトランスポート |
| `verify_ecu_can_to_dds.sh` | CAN から DDS へのゲートウェイ |
| `verify_ecu_reference_gateway.sh` | リファレンスゲートウェイ全体 |
| `verify_ecu_someip_dds.sh` | SOME/IP と DDS の相互接続 |

---

## アーキテクチャ図

![シミュレーションフロー図](simulation_flow_diagram.png)

ソース: `docs/simulation_flow_diagram.drawio`（[draw.io](https://draw.io) で編集可能）

---

## API リファレンス（Doxygen）

- **オンライン**: <https://tatsuyai713.github.io/Adaptive-AUTOSAR/>
- **ローカル生成**:

  ```bash
  ./scripts/generate_doxygen_docs.sh
  ```

- **出力先**: `docs/api/index.html`（生成後）
- **設定ファイル**: `docs/doxygen.conf`
