# ESP32 Codex Notifications

[![CI](https://github.com/roflsunriz/esp32-codex-notifications/actions/workflows/ci.yml/badge.svg)](https://github.com/roflsunriz/esp32-codex-notifications/actions/workflows/ci.yml)
[![Release firmware](https://github.com/roflsunriz/esp32-codex-notifications/actions/workflows/release.yml/badge.svg)](https://github.com/roflsunriz/esp32-codex-notifications/actions/workflows/release.yml)

ESP32-2432S028R（ILI9341 / XPT2046、通称 CYD）を、ChatGPT Desktop の Codex Micro 機能から直接認識できる BLE コントローラーにする非公式ファームウェアです。

画面には6つのCodexタスク状態が色とアイコンで表示されます。タッチ操作でタスク切り替え、Fast、承認、拒否、フォーク、音声入力、送信、履歴移動、ダイヤル操作を実行できます。PC側の常駐ブリッジやOpenAI APIキーは不要です。

> このプロジェクトは OpenAI、Work Louder、Espressif の公式製品ではありません。文書化されていない互換プロトコルは ChatGPT Desktop の更新で変わる可能性があります。

## 対応ハードウェア

- ESP32-2432S028R
- ESP-WROOM-32
- 320×240 ILI9341 ディスプレイ
- XPT2046 抵抗膜タッチパネル

同じ外観でも ST7789 や ESP32-S3 を搭載した派生基板は、この構成の対象外です。基板とLCDコントローラーの型番を確認してから書き込んでください。

## 画面

下部の3つのアイコンで画面を切り替えます。

1. 6分割アイコン: Agent 1〜6の状態と通知
2. 稲妻アイコン: Fast、承認、拒否、フォーク、マイク、送信
3. 十字アイコン: 上下左右、左でダイヤル値を減少、右で増加、ダイヤル押下

物理 `BOOT` ボタンを短く押して離すと、表示が180度回転します。タッチ座標も同時に反転し、選んだ向きは再起動後も維持されます。

状態表示は公式Codex Microの意味に合わせています。

| 色 / アイコン | 状態 |
| --- | --- |
| 白 / 横線 | 待機中 |
| 青 / 3点 | 処理中 |
| 緑 / チェック | 未読の完了 |
| 黄 / `!` | 承認または入力待ち |
| 赤 / `×` | エラー |
| 消灯 / 円 | タスク未割り当て |

緑・黄・赤へ変わると、画面上部に該当Agentの通知が4秒間表示されます。

## 自動調光と画面OFF

Codex Desktopの `Settings > Codex Micro > Auto-dim` と同期します。Desktopで選んだ30秒、1分、3分、10分、30分、1時間の無操作時間に達すると、TFTの表示とバックライトを自動的にOFFにします。`Off` を選んだ場合は自動消灯しません。

消灯中の最初のタッチは画面復帰だけに使われ、タスク切り替えやコマンドは実行しません。指を離してから改めてタッチすると通常操作になります。Agent Keyの色または状態が変わった場合も、Codex Micro照明と同時に画面が自動復帰します。

PCのスリープ、シャットダウン、Bluetooth切断、Codex Desktop終了などでCodex Microとの接続が切れた場合は、DesktopのAuto-dim設定に関係なく30秒後に画面をOFFにします。30秒以内に再接続すれば消灯を取り消し、既に消灯していた場合も再接続時に自動復帰します。切断中にタッチで復帰した場合は、確認できる状態を30秒間維持してから再び消灯します。

## ビルドと書き込み

[PlatformIO Core](https://platformio.org/install/cli) または VS Code の PlatformIO IDE を用意し、リポジトリ直下で実行します。

```powershell
pio run
pio run --target upload
pio device monitor
```

起動成功時のシリアル出力は `CODEX_CYD_READY` です。

## ビルド済みファームウェア

[GitHub Releases](https://github.com/roflsunriz/esp32-codex-notifications/releases/latest) では、次の検証可能な配布物を公開します。

- `*-merged.bin`: 初回導入・初期化向けに0x0へ一括書き込みするイメージ
- `*-firmware.bin`: 既存のBluetooth pairingと端末設定を維持する更新用アプリイメージ
- `*-bundle.zip`: 分割イメージ、manifest、書き込み手順
- `SHA256SUMS.txt`: SHA-256検証値

Windowsではダウンロード後に次のように検証できます。

```powershell
Get-FileHash .\esp32-codex-notifications-v0.3.0-merged.bin -Algorithm SHA256
```

値が `SHA256SUMS.txt` と一致したら、初回導入ではmergedイメージを0x0へ書き込みます。この操作はBluetooth bonding、タッチ調整、画面方向を含むNVS設定を初期化します。

```powershell
python -m esptool --chip esp32 --port COM3 write_flash 0x0 .\esp32-codex-notifications-v0.3.0-merged.bin
```

`COM3` は実際のCH340ポートに置き換えてください。各Release assetにはGitHub Actionsのbuild provenance attestationも付与します。

既に本ファームウェアを利用中で設定を維持する更新では、`*-firmware.bin` を0x10000へ書き込みます。

```powershell
python -m esptool --chip esp32 --port COM3 write_flash 0x10000 .\esp32-codex-notifications-v0.3.0-firmware.bin
```

## Bluetooth接続

1. ファームウェアを書き込み、ESP32を再起動します。
2. WindowsまたはmacOSのBluetooth設定で `Codex Micro` をペアリングします。
3. ChatGPT Desktopを起動します。
4. `Settings > Codex Micro` でAgent Keyと各操作を設定します。
5. 6つのAgent Keyへタスクを割り当て、状態色とタッチ操作を確認します。

古いHID情報がOSに残って検出されない場合は、Bluetooth設定から `Codex Micro` を削除し、ESP32を再起動してペアリングし直してください。

## USBだけで接続できるか

このESP32-2432S028Rでは、Codex Micro互換通信にBluetoothペアリングが必要です。基板のUSB端子はCH340を介した書き込み・シリアル通信用で、ESP-WROOM-32からUSB HIDのVID、PID、Report Descriptorを制御できません。

Codex Desktopは通常動作のMicroをWork LouderのVID `0x303A`、対応PID、HID Usage Page `0xFF00`で発見します。CH340のCOMポートはこの条件に一致せず、Desktop同梱SDKのシリアル探索もファームウェア書き込み用ブートローダーに限定されています。そのため、ファームウェア変更だけでUSB専用経路を追加することはできません。

USB専用にするには、次のいずれかのハードウェアまたはPC側構成変更が必要です。

- USB Device/HID対応のESP32-S2またはESP32-S3基板へ変更する。
- COMポートと仮想HIDを中継する署名済みWindowsドライバー／常駐ブリッジを別途導入する。

現在の基板では、USBを給電・書き込み、BLEをCodex Micro通信に使う構成が最小で安全です。

## 画面の向きと自動回転

標準のESP32-2432S028Rには加速度・ジャイロセンサーがないため、スマートフォンのような重力による自動回転はできません。物理 `BOOT` ボタンを短く押して180度反転してください。外付け加速度センサーを増設すれば自動検知は可能ですが、標準基板だけで誤検知なく実現する方法はありません。

## 検証済み環境

- ESP32-2432S028R / ESP32-D0WD-V3
- Windows 11 x64
- Codex Desktop `26.818.5229.0`
- Codex Desktop同梱 `@worklouder/device-kit-oai 0.2.1`
- BLEペアリング、Windows PnP列挙、`v.oai.rgbcfg`、`v.oai.thstatus`、`device.status` の往復

macOSでは同じBLEプロトコルのM5Stack Core2公開実装が検証されていますが、この基板と本ファームウェアの実機検証はWindowsで行っています。

## タッチ位置の調整

タッチ位置がずれる場合は、動作中に物理 `BOOT` ボタンを1.5秒以上押して離します。画面に `TOUCH 1/2` と十字が表示されたら十字を押し、次に `TOUCH 2/2` の十字を押します。調整値はESP32内へ保存されます。従来どおり、`BOOT` を押したまま再起動して調整を始めることもできます。

付属タッチペンの短い押下を認識しやすくするため、XPT2046の圧力閾値は標準ライブラリの固定値400から120へ変更し、読取周期を短縮しています。抵抗膜式なので表面をわずかに押し込む必要があります。ノイズによる誤操作を防ぐため、近い座標を3回連続して取得した場合だけ押下が成立します。ドラッグではなくボタン中央を短く押してください。

## 安全性と制約

- BLEはパスキーなしの `Just Works` ペアリングです。信頼できる場所でのみペアリングしてください。
- このファームウェアはWi-Fi、OpenAI API、PCのマイク音声へアクセスしません。
- 音声ボタンはPC側のマイクをChatGPT Desktopに操作させます。
- USB端子は給電・書き込み用です。Codex Micro互換通信はBLEのみです。
- 基板にバッテリー計測機能がないため、互換ステータスでは100%を返します。

解析根拠と通信仕様は [docs/protocol.md](docs/protocol.md)、更新方法は [how-to-update.md](how-to-update.md) を参照してください。

## ライセンス

MIT Licenseです。互換トランスポートとボード設定の出典・商標上の注意は [NOTICE.md](NOTICE.md) に記載しています。
