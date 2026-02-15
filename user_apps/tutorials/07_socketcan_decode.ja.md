# 07: SocketCAN 受信とデコード

## 対象ターゲット

- `autosar_user_tpl_can_socketcan_receiver`
- ソース: `user_apps/src/apps/feature/can/socketcan_receive_decode_app.cpp`

## 目的

- Linux SocketCAN フレームを受信し、車両ステータスへデコードする。
- `socketcan` と `mock` の両バックエンド切替を理解する。

## 実行

Mock バックエンド:

```bash
./build-user-apps-opt/autosar_user_tpl_can_socketcan_receiver \
  --can-backend=mock \
  --recv-timeout-ms=20
```

SocketCAN バックエンド:

```bash
./build-user-apps-opt/autosar_user_tpl_can_socketcan_receiver \
  --can-backend=socketcan \
  --ifname=can0 \
  --powertrain-can-id=0x100 \
  --chassis-can-id=0x101
```

## 変更ポイント

1. CAN ID と信号マッピングを ECU 仕様に合わせて更新する。
2. `RequireBothFramesBeforeDecode` を要件に応じて調整する。
3. デコード結果を DDS/SOMEIP 送信処理に接続する。
