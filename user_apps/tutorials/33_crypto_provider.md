# 33: Cryptography Provider

## Target

`autosar_crypto_provider` — source: `src/main_crypto_provider.cpp`

## Purpose

The crypto provider daemon delivers **AUTOSAR Cryptography key management
and cryptographic operations** as a platform service.
It initializes the HSM (or software-emulated) crypto provider, manages key
slots, supports key rotation, and runs periodic self-tests.

Deployed as `autosar-crypto-provider.service`, it starts before all other
platform daemons (Tier 1 — after `local-fs.target`) so that crypto services
are available from the earliest boot stage.

## Features

- **HSM initialization** via `ara::crypto::HsmProvider`
- **Key slot management** — allocate, occupy, lock, query
- **Key rotation** — automatic rotation on configurable interval
- **Self-test** — periodic encrypt/decrypt/verify cycle using AES-128 and HMAC-SHA256
- **Status reporting** to `/run/autosar/crypto_provider.status`

## Architecture

```
  ┌─────────────────────────────────────────────┐
  │           autosar_crypto_provider            │
  │                                             │
  │  HsmProvider                                │
  │  ├─ Token: "AutosarEcuHsm"                 │
  │  ├─ Key Slots: [occupied/empty/locked]      │
  │  ├─ Self-Test: AES-128 + HMAC-SHA256        │
  │  └─ Key Rotation: (if enabled)              │
  │                                             │
  │  KeyStorageProvider                         │
  │  └─ /var/lib/autosar/crypto/keys            │
  │                                             │
  │  → /run/autosar/crypto_provider.status      │
  └─────────────────────────────────────────────┘
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `AUTOSAR_CRYPTO_TOKEN` | `AutosarEcuHsm` | HSM token identifier |
| `AUTOSAR_CRYPTO_PERIOD_MS` | `5000` | Main loop cycle (ms) |
| `AUTOSAR_CRYPTO_KEY_ROTATION` | `false` | Enable key rotation |
| `AUTOSAR_CRYPTO_ROTATION_INTERVAL_MS` | `3600000` | Rotation interval (1 hour) |
| `AUTOSAR_CRYPTO_SELF_TEST` | `true` | Enable periodic self-test |
| `AUTOSAR_CRYPTO_SELF_TEST_INTERVAL_MS` | `60000` | Self-test interval (1 min) |
| `AUTOSAR_CRYPTO_STATUS_FILE` | `/run/autosar/crypto_provider.status` | Status output |
| `AUTOSAR_CRYPTO_KEY_STORAGE_PATH` | `/var/lib/autosar/crypto/keys` | Key storage directory |

## Run (Standalone)

```bash
# Run with defaults
./build-local-check/bin/autosar_crypto_provider

# Enable key rotation for testing
AUTOSAR_CRYPTO_KEY_ROTATION=true \
AUTOSAR_CRYPTO_ROTATION_INTERVAL_MS=30000 \
AUTOSAR_CRYPTO_SELF_TEST_INTERVAL_MS=10000 \
  ./build-local-check/bin/autosar_crypto_provider
```

## Status File

The daemon writes to `/run/autosar/crypto_provider.status`:

```
initialized=1
total_slots=16
occupied_slots=4
self_tests_passed=12
self_tests_failed=0
key_rotations=0
updated_epoch_ms=1717171717000
```

Monitor live:

```bash
watch -n5 cat /run/autosar/crypto_provider.status
```

## Self-Test Cycle

Each self-test iteration:

1. Generate a temporary AES-128 key
2. Encrypt a test plaintext block
3. Decrypt and compare
4. Generate a HMAC-SHA256 over the plaintext
5. Verify the HMAC

If any step fails, `self_tests_failed` increments and the event is logged.

## systemd Service

```bash
sudo systemctl enable --now autosar-crypto-provider.service
sudo systemctl status autosar-crypto-provider.service --no-pager
```

Service ordering:
- **After**: `local-fs.target`
- **Before**: `autosar-platform-app.service`
- **Restart**: `on-failure` (delay 3 s)

The crypto provider deliberately starts **before** the platform application
binary so that encryption services are available at boot.

## Key Storage

Keys are persisted to `AUTOSAR_CRYPTO_KEY_STORAGE_PATH`
(default: `/var/lib/autosar/crypto/keys`).

```bash
# Check key storage
ls -la /var/lib/autosar/crypto/keys/

# Ensure directory exists with correct permissions
sudo mkdir -p /var/lib/autosar/crypto/keys
sudo chmod 700 /var/lib/autosar/crypto/keys
```

## Next Steps

| Tutorial | Topic |
|----------|-------|
| [30_platform_service_architecture](30_platform_service_architecture.md) | Full platform overview |
| [31_diag_server](31_diag_server.md) | Diagnostic server |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.md) | Deploy to RPi |
