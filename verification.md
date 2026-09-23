# 検証手順

## Navigate自動消灯メニュー（2026-09-23、ドラッグ修正の実機確認待ち）

作戦書 `esp32-ui-style.md` に基づき、Navigate画面の下部へ0〜59分・0〜24時間のスライダーによる本体側の無操作タイマーを追加し、画面をスクロール対応にした。0分0時間は本体側の自動消灯を無効にする。設定はNVS `codex-ui` の `sleep_sec` へ保存する。

- ドラッグ不能の原因は、安定した最初の座標でのみ `true` になる `TouchSampleFilter::push()` の結果を、毎ループの `touched` として扱っていたこと。継続する座標を `current()` から返し、1回の読取値を押下判定とドラッグで共有するよう修正した。描画はNavigateの内容領域へ限定した。
- ホストの`ui-model`テスト18件、Python単体テスト9件、`pio run -e cyd` が成功した。`pio check -e cyd --skip-packages` は終了コード0だが、依存先ArduinoJson内の警告を含む。CMakeはこのWindows環境で`PATH`/`Path`の重複によりMSBuildのコンパイラ試験が失敗したため、同じC++ソースをclang++で直接コンパイルして18件を実行した。
- COM7基板（MAC 68:09:47:85:d0:cc）の全4,194,304バイトを `.local/backups/micro-com7-20260923.bin` へ退避し、`verify-flash` のdigest一致を確認した。退避データにCodex Micro識別子があり他機種識別子がないことを確認した。
- 今回の修正後の実機操作は未検証。書込時はアプリ領域のみ更新し、NVSの校正値・設定値を維持する。既存の基板を更新する前に所有者の書込指示を確認する。

実機での確認順序:

1. Navigateタブの空き領域を上下にドラッグし、指の動きに合わせて内容が移動することを確認する。ヘッダーと下部タブが消えたり、内容に上書きされたりしないことも見る。
2. 分・時間のスライダーを左右にドラッグし、つまみが追従し、離すと表示値が確定することを確認する。0分0時間と最大24時間59分、再起動後の保持も確認する。
3. シリアル115200 baudで、1回の連続接触が繰り返しの `UI input` にならず、操作中に `CODEX_CYD_READY` が再出力されないことを確認する。再出力があれば例外ログを保存する。
4. Agent/Commandの長押しを離したときBLEキーがすぐ解放され、画面反転後もドラッグ座標が合うことを確認する。

## タッチ位置・押圧感度（実機未検証）

ホストの`ui-model`テストとファームウェアビルドに成功した。起動後BOOTを1.5秒以上押して離し、普段使うペンで左上と右下の十字を押して離す。調整後は両方向のタブ・ボタン操作、指での操作、タッチしない時の誤動作なし、再起動後の保持を確認する。十字の押下が取得できない場合は15秒で元の調整値へ戻ることを確認する。接続中のCodex Micro基板は、所有者の検証指示なしには書き換えない。

v0.4.0のmain CIとタグ起点リリースが成功し、公開bundle内の全コンポーネントのmanifest SHA-256、公開結合イメージのSHA-256、GitHub provenance attestationを検証した。実機操作の未検証状態は変わらない。

## Codex Micro Brightness同期

1. ESP32-2432S028RをUSBで接続し、Codex Desktopへ `Codex Micro` としてBLE接続します。
2. シリアルモニターを115200 baudで開き、`CODEX_CYD_READY` と `BLE host connected` を確認します。
3. `Settings > Codex Micro > Brightness` を100%、50%、10%、0%の順に変更します。
4. 画面が段階的に暗くなり、ログが次の値になることを確認します。

| Brightness | 期待するログ |
| --- | --- |
| 100% | `UI backlight duty=255` |
| 50% | `UI backlight duty=128` |
| 10% | `UI backlight duty=26` |
| 0% | `UI backlight duty=0` |

5. 0%より大きい値へ戻すと、対応する明るさへ復帰することを確認します。
6. 確認後はBrightnessを元の値へ戻します。

色またはeffectが有効なAgentが一つもなく、ambientとkeysも完全に消灯している場合、Desktopは全ゾーンへ同じ全off値だけを送るため、新しい設定値を通信から識別できません。その場合はAgentを一つ割り当てるか状態を変化させ、有効な照明更新を発生させてから再確認します。

## 自動検証

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
$env:PLATFORMIO_CORE_DIR = (Resolve-Path .\.pio-core).Path
pio run -e cyd
pio check -e cyd --skip-packages
```

CMake用C++コンパイラがない環境では、ホスト単体テストは実行できません。PlatformIOのファームウェアビルドと上記の実機確認で代替し、未実行理由を記録します。
