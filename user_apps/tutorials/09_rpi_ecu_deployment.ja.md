# 09: Raspberry Pi ECU 配備

## 対象

- Raspberry Pi Linux マシンをプロトタイプ ECU として起動する
- 対象アプリ:
  - `autosar_user_tpl_ecu_full_stack`
  - `autosar_user_tpl_ecu_someip_source` (SOME/IP 入力の動作確認用)

## 目的

- `/opt/autosar_ap` へランタイムと user_apps をインストールする
- `systemd` で ECU アプリを常駐実行する
- CAN/SOME-IP/DDS/ZeroCopy をまとめて検証する

## 1) ビルドとインストール

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build
```

## 2) CAN インターフェースを準備

実機 CAN:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

検証用 vcan:

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

設定ファイル:

- `/etc/default/autosar-ecu-full-stack`

必要に応じて以下を編集:

- `AUTOSAR_ECU_CAN_BACKEND` (`socketcan` or `mock`)
- `AUTOSAR_ECU_CAN_IF` (例: `can0`)
- `AUTOSAR_ECU_ENABLE_SOMEIP`
- `AUTOSAR_ECU_DDS_DOMAIN_ID`
- `AUTOSAR_ECU_DDS_TOPIC`

## 4) サービス起動

```bash
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-ecu-full-stack.service
sudo systemctl status autosar-ecu-full-stack.service --no-pager
```

## 5) 検証

mock CAN で統合チェック:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock
```

socketcan で統合チェック:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0
```

## 6) 何を確認しているか

- SOME/IP: provider/consumer テンプレート間通信 + ルーティングマネージャ起動確認
- DDS: pub/sub テンプレート間通信
- ZeroCopy: iceoryx RouDi + pub/sub テンプレート通信
- ECU:
  - CAN 受信 (mock/socketcan) -> DDS 送信
  - SOME/IP 受信 -> DDS 送信

## 補足

- 量産 ECU の安全・セキュリティ要件 (ASIL/SOTIF、Secure Boot、鍵管理、更新キャンペーン管理など) は別途システム統合が必要です。
