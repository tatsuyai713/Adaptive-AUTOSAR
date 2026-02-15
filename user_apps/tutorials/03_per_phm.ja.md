# 03: Persistency と PHM

## 対象ターゲット

- `autosar_user_per_phm_demo`
- ソース: `user_apps/src/apps/basic/per_phm_app.cpp`

## 目的

- `ara::per` でカウンタなどの永続データを扱う。
- `ara::phm` で正常/異常/停止状態を報告する。

## 実行

```bash
./build-user-apps-opt/autosar_user_per_phm_demo
```

## 変更ポイント

1. キー名を ECU 機能に合わせて分割する（例: `rx.count`, `tx.count`）。
2. 異常時に `kFailed` を報告する条件を具体化する。
3. 重要値更新後に `SyncToStorage()` を呼ぶタイミングを設計する。
