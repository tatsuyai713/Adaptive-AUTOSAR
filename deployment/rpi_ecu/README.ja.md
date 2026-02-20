# Raspberry Pi ECU デプロイメントプロファイル

このプロファイルは、Raspberry Pi Linux マシンをこのリポジトリのランタイムと
ユーザーアプリを使ってプロトタイプ AUTOSAR スタイル ECU に変えるためのものです。

## このプロファイルでカバーする内容

- ランタイムのビルド・インストール（`/opt/autosar_ap` へ）
- インストール済みランタイムのみに対して `user_apps` をビルド
- Linux 上での SocketCAN 設定
- 以下の `systemd` ユニットのインストール:
  - iceoryx RouDi
  - vSomeIP ルーティングマネージャ
  - 時刻同期デーモン
  - 永続化ガードデーモン
  - IAM ポリシーローダーデーモン
  - CAN インターフェースマネージャデーモン
  - プラットフォームアプリスタック（`adaptive_autosar`, EM/SM/PHM/Diag/Vehicle）
  - 実行マネージャブリッジサービス（ユーザー bringup スクリプトを実行）
  - ユーザーアプリモニターデーモン（登録/ヘルス/監視）
  - ウォッチドッグスーパーバイザーデーモン
  - ECU フルスタックアプリ（`CAN + SOME/IP` 受信、`DDS` 送信）
- 単一スクリプトでトランスポートと ECU パスを検証

## 前提条件

- Raspberry Pi 上の Linux（64ビット推奨）
- C++14 ツールチェーン、CMake、Python3 + PyYAML
- 以下にインストールされたミドルウェア:
  - `/opt/vsomeip`
  - `/opt/iceoryx`
  - `/opt/cyclonedds`
- `systemd` が利用可能

前提条件とミドルウェアをインストール:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

## 1) ランタイムとユーザーアプリをビルド・インストール

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware
```

## 2) CAN インターフェースを設定

物理 CAN:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

仮想 CAN（立ち上げ確認用のみ）:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

## 3) systemd サービスをインストール

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

以下のファイルでランタイムオプションを編集:

- `/etc/default/autosar-ecu-full-stack`
- `/etc/default/autosar-vsomeip-routing`
- `/etc/default/autosar-time-sync`
- `/etc/default/autosar-persistency-guard`
- `/etc/default/autosar-iam-policy`
- `/etc/default/autosar-can-manager`
- `/etc/default/autosar-platform-app`
- `/etc/default/autosar-exec-manager`
- `/etc/default/autosar-watchdog`
- `/etc/default/autosar-user-app-monitor`
- `/etc/autosar/iam_policy.csv`

ユーザー起動スクリプトを編集:

- `/etc/autosar/bringup.sh`
- `/etc/autosar/startup.sh`（エイリアス）

## 4) サービスを起動

```bash
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-can-manager.service
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-watchdog.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-ecu-full-stack.service
sudo systemctl status autosar-vsomeip-routing.service --no-pager
sudo systemctl status autosar-platform-app.service --no-pager
sudo systemctl status autosar-exec-manager.service --no-pager
sudo systemctl status autosar-watchdog.service --no-pager
sudo systemctl status autosar-user-app-monitor.service --no-pager
```

`bringup.sh` のみで独自アプリを起動する場合は、固定サンプルサービスを無効化:

```bash
sudo systemctl disable --now autosar-ecu-full-stack.service
```

## ユーザー bringup ワークフロー

1. `${AUTOSAR_USER_APPS_BUILD_DIR}` へアプリをビルド（デフォルト: `/opt/autosar_ap/user_apps_build`）。
2. `/etc/autosar/bringup.sh` を編集。
3. ヘルパー関数（`launch_app` / `launch_app_with_heartbeat` / `launch_app_managed`）で起動コマンドを追加。
4. 実行マネージャサービスを再起動:

```bash
sudo systemctl restart autosar-exec-manager.service
```

`autosar-vsomeip-routing.service` が SOME/IP ルーティングを常駐させます。
`autosar-time-sync.service`、`autosar-persistency-guard.service`、
`autosar-iam-policy.service`、`autosar-can-manager.service` が
コア常駐プラットフォームサポートを初期化します。
その後、`autosar-platform-app.service` が組み込みプラットフォームプロセススタックを起動します。
次に `autosar-exec-manager.service` が bringup スクリプトを実行します。
`autosar-user-app-monitor.service` は登録済みアプリの PID、オプションのハートビート鮮度、
`ara::phm::HealthChannel` 状態ファイルを継続的に監視し、障害検知時に再起動リカバリをトリガーします。
`autosar-watchdog.service` がランタイムハートビートを監視します。

`autosar-user-app-monitor` の対応設定:
- 起動グレース期間（`AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS`）
- 再起動バックオフ（`AUTOSAR_USER_APP_MONITOR_RESTART_BACKOFF_MS`）
- 非アクティブ停止を許可（`AUTOSAR_USER_APP_MONITOR_ALLOW_DEACTIVATED_AS_HEALTHY`）

登録/監視で使用されるランタイムファイル:
- `/run/autosar/user_apps_registry.csv`（`bringup.sh` が生成）
- `/run/autosar/user_app_monitor.status`（ユーザーアプリモニターデーモンが生成）
- `/run/autosar/phm/health/*.status`（`ara::phm::HealthChannel` が生成）

`bringup.sh` ヘルパー API:
- `launch_app "<名前>" <コマンド> [引数...]`
  : PID、デフォルトインスタンス指定子、デフォルト再起動ポリシーを登録。
- `launch_app_with_heartbeat "<名前>" "<heartbeat_file>" "<timeout_ms>" <コマンド> [引数...]`
  : 生存確認と再起動ポリシーにハートビート鮮度チェックを追加。
- `launch_app_managed "<名前>" "<instance_specifier>" "<heartbeat_file>" "<timeout_ms>" "<restart_limit>" "<restart_window_ms>" <コマンド> [引数...]`
  : 明示的な PHM インスタンスと再起動ポリシーを持つ完全な AUTOSAR スタイル登録。

## 5) 準備状況と通信パスを検証

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

実際の CAN 入力を使う場合:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0 \
  --require-platform-binary
```

## 運用上の注意

- このプロファイルは Linux ベースのプロトタイプ ECU 運用を対象としています。
- 認定済み商用 AUTOSAR スタックではありません。
- 安全・セキュリティの堅牢化（ASIL/SOTIF、セキュアブートチェーン、完全な更新キャンペーン、
  本番診断、ウォッチドッグ統合ポリシー）はプロダクトワークとして残ります。
