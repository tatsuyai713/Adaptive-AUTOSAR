# 32: プラットフォームヘルス管理デーモン

## 対象

`autosar_phm_daemon` — ソース: `src/main_phm_daemon.cpp`

## 目的

PHM デーモンは **プラットフォーム全体のヘルスオーケストレータ** として、
登録されたすべてのエンティティの監視結果を集約し、
プラットフォーム全体のヘルス状態 (Normal / Degraded / Critical) を評価します。

`autosar-phm-daemon.service` として配備され、監視対象アプリケーションとは
独立して動作し、定期的にヘルスサマリを書き出します。

## アーキテクチャ

```
  ┌─────────────────────────────────────────────┐
  │              autosar_phm_daemon              │
  │                                             │
  │  PhmOrchestrator                            │
  │  ├─ Entity: vsomeip_routing  → kOk          │
  │  ├─ Entity: time_sync        → kOk          │
  │  ├─ Entity: persistency      → kOk          │
  │  ├─ Entity: diag_server      → kFailed      │
  │  ├─ Entity: crypto_provider  → kOk          │
  │  ├─ …                                       │
  │  └─ Platform Health: kDegraded              │
  │                                             │
  │  → /run/autosar/phm.status                  │
  └─────────────────────────────────────────────┘
         ▲                        ▲
         │  ara::phm API          │  ara::phm API
  ┌──────┴──────┐          ┌──────┴──────┐
  │ ユーザーアプリ│          │ プラットフォーム│
  │ (ハートビート)│          │ デーモン群    │
  └─────────────┘          └─────────────┘
```

## ヘルス状態

### プラットフォームヘルス

| 状態 | 意味 |
|------|------|
| `kNormal` | 全エンティティ OK、または劣化閾値以内 |
| `kDegraded` | 障害エンティティ比率が劣化閾値超過 |
| `kCritical` | 障害エンティティ比率が危機閾値超過 |

### エンティティ監視ステータス

| ステータス | 意味 |
|-----------|------|
| `kOk` | 生存確認済、デッドライン遵守 |
| `kFailed` | 監視障害検出 |
| `kExpired` | 生存タイムアウト超過 |
| `kDeactivated` | 監視無効化 |

## 環境変数

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_PHM_PERIOD_MS` | `1000` | 評価サイクル (ms) |
| `AUTOSAR_PHM_DEGRADED_THRESHOLD` | `0.0` | 障害比率 → kDegraded |
| `AUTOSAR_PHM_CRITICAL_THRESHOLD` | `0.5` | 障害比率 → kCritical |
| `AUTOSAR_PHM_ENTITIES` | *(13 プラットフォームサービス)* | カンマ区切りエンティティ名 |
| `AUTOSAR_PHM_STATUS_FILE` | `/run/autosar/phm.status` | ステータス出力パス |

### デフォルト監視エンティティ

```
vsomeip_routing,time_sync,persistency,iam_policy,sm_state,
can_manager,dlt,network_manager,crypto_provider,diag_server,
ucm,user_app_monitor,watchdog
```

## 実行（スタンドアロン）

```bash
# デフォルトで実行
./build-local-check/bin/autosar_phm_daemon

# 閾値をカスタマイズ
AUTOSAR_PHM_DEGRADED_THRESHOLD=0.1 \
AUTOSAR_PHM_CRITICAL_THRESHOLD=0.3 \
  ./build-local-check/bin/autosar_phm_daemon
```

## ステータスファイル

デーモンは `/run/autosar/phm.status` に書き出します:

```
platform_health=kNormal
entity_count=13
entity.vsomeip_routing.status=kOk
entity.vsomeip_routing.failures=0
entity.time_sync.status=kOk
entity.time_sync.failures=0
entity.diag_server.status=kFailed
entity.diag_server.failures=2
failed_entity_count=1
updated_epoch_ms=1717171717000
```

リアルタイム監視:

```bash
watch -n1 cat /run/autosar/phm.status
```

## systemd サービス

```bash
sudo systemctl enable --now autosar-phm-daemon.service
sudo systemctl status autosar-phm-daemon.service --no-pager
```

サービス起動順序:
- **After**: `autosar-platform-app.service`
- **Before**: `autosar-exec-manager.service`
- **Restart**: `on-failure` (2 秒遅延)

## ユーザーアプリとの連携

ユーザーアプリは `ara::phm::HealthChannel` 経由で PHM デーモンにヘルス報告します:

```cpp
#include "ara/phm/health_channel.h"

ara::core::InstanceSpecifier spec("/app/health");
ara::phm::HealthChannel hc(spec);

// 正常報告
hc.ReportHealthStatus(ara::phm::HealthStatus::kNormal);

// 障害報告
hc.ReportHealthStatus(ara::phm::HealthStatus::kFailed);
```

ユーザーアプリ側の例はチュートリアル [03_per_phm](03_per_phm.ja.md) を参照。

## 次のステップ

| チュートリアル | トピック |
|---------------|---------|
| [03_per_phm](03_per_phm.ja.md) | ユーザーアプリからのヘルス報告 |
| [04_exec_runtime_monitoring](04_exec_runtime_monitoring.ja.md) | 生存タイムアウト監視 |
| [33_crypto_provider](33_crypto_provider.ja.md) | 暗号プロバイダデーモン |
