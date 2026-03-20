# 50: Raspberry Pi ECU 配備

## 対象

17 のプラットフォームデーモンとユーザーアプリケーションを Raspberry Pi 上に配備し、
20 の systemd サービスで管理するプロトタイプ Adaptive AUTOSAR ECU を構築する。

## 目的

- AUTOSAR AP ランタイム一式を `/opt/autosar-ap` にインストールする
- 17 のプラットフォームデーモンを systemd サービスとして配備する
- ユーザーアプリ (本番 ECU アプリ、フルスタックデモ、カスタム) を常駐実行する
- CAN / SOME/IP / DDS / ZeroCopy をまとめて検証する

## 事前知識

デーモンとサービスの全体像は
[30_platform_service_architecture](30_platform_service_architecture.ja.md) を参照。

## 1) ビルドとインストール

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --install-middleware
```

ビルド・インストールされるもの:
- 静的ライブラリ 14 本 (`libara_*.a`)
- プラットフォームデーモンバイナリ 17 本
- ユーザーアプリテンプレート
- 配備構成ファイル一式

## 2) CAN インターフェースを準備

実機 CAN:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

検証用 vcan:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

CAN を使わない場合はこの手順をスキップ。

## 3) systemd サービスをインストール

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --enable
```

**20 の systemd サービス** がラッパースクリプト・環境変数ファイル・
bringup.sh・IAM ポリシーとともにインストールされる。

`/etc/default/` 内の主要設定ファイル:

| ファイル | 主な変数 |
|----------|----------|
| `autosar-ecu-full-stack` | `AUTOSAR_ECU_CAN_BACKEND`, `AUTOSAR_ECU_CAN_IF`, `AUTOSAR_ECU_DDS_DOMAIN_ID` |
| `autosar-diag-server` | `AUTOSAR_DIAG_LISTEN_PORT`, `AUTOSAR_DIAG_P2_SERVER_MS` |
| `autosar-phm-daemon` | `AUTOSAR_PHM_PERIOD_MS`, `AUTOSAR_PHM_DEGRADED_THRESHOLD` |
| `autosar-crypto-provider` | `AUTOSAR_CRYPTO_TOKEN`, `AUTOSAR_CRYPTO_KEY_ROTATION` |

## 4) サービス起動

### フルプロファイル (全 20 サービス)

```bash
# Tier 0: 通信基盤
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service

# Tier 1: プラットフォーム基盤
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-ntp-time-provider.service
sudo systemctl start autosar-ptp-time-provider.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-sm-state.service
sudo systemctl start autosar-can-manager.service
sudo systemctl start autosar-dlt.service
sudo systemctl start autosar-network-manager.service
sudo systemctl start autosar-crypto-provider.service

# Tier 2: プラットフォームアプリケーション
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-diag-server.service
sudo systemctl start autosar-phm-daemon.service
sudo systemctl start autosar-ucm.service

# Tier 3: 実行管理・監視
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-watchdog.service

# ユーザーアプリ (以下から選択)
sudo systemctl start autosar-ecu-full-stack.service
```

### 最小プロファイル

```bash
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-exec-manager.service
```

### 状態確認

```bash
# 全 AUTOSAR サービスを一覧
systemctl list-units 'autosar-*' --no-pager

# 個別サービスの確認
sudo systemctl status autosar-diag-server.service --no-pager

# 全デーモンステータスファイル
for f in /run/autosar/*.status; do
  echo "=== $(basename "$f") ==="
  cat "$f"
  echo
done
```

## 5) 検証

mock CAN で統合チェック:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

socketcan で統合チェック:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0 \
  --require-platform-binary
```

## 6) 何を確認しているか

| レイヤー | 確認内容 |
|----------|----------|
| SOME/IP | provider/consumer テンプレート + ルーティングマネージャ |
| DDS | pub/sub テンプレート間通信 |
| ZeroCopy | iceoryx RouDi + pub/sub テンプレート通信 |
| ECU | CAN 受信 → DDS 送信 |
| ECU | SOME/IP 受信 → DDS 送信 |
| 診断 | `autosar_diag_server` が DoIP ポートで待機しているか |
| PHM | `autosar_phm_daemon` のヘルス集約 |
| Crypto | `autosar_crypto_provider` のセルフテスト通過 |
| ステータス | `/run/autosar/*.status` が更新されているか |

## 7) サービス管理

### 不要サービスの無効化

```bash
sudo systemctl disable --now autosar-can-manager.service    # CAN 不使用時
sudo systemctl disable --now autosar-ntp-time-provider.service
sudo systemctl disable --now autosar-ptp-time-provider.service
sudo systemctl disable --now autosar-dlt.service
sudo systemctl disable --now autosar-ucm.service
```

### 設定変更

```bash
# 環境変数ファイルを編集
sudo nano /etc/default/autosar-diag-server

# 対象サービスを再起動
sudo systemctl restart autosar-diag-server.service
```

### ログ確認

```bash
# 全プラットフォームログ
journalctl -u 'autosar-*' --no-pager -n 50

# 特定サービスのログ (リアルタイム追跡)
journalctl -u autosar-phm-daemon.service --no-pager -f
```

## 補足

- 最小 ECU プロファイルとユーザーアプリ管理については
  [51_rpi_minimal_ecu_user_app_management](51_rpi_minimal_ecu_user_app_management.ja.md) を参照。
- プラットフォームサービスの詳細は
  [30_platform_service_architecture](30_platform_service_architecture.ja.md) を参照。
- 量産 ECU の安全・セキュリティ要件 (ASIL/SOTIF、Secure Boot、鍵管理、更新
  キャンペーン管理など) は別途システム統合が必要です。
