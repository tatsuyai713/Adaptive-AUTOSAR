# 02: 実行制御シグナル処理

## 対象ターゲット

- `autosar_user_exec_signal_template`
- ソース: `user_apps/src/apps/basic/exec_signal_app.cpp`

## 目的

- `ara::exec::SignalHandler` を使った安全停止の基本を理解する。
- `SIGINT`/`SIGTERM` で終了フラグを監視する構成を作る。

## 実行

```bash
./build-user-apps-opt/src/apps/basic/autosar_user_exec_signal_template
```

`Ctrl+C` で停止させ、終了処理が実行されることを確認します。

## 変更ポイント

1. メインループの周期処理へ監視・通信処理を追加する。
2. 停止時に必要なクリーンアップ（ファイル同期、通信停止）を追加する。
