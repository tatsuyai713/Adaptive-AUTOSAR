# 31: 診断サーバー (UDS/DoIP)

## 対象

`autosar_diag_server` — ソース: `src/main_diag_server.cpp`

## 目的

診断サーバーは専用の **UDS over DoIP** エンドポイントを提供し、
設定可能な TCP ポートでリッスンして `ara::diag::DiagnosticManager` を通じて
診断リクエストを処理します。

`autosar-diag-server.service` としてスタンドアロンの systemd サービスで配備され、
ユーザーアプリの再起動中も診断インターフェースが利用可能です。

## サポートする UDS サービス

| SID | サービス | 説明 |
|-----|---------|------|
| 0x10 | DiagnosticSessionControl | セッション管理 |
| 0x11 | ECUReset | ハード/ソフトリセット |
| 0x14 | ClearDTC | DTC クリア |
| 0x19 | ReadDTC | DTC 情報読出し |
| 0x22 | ReadDataByID | データ ID 読出し |
| 0x27 | SecurityAccess | シード/キー認証 |
| 0x2E | WriteDataByID | データ ID 書込み |
| 0x2F | IOControlByID | 入出力制御 |
| 0x31 | RoutineControl | ルーチン開始/停止/問合せ |
| 0x34 | RequestDownload | ダウンロードセッション開始 |
| 0x35 | RequestUpload | アップロードセッション開始 |
| 0x36 | TransferData | ブロック転送 |
| 0x37 | RequestTransferExit | 転送完了 |
| 0x3E | TesterPresent | キープアライブ |
| 0x85 | ControlDTCSetting | DTC 記録の有効/無効 |

## アーキテクチャ

```
  DoIP テスター (Ubuntu)            Raspberry Pi ECU
  ┌──────────────┐           ┌──────────────────────┐
  │ doip_diag_   │  TCP:13400│  autosar_diag_server │
  │ tester.py    │ ─────────►│  ┌────────────────┐  │
  │              │           │  │DiagnosticManager│  │
  │              │◄───────── │  │ (15 UDS SIDs)  │  │
  └──────────────┘           │  └────────────────┘  │
                             │  ステータス →         │
                             │  /run/autosar/        │
                             │  diag_server.status   │
                             └──────────────────────┘
```

## 環境変数

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_DIAG_LISTEN_ADDR` | `0.0.0.0` | バインドアドレス |
| `AUTOSAR_DIAG_LISTEN_PORT` | `13400` | DoIP TCP ポート |
| `AUTOSAR_DIAG_P2_SERVER_MS` | `50` | P2 応答タイミング (ms) |
| `AUTOSAR_DIAG_P2STAR_SERVER_MS` | `5000` | P2* 拡張タイミング (ms) |
| `AUTOSAR_DIAG_STATUS_PERIOD_MS` | `2000` | ステータス書込み間隔 (ms) |
| `AUTOSAR_DIAG_STATUS_FILE` | `/run/autosar/diag_server.status` | ステータス出力パス |

## 実行（スタンドアロン）

```bash
# ランタイムをビルド（未ビルドの場合）
cd /home/user/Adaptive-AUTOSAR
cmake -B build-local-check -DCMAKE_INSTALL_PREFIX=/opt/autosar-ap .
cmake --build build-local-check -j$(nproc)

# 直接実行
AUTOSAR_DIAG_LISTEN_PORT=13400 \
  ./build-local-check/bin/autosar_diag_server
```

## ステータスファイル

デーモンは `/run/autosar/diag_server.status` にステータスを書き出します:

```
listening=1
total_requests=42
total_connections=3
active_connections=1
positive_responses=38
negative_responses=4
updated_epoch_ms=1717171717000
```

リアルタイム監視:

```bash
watch -n1 cat /run/autosar/diag_server.status
```

## systemd サービス

```bash
# 有効化して起動
sudo systemctl enable --now autosar-diag-server.service

# ステータス確認
sudo systemctl status autosar-diag-server.service --no-pager

# 設定変更
sudo nano /etc/default/autosar-diag-server
sudo systemctl restart autosar-diag-server.service
```

サービス起動順序:
- **After**: `network-online.target`, `autosar-platform-app.service`
- **Before**: `autosar-exec-manager.service`
- **Restart**: `on-failure` (2 秒遅延)

## DoIP テスターでの動作確認

Ubuntu ホスト側の DoIP テスターツールを使用:

```bash
# Ubuntu ホスト上で
cd tools/host_tools/doip_diag_tester
python3 doip_diag_tester.py --target <rpi-ip> --port 13400 --sid 0x22 --did 0xF190
```

詳細は `tools/host_tools/doip_diag_tester/README.ja.md` を参照。

## 次のステップ

| チュートリアル | トピック |
|---------------|---------|
| [30_platform_service_architecture](30_platform_service_architecture.ja.md) | プラットフォーム全体像 |
| [32_phm_daemon](32_phm_daemon.ja.md) | ヘルス管理デーモン |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.ja.md) | RPi への配備 |
