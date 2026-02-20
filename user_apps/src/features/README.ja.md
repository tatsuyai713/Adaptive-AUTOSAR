# ユーザーアプリ フィーチャーモジュール

このディレクトリには、高度なアプリで共有される再利用可能な実装モジュールが含まれています。

## ディレクトリ構成

- `communication/vehicle_status/`
  - SOME/IP 指向のサービス型 / Proxy / Skeleton ラッパーを共有
- `communication/pubsub/`
  - トランスポートに依存しない ara::com Pub/Sub ヘルパー層
  - 共通ペイロードのシリアライズ / デシリアライズ
  - フィーチャーレベルの Pub/Sub 対応 DDS IDL
- `communication/can/`
  - SocketCAN 受信器
  - モック CAN 受信器
  - 車両ステータス CAN デコーダー
- `ecu/`
  - ECU ユーティリティヘルパー（引数解析、PHM/PER ラッパー、バックエンドファクトリー）

## 注記

- これらのモジュールはスタンドアロンの実行可能ファイルではありません。
- これらを使用するアプリエントリポイントは `user_apps/src/apps/feature/` にあります。
