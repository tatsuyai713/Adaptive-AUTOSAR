# 11. Ubuntu 側 DoIP/DIAG テスターで Raspberry Pi ECU を検証

このチュートリアルでは、Ubuntu マシンから Raspberry Pi ECU へ DoIP/DIAG 通信を行い、
データ取得・送信試験・受信試験を実施する手順を示します。

対象実行ファイル:
- `autosar_user_com_doip_diag_tester`

## 1) ビルド

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

実行ファイル例:
- `build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester`

## 2) 基本パラメータ

- `--host`: Raspberry Pi ECU の IP
- `--tcp-port`: DoIP TCP ポート（このリポジトリ既定は `8081`）
- `--udp-port`: Vehicle-ID 用 UDP ポート（同じく `8081`）
- `--vehicle-id-transport`: `tcp` または `udp`（既定 `tcp`）
- `--tester-address`: テスター論理アドレス（既定 `0x0E80`）
- `--target-address`: ECU 論理アドレス（既定 `0x0001`）
- `--did`: `diag-read-did` やテストで読む DID
- `--uds`: カスタム UDS（例: `22F50D`）
- `--fixed-packet-size`: この実装既定は `64`（標準可変長にしたい場合は `0`）
- `--min-rx`: RX 成功とみなす最小受信数（既定 `0`、厳密評価時は明示指定）

この実装で応答可能な代表 DID:
- `0xF50D` 平均車速
- `0xF52F` 燃料量
- `0xF546` 外気温
- `0xF55E` 平均燃費
- `0xF505` 冷却水温
- `0xF5A6` オドメータ

## 3) Vehicle-ID 応答確認

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=vehicle-id \
  --host=192.168.10.20 \
  --vehicle-id-transport=tcp \
  --tcp-port=8081
```

必要に応じて UDP broadcast 送信:

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=vehicle-id \
  --host=255.255.255.255 \
  --vehicle-id-transport=udp \
  --udp-port=8081 \
  --udp-broadcast
```

## 4) Routing Activation 確認

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=routing-activation \
  --host=192.168.10.20 \
  --tcp-port=8081
```

## 5) DIAG でデータ取得 (ReadDataByIdentifier)

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=diag-read-did \
  --host=192.168.10.20 \
  --did=0xF50D
```

## 6) カスタム UDS 送信

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=diag-custom \
  --host=192.168.10.20 \
  --uds=22F52F
```

## 7) 送信テスト (TX)

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=tx-test \
  --host=192.168.10.20 \
  --did=0xF505 \
  --count=100 \
  --interval-ms=20
```

## 8) 受信テスト (RX)

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=rx-test \
  --host=192.168.10.20 \
  --did=0xF5A6 \
  --count=100 \
  --min-rx=90 \
  --interval-ms=20
```

## 9) 一括テスト

```bash
./build-user-apps-opt/src/apps/communication/diag/autosar_user_com_doip_diag_tester \
  --mode=full-test \
  --host=192.168.10.20 \
  --did=0xF50D \
  --count=50
```

`full-test` は以下を順に実行します。
- Vehicle-ID（`--request-vehicle-id-in-full=true` の場合）
- Routing Activation
- 単発 DIAG 読み出し
- TX テスト
- RX テスト

注記:
- 既定では `--request-vehicle-id-in-full=false` です。
- このリポジトリの DoIP サーバ実装は Vehicle-ID を TCP で扱うため、
  Vehicle-ID を `full-test` に含める場合は `--request-vehicle-id-in-full=true` を指定してください。
