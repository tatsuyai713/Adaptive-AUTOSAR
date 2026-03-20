# 33: 暗号プロバイダ

## 対象

`autosar_crypto_provider` — ソース: `src/main_crypto_provider.cpp`

## 目的

暗号プロバイダデーモンは、**AUTOSAR Cryptography の鍵管理と暗号処理** を
プラットフォームサービスとして提供します。
HSM（またはソフトウェアエミュレーション）暗号プロバイダを初期化し、
鍵スロットを管理し、鍵ローテーションに対応し、定期的なセルフテストを実行します。

`autosar-crypto-provider.service` として配備され、他のプラットフォームデーモンより
先に起動します（Tier 1 — `local-fs.target` の後）。
これにより暗号サービスがブート初期段階から利用可能になります。

## 機能

- **HSM 初期化** — `ara::crypto::HsmProvider` 経由
- **鍵スロット管理** — 割当、占有、ロック、照会
- **鍵ローテーション** — 設定可能な間隔での自動ローテーション
- **セルフテスト** — AES-128 と HMAC-SHA256 を使った定期的な暗号/復号/検証サイクル
- **ステータス報告** — `/run/autosar/crypto_provider.status` への書出し

## アーキテクチャ

```
  ┌─────────────────────────────────────────────┐
  │           autosar_crypto_provider            │
  │                                             │
  │  HsmProvider                                │
  │  ├─ Token: "AutosarEcuHsm"                 │
  │  ├─ Key Slots: [occupied/empty/locked]      │
  │  ├─ Self-Test: AES-128 + HMAC-SHA256        │
  │  └─ Key Rotation: (有効時)                   │
  │                                             │
  │  KeyStorageProvider                         │
  │  └─ /var/lib/autosar/crypto/keys            │
  │                                             │
  │  → /run/autosar/crypto_provider.status      │
  └─────────────────────────────────────────────┘
```

## 環境変数

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_CRYPTO_TOKEN` | `AutosarEcuHsm` | HSM トークン識別子 |
| `AUTOSAR_CRYPTO_PERIOD_MS` | `5000` | メインループサイクル (ms) |
| `AUTOSAR_CRYPTO_KEY_ROTATION` | `false` | 鍵ローテーション有効/無効 |
| `AUTOSAR_CRYPTO_ROTATION_INTERVAL_MS` | `3600000` | ローテーション間隔 (1 時間) |
| `AUTOSAR_CRYPTO_SELF_TEST` | `true` | セルフテスト有効/無効 |
| `AUTOSAR_CRYPTO_SELF_TEST_INTERVAL_MS` | `60000` | セルフテスト間隔 (1 分) |
| `AUTOSAR_CRYPTO_STATUS_FILE` | `/run/autosar/crypto_provider.status` | ステータス出力 |
| `AUTOSAR_CRYPTO_KEY_STORAGE_PATH` | `/var/lib/autosar/crypto/keys` | 鍵ストレージディレクトリ |

## 実行（スタンドアロン）

```bash
# デフォルトで実行
./build-local-check/bin/autosar_crypto_provider

# テスト用に鍵ローテーションを有効化
AUTOSAR_CRYPTO_KEY_ROTATION=true \
AUTOSAR_CRYPTO_ROTATION_INTERVAL_MS=30000 \
AUTOSAR_CRYPTO_SELF_TEST_INTERVAL_MS=10000 \
  ./build-local-check/bin/autosar_crypto_provider
```

## ステータスファイル

デーモンは `/run/autosar/crypto_provider.status` に書き出します:

```
initialized=1
total_slots=16
occupied_slots=4
self_tests_passed=12
self_tests_failed=0
key_rotations=0
updated_epoch_ms=1717171717000
```

リアルタイム監視:

```bash
watch -n5 cat /run/autosar/crypto_provider.status
```

## セルフテストサイクル

各セルフテスト反復:

1. 一時的な AES-128 鍵を生成
2. テスト平文ブロックを暗号化
3. 復号して比較
4. 平文に対して HMAC-SHA256 を生成
5. HMAC を検証

いずれかのステップが失敗すると `self_tests_failed` が加算され、イベントがログ出力されます。

## systemd サービス

```bash
sudo systemctl enable --now autosar-crypto-provider.service
sudo systemctl status autosar-crypto-provider.service --no-pager
```

サービス起動順序:
- **After**: `local-fs.target`
- **Before**: `autosar-platform-app.service`
- **Restart**: `on-failure` (3 秒遅延)

暗号プロバイダはプラットフォームアプリケーションバイナリより **先に** 起動し、
暗号サービスをブート時から利用可能にします。

## 鍵ストレージ

鍵は `AUTOSAR_CRYPTO_KEY_STORAGE_PATH`
（デフォルト: `/var/lib/autosar/crypto/keys`）に永続化されます。

```bash
# 鍵ストレージを確認
ls -la /var/lib/autosar/crypto/keys/

# 適切な権限でディレクトリを作成
sudo mkdir -p /var/lib/autosar/crypto/keys
sudo chmod 700 /var/lib/autosar/crypto/keys
```

## 次のステップ

| チュートリアル | トピック |
|---------------|---------|
| [30_platform_service_architecture](30_platform_service_architecture.ja.md) | プラットフォーム全体像 |
| [31_diag_server](31_diag_server.ja.md) | 診断サーバー |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.ja.md) | RPi への配備 |
