# Codex Micro互換プロトコル調査

調査日: 2026-08-23

## 確認した情報源

- [OpenAI Docs: Codex Micro](https://learn.chatgpt.com/docs/features/codex-micro): 6つのAgent Key、状態色、既定操作、BLE接続、ChatGPT Desktop設定の公式仕様。
- [ZyoungInc/codex-keyboard](https://github.com/ZyoungInc/codex-keyboard) commit `2ee23a4ab696f94bb78d250f28cc4a9b879ba079`: ESP32 BLE HIDとしてChatGPT Desktopに認識させるMIT実装。
- [mpociot/codex-micro-stream-deck-emulator](https://github.com/mpociot/codex-micro-stream-deck-emulator): HID Report 6、フレーミング、RPCメソッドを独立に再現したMIT実装。
- [arthurcolle/codex-micro-open](https://github.com/arthurcolle/codex-micro-open): Creator Micro 2実機のUSB/BLE HID解析とJSON-RPCの独立検証。
- [witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display): ESP32-2432S028Rの表示・タッチ配線とPlatformIO設定。

OpenAI Docsは製品の利用方法と観測可能な状態を説明していますが、通信バイト列は公開していません。以下の通信詳細は、複数の公開互換実装で一致した非公式仕様です。

## BLE HID識別

| 項目 | 値 |
| --- | --- |
| デバイス名 | `Codex Micro` |
| Manufacturer | `Work Louder` |
| Vendor ID | `0x303A` |
| Product ID | `0x8360` |
| Usage Page | `0xFF00` |
| Report ID | `6` |
| Input / Output body | 各63 bytes |

BLE HOGPではReport IDがCharacteristicの識別に使われ、通常は63-byte bodyに含まれません。互換ブリッジを考慮し、受信側は先頭にReport ID 6が付く64-byte形式も受け入れます。
63-byte input notificationを切り詰めず送れるよう、ESP32側のローカルATT MTUは185に設定します。Windows HID-over-GATTではvendor input reportのCCCDが自動購読されない場合があるため、この接続専用Characteristicはnotificationを既定有効にします。

## 対象基板のUSB境界

OpenAI Docsでは公式Codex MicroがUSB-CまたはBluetoothで接続できると案内されています。一方、このプロジェクトのESP32-2432S028RはESP-WROOM-32とCH340 USB-UART変換器の構成で、ESP32-S2/S3のようなUSB Device peripheralを持ちません。

Codex Desktop `26.818.5229.0` 同梱SDKの通常デバイス探索は次をすべて要求します。

- HID Vendor ID `0x303A`
- DEVICE_REGISTRYに存在するProduct ID
- Usage Page `0xFF00`

シリアルポート探索はVID `0x303A`のファームウェア書き込み用ブートローダーだけを対象にします。実機のCH340は別のVID/PIDであり、ESP32ファームウェアからCH340のUSB descriptorは変更できません。このため、現在の基板へUSB-only接続を追加するにはPC側仮想HIDブリッジが必要で、ファームウェア単体の機能としては成立しません。

## 画面回転

標準基板には姿勢センサーがないため、重力方向の自動検出は行いません。物理BOOTボタンの短押しでTFTをrotation 1/3間で切り替え、タッチ座標を `(319-x, 239-y)` へ変換します。向きはバージョン付きPreferencesへ保存します。タッチ調整値は常にrotation 1の物理座標で保持し、表示方向を変えても再調整を不要にしています。

付属ペンでの押下を安定させるため、Paul StoffregenのMIT版XPT2046ドライバーを閾値可変にしたローカル実装を使います。押下閾値は120、解放閾値は35で、固定閾値400の上流版より軽い筆圧を取得します。読取間隔は1ms、メインループ待機は2msとし、BLE送信queueより先にタッチを読み取って短い押下の取りこぼしを減らします。さらに18px以内の座標を3回連続して取得した場合だけ押下を通知し、単発ノイズと座標飛びを破棄します。物理BOOTボタンを1.5秒以上押して離すと、実行中に2点調整を開始できます。

## フレーム

63-byte bodyは次の構成です。

| Offset | 内容 |
| --- | --- |
| 0 | channel。RPCは `2` |
| 1 | このreport内のpayload長（0〜61） |
| 2〜62 | UTF-8 JSON断片と0 padding |

デバイスからホストへ送るJSONは末尾へ改行を追加し、最大61 bytesずつ通知します。ホストからデバイスへのJSONは改行なしで分割されるため、ArduinoJsonの `IncompleteInput` を利用して完全なJSONになるまで再構成します。入力は4096 bytesで打ち切り、破損・過大入力でメモリを消費し続けないようにしています。

## RPC

ホスト要求には `method`、`params`、`id` が入り、応答は同じ `id` と `result` または `error` を返します。
照明系の成功応答は `result: true`、`sys.version` は `{version: string}` を返します。BLE write callback内で直接notificationを送らず、メインループの送信queueから返すことでWindows BlueDroid上の再入を避けます。

| ホストから受信 | 用途 |
| --- | --- |
| `sys.version` | ファームウェア版 |
| `device.status` | profile、layer、battery |
| `v.oai.thstatus` | 6つのAgent状態色とeffect。Auto-dim時は全Agent消灯 |
| `v.oai.rgbcfg` | ambient/key照明設定。Auto-dim時は両方消灯 |
| `lights.preview` | 照明プレビュー |
| `host.focused_app` | フォーカス中アプリ通知 |

| ESP32から送信 | 用途 |
| --- | --- |
| `v.oai.hid` | Agent、Command、Encoderの押下・解放・回転 |
| `v.oai.rad` | アナログスティック方向と距離 |

永続設定、ファイルシステム、ブートローダーなどの破壊的RPCは実装していません。未知の要求にはJSON-RPC互換の `-32601 Method not found` を返します。

## Auto-dimと画面電源同期

Codex Desktop `26.818.5229.0` の同梱実装では、Auto-dim設定値をデバイスへ直接送る専用RPCはありません。設定値はDesktop側で30秒、1分、3分、10分、30分、1時間のタイマーへ変換され、期限に達すると次の既存RPCを順に送ります。

1. `v.oai.rgbcfg` でambientとkeysを `effect: off`、brightness 0、color 0にする。
2. `v.oai.thstatus` で6つすべてを `effect: off`、brightness 0、color 0にする。

ESP32はこの全消灯ペアをDesktop側Auto-dimの同期信号として扱い、ILI9341へDisplay OffとSleep Inを送り、GPIO 21のバックライトもLOWにします。独立した消灯時間をESP32へ保存しないため、DesktopでAuto-dimを変更した時点から新しい時間がそのまま適用されます。

消灯中の最初のタッチは操作へ変換せず、未割り当てキーID `__WAKE__` のHID通知だけを送ります。Desktop側は任意のHID通知を照明アクティビティとして先に処理してからキー割り当てを評価するため、アプリ操作を発生させず現在の照明状態を再送できます。ESP32はSleep Out後に表示を再描画し、同じ物理接触が離れるまでは次の操作を受け付けません。

通信上、全Agentが未割り当ての通常状態とAuto-dimの全消灯payloadは同一です。接続直後とタッチ復帰直後の5秒間に届く全消灯ペアはすべて通常状態として扱い、Desktopの最短Auto-dim時間である30秒より前の重複再送で消灯しないようにします。猶予終了後にDesktopが送る全消灯ペアで消灯します。

## 互換性境界

これはOpenAIまたはWork Louderが安定性を保証した公開APIではありません。ChatGPT Desktop更新後は、次を実機で再確認します。

1. OSが `303A:8360` / Report 6として列挙すること。
2. `device.status` の要求へ10秒以内に応答すること。
3. 6 Agentの `v.oai.thstatus` が到達すること。
4. 全操作のpress/releaseとEncoder stepが重複しないこと。
5. 切断後にBLE advertisingが再開すること。
6. Auto-dimの各設定時間で画面が消灯し、最初のタッチでは操作せず復帰すること。

## 実機検証

2026-08-23にESP32-2432S028RとWindows版Codex Desktop `26.818.5229.0` の組み合わせで確認しました。Desktop同梱の `@worklouder/device-kit-oai` は `0.2.1`、依存する `@worklouder/wl-device-kit` は `0.2.2` です。

確認結果:

- WindowsのPnP状態が `OK` の `Codex Micro` としてBLE列挙された。
- bonding後の再書き込み・再起動で自動再接続した。
- `v.oai.rgbcfg` への応答notificationが成功した。
- 応答後に `v.oai.thstatus` と `device.status` が順番に到達した。
- 応答前に発生していた10秒周期の `v.oai.rgbcfg` 再送が停止した。
- シリアル起動ログにNVS、JSON、GATT、heapのエラーがない。
- 物理BOOT短押しで180度回転し、再起動後も向きが保持された。
- 押下閾値75と2点調整後、付属ペンで下部タブと6つのAgent領域を個別に操作できた。
- 圧力が揺れてもIRQ解放までは1接触として保持され、タブ切替後の上部ボタンへ入力が漏れなかった。
- v0.2.0を書き込んだ実機でAuto-dim期限到達時にDisplay Off、Sleep In、バックライトOFFへ移行した。
- 消灯中の最初のタッチがアプリ操作を発生させず画面を復帰し、Desktopから通常照明が再送された後も復帰猶予中の重複全消灯で再消灯しなかった。
- タッチ復帰後、次のAuto-dim期限で再び画面が消灯した。

切断後の再広告は目視・操作を伴うため、リリース前の手動確認項目として残します。
