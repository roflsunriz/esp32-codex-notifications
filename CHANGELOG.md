# Changelog

このプロジェクトの重要な変更を記録します。書式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) に従います。

## [Unreleased]

### Added

- ESP32-2432S028RをChatGPT DesktopからCodex Micro互換BLE HIDとして認識できるように、Report 6のJSON-RPC通信を追加した。
- 6つのCodexタスクを離席中でも把握できるように、状態色、状態アイコン、完了・入力待ち・エラー通知を表示するタッチUIを追加した。
- PCへ戻らず主要操作を行えるように、Agent、Command、方向、ダイヤルの全操作画面を追加した。
- 基板ごとのタッチ誤差を補正できるように、BOOT起動による2点調整とバージョン付き保存を追加した。
- プロトコル変更による退行を見つけやすくするため、入力領域と状態色分類のネイティブ単体テストを追加した。

### Fixed

- 初回起動時に未作成のタッチ設定namespaceを読み取り専用で開いてNVSエラーになる問題を修正した。
- 現行SDKとの応答形式を揃えるため、照明系の成功応答を真偽値へ修正した。
- Windows BLEでRPC応答を確実に返せるように、GATT write callback内の直接notificationを送信queueへ分離した。
- 63-byte HID応答が既定ATT MTUで切り詰められないように、BLEローカルMTUを185へ拡張した。
- Windows HID-over-GATTでvendor input reportが未購読になる問題を避けるため、専用CCCDのnotificationを既定有効にした。
- Windows版Codex Desktop 26.818.5229.0との実機検証で、BLE列挙、bonding、照明設定、6 Agent状態、端末状態のRPC往復を確認した。
- ダイヤル押下が現在のDesktopで認識されるように、HIDキーIDを `ENC_CLK` へ修正した。
