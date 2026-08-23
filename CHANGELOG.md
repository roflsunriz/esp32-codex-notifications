# Changelog

このプロジェクトの重要な変更を記録します。書式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) に従います。

## [Unreleased]

## [0.3.0] - 2026-08-23

### Added

- PCのスリープやシャットダウン後も画面の焼き付きを抑えられるように、Codex MicroのBLE接続が切れてから30秒後に画面とバックライトを自動消灯する機能を追加した。
- 一時的な通信断で画面確認を妨げないように、30秒以内の再接続では消灯を取り消し、消灯後の再接続では画面を自動復帰する処理を追加した。
- PC不在中でも必要なときだけ状態を確認できるように、切断中のタッチ復帰後は30秒を再計測して再び自動消灯するようにした。

## [0.2.0] - 2026-08-23

### Added

- 画面の焼き付きを抑えるため、Codex DesktopのCodex Micro Auto-dimが送る全消灯照明状態と同期し、ILI9341の表示・スリープとバックライトを自動的にOFFにする機能を追加した。
- 誤操作せず画面を確認できるように、消灯中の最初のタッチを復帰専用にし、副作用のない活動通知でDesktop側の照明状態も復帰させる処理を追加した。

### Fixed

- 接続直後またはタッチ復帰直後に全Agentが未割り当ての場合、短時間に複数回届く通常の全消灯状態をAuto-dimと誤認して即座に再消灯しないようにした。

## [0.1.1] - 2026-08-23

### Fixed

- 黄色のダイヤル操作で画面上の向きと値の変化を一致させるため、左ボタンを減少、右ボタンを増加へ修正した。

## [0.1.0] - 2026-08-23

### Added

- ESP32-2432S028RをChatGPT DesktopからCodex Micro互換BLE HIDとして認識できるように、Report 6のJSON-RPC通信を追加した。
- 6つのCodexタスクを離席中でも把握できるように、状態色、状態アイコン、完了・入力待ち・エラー通知を表示するタッチUIを追加した。
- PCへ戻らず主要操作を行えるように、Agent、Command、方向、ダイヤルの全操作画面を追加した。
- 基板ごとのタッチ誤差を補正できるように、BOOT起動による2点調整とバージョン付き保存を追加した。
- プロトコル変更による退行を見つけやすくするため、入力領域と状態色分類のネイティブ単体テストを追加した。
- USBケーブルの取り回しに合わせられるように、物理BOOTボタンの短押しによる180度回転、タッチ座標反転、向きの永続化を追加した。
- 付属ペンの軽い押下を認識できるように、XPT2046ドライバーの圧力閾値を400から120へ変更し、読取周期短縮とBOOTボタン長押しによる実行時2点調整を追加した。
- タグから検証可能なファームウェアを配布できるように、CI、Release workflow、mergedイメージ、分割bundle、SHA-256、build provenance attestationを追加した。

### Security

- Releaseビルド環境の依存を固定してCIとReleaseの両方で `pip-audit` を実行し、PlatformIOが要求するWeb server依存の到達不能なadvisoryだけを根拠・見直し条件付きで限定除外した。

### Changed

- Bluetoothペアリングの要否を判断できるように、ESP-WROOM-32、CH340、Codex DesktopのUSB探索条件とUSB-only化に必要な代替構成を文書化した。
- 回転操作が下部タブへ誤到達しないように、画面内の回転タッチ領域を物理BOOTボタンへ置き換えた。

### Fixed

- 初回起動時に未作成のタッチ設定namespaceを読み取り専用で開いてNVSエラーになる問題を修正した。
- 現行SDKとの応答形式を揃えるため、照明系の成功応答を真偽値へ修正した。
- Windows BLEでRPC応答を確実に返せるように、GATT write callback内の直接notificationを送信queueへ分離した。
- 63-byte HID応答が既定ATT MTUで切り詰められないように、BLEローカルMTUを185へ拡張した。
- Windows HID-over-GATTでvendor input reportが未購読になる問題を避けるため、専用CCCDのnotificationを既定有効にした。
- Windows版Codex Desktop 26.818.5229.0との実機検証で、BLE列挙、bonding、照明設定、6 Agent状態、端末状態のRPC往復を確認した。
- ダイヤル押下が現在のDesktopで認識されるように、HIDキーIDを `ENC_CLK` へ修正した。
- ペンを離す前の圧力揺れを別の押下として扱い、タブ切替後の上部ボタンへ操作が漏れる問題を、IRQ解放まで接触をロックして修正した。
- キャリブレーション後に無操作でもタスクやボタンが発火する問題を防ぐため、近い座標の3回連続取得を必須にし、単発ノイズと座標飛びを破棄した。

[Unreleased]: https://github.com/roflsunriz/esp32-codex-notifications/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/roflsunriz/esp32-codex-notifications/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/roflsunriz/esp32-codex-notifications/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/roflsunriz/esp32-codex-notifications/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/roflsunriz/esp32-codex-notifications/releases/tag/v0.1.0
