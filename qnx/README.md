# QNX800 Cross-Build

このディレクトリには、QNX SDP 8.0 を使って本リポジトリをクロスコンパイルするためのスクリプトをまとめています。

## 対象
- ホスト: Linux
- ターゲット: QNX 8.0 (`aarch64le` / `x86_64`)
- 前提: `QNX_HOST` と `QNX_TARGET` が設定済み

---

## ビルド 

QNX ビルドを実行する場合の簡易手順：

**注意**: ビルドされたライブラリは `/opt/qnx` にシステムワイドでインストールされます。

```bash
# プロジェクトルートへ移動
cd ~/Adaptive-AUTOSAR

# ヘルパースクリプトでビルド
./qnx/docker_build_qnx.sh cyclonedds    # CycloneDDS のみ
./qnx/docker_build_qnx.sh iceoryx       # iceoryx のみ
./qnx/docker_build_qnx.sh all           # 全ミドルウェア
./qnx/docker_build_qnx.sh autosar       # AUTOSAR AP
```

アーキテクチャを指定する場合：

```bash
./qnx/docker_build_qnx.sh cyclonedds aarch64le   # ARM64 (Raspberry Pi等)
./qnx/docker_build_qnx.sh all x86_64             # x86_64
```

### メモリ不足エラーが出る場合

メモリを 6GB 以上に増やすか、並列数を制限：

```bash
export AUTOSAR_QNX_JOBS=2
./qnx/docker_build_qnx.sh cyclonedds
```

---

## 1. 環境設定 (手動セットアップの場合)

```bash
source /path/to/qnxsdp-env.sh
source qnx/env/qnx800.env.example
```

必要に応じて `qnx/env/qnx800.env.example` をコピーして値を調整してください。

## 2. 依存確認（ホスト側）

```bash
./qnx/scripts/install_dependency.sh
```

- このスクリプトは `apt` などのパッケージマネージャは使いません。
- 必須コマンドの存在確認と、必要ならホストツールのビルドを行います。

## 3. ミドルウェアをクロスビルド

```bash
./qnx/scripts/build_libraries_qnx.sh all install
```

個別ビルド例:

```bash
./qnx/scripts/build_libraries_qnx.sh cyclonedds install --disable-shm
./qnx/scripts/build_libraries_qnx.sh vsomeip install
./qnx/scripts/build_libraries_qnx.sh all clean
```

## 4. AUTOSAR AP ランタイムをクロスビルド

```bash
./qnx/scripts/build_autosar_ap_qnx.sh install
```

## 5. user_apps をクロスビルド

```bash
./qnx/scripts/build_user_apps_qnx.sh build
```

## 6. 一括実行

```bash
./qnx/scripts/build_all_qnx.sh install
```

## 主なスクリプト
- `qnx/scripts/build_libraries_qnx.sh`: Boost / iceoryx / Cyclone DDS / vSomeIP
- `qnx/scripts/build_autosar_ap_qnx.sh`: 本体ライブラリ/実行バイナリ
- `qnx/scripts/build_user_apps_qnx.sh`: user_apps
- `qnx/scripts/build_all_qnx.sh`: 上記を順番に実行
- `qnx/cmake/toolchain_qnx800.cmake`: QNX用 CMake toolchain

## 出力先（デフォルト）
- `${AUTOSAR_QNX_OUT_ROOT:-<repo>/out/qnx}`
  - `install/third_party/<arch>`
  - `install/middleware/<arch>`
  - `install/autosar_ap/<arch>`
  - `build/*`

## 補足
- 実機デプロイ手順（コピー/起動）は、ターゲット環境の運用ポリシーに合わせて別途用意してください。
