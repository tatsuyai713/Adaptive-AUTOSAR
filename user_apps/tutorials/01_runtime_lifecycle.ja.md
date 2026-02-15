# 01: ランタイム初期化と終了

## 対象ターゲット

- `autosar_user_tpl_runtime_lifecycle`
- ソース: `user_apps/src/apps/feature/runtime/runtime_lifecycle_app.cpp`

## 目的

- `ara::core::Initialize()` と `ara::core::Deinitialize()` の最小構成を理解する。
- ロガー初期化とメインループ構造の雛形を作る。

## 実行

```bash
./build-user-apps-opt/autosar_user_tpl_runtime_lifecycle
```

## 変更ポイント

1. `for` ループ内を自分の業務ロジックへ置き換える。
2. ロガーコンテキストID（`UTRL`/`RTLF`）をアプリ用に変更する。
3. 例外処理が必要なら `main()` に `try-catch` を追加する。
