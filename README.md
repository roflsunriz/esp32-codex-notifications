# ESP32 Codex Notifications

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
3. 十字アイコン: 上下左右、ダイヤル左回転・右回転・押下

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

## ビルドと書き込み

[PlatformIO Core](https://platformio.org/install/cli) または VS Code の PlatformIO IDE を用意し、リポジトリ直下で実行します。

```powershell
pio run
pio run --target upload
pio device monitor
```

起動成功時のシリアル出力は `CODEX_CYD_READY` です。

## Bluetooth接続

1. ファームウェアを書き込み、ESP32を再起動します。
2. WindowsまたはmacOSのBluetooth設定で `Codex Micro` をペアリングします。
3. ChatGPT Desktopを起動します。
4. `Settings > Codex Micro` でAgent Keyと各操作を設定します。
5. 6つのAgent Keyへタスクを割り当て、状態色とタッチ操作を確認します。

古いHID情報がOSに残って検出されない場合は、Bluetooth設定から `Codex Micro` を削除し、ESP32を再起動してペアリングし直してください。

## 検証済み環境

- ESP32-2432S028R / ESP32-D0WD-V3
- Windows 11 x64
- Codex Desktop `26.818.5229.0`
- Codex Desktop同梱 `@worklouder/device-kit-oai 0.2.1`
- BLEペアリング、Windows PnP列挙、`v.oai.rgbcfg`、`v.oai.thstatus`、`device.status` の往復

macOSでは同じBLEプロトコルのM5Stack Core2公開実装が検証されていますが、この基板と本ファームウェアの実機検証はWindowsで行っています。

## タッチ位置の調整

タッチ位置がずれる場合は、基板の `BOOT` ボタンを押したまま再起動します。画面に `TOUCH 1/2` と左上の十字が表示されたら十字を押し、次に `TOUCH 2/2` の右下の十字を押します。調整値はESP32内へ保存されます。

## 安全性と制約

- BLEはパスキーなしの `Just Works` ペアリングです。信頼できる場所でのみペアリングしてください。
- このファームウェアはWi-Fi、OpenAI API、PCのマイク音声へアクセスしません。
- 音声ボタンはPC側のマイクをChatGPT Desktopに操作させます。
- USB端子は給電・書き込み用です。Codex Micro互換通信はBLEのみです。
- 基板にバッテリー計測機能がないため、互換ステータスでは100%を返します。

解析根拠と通信仕様は [docs/protocol.md](docs/protocol.md)、更新方法は [how-to-update.md](how-to-update.md) を参照してください。

## ライセンス

MIT Licenseです。互換トランスポートとボード設定の出典・商標上の注意は [NOTICE.md](NOTICE.md) に記載しています。
