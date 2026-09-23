# 引き継ぎ: Navigate画面のスクロール／スライダードラッグ不具合

日付: 2026-09-23 / 対象: esp32-codex-notifications / 実機: COM7 Codex Micro (MAC 68:09:47:85:d0:cc)

## 要旨

作戦書 `esp32-ui-style.md` の「notifications第3タブのスクロール＋自動消灯スライダー」を実装したが、
Navigate画面のドラッグ操作が実機で動いていない。タップ操作は動く。ホスト検証は全件緑。
後任はドラッグ経路の実機デバッグから再開すること。

## 確定している不具合（未解決）

1. 空き領域ドラッグでスクロールしない。スライダーのドラッグで値が追従しない。
2. スクロール操作中に画面全体がチラつく（0.2秒周期のON・OFF感）。
3. 「Navigate画面で操作するとクラッシュして最初の画面に戻る」との報告あり（再現・特定できていない）。

## 実測で掴んだ事実

- シリアルログ（115200 baud、DTR/RTSパルスなし接続）で、ドラッグ中に
  `UI input kind=0`（None）の Press が約1.2秒周期で繰り返すことを確認。
  接触が1秒前後で切れてタップに化けている。ドラッグ継続の前提が崩れている。
- 同ログで再起動（`CODEX_CYD_READY` 再出・例外）は観測されず。スライダーの
  タップは正常に受理されていた（kind=7/8、範囲内の値）。
- タブ切り替え・タブ自体のタップは正常。

## 原因仮説（優先度順）

1. ペン圧が押圧閾値付近で、PENIRQがHIGHに戻って接触が切れる。`readTouch` と
   `readDragPoint` が別経路のため、タップは通るがドラッグが続かない。
   対策案: ドラッグ継続条件を緩和する（短い途切れを同一接触とみなす猶予）、
   またはタップ座標系へ一本化する。ただし安易な猶予はBLEキー離し
   （`release()`）の遅延になるため、ドラッグ専用の猶予に留めること。
2. XPT2046の `getPoint()` 末尾の制御バイトでPD0=1が残りPENIRQが無効化される
   可能性。`lib/sensitive-xpt2046` は末尾 `transfer16(0)` でPD0=0を維持する
   設計だが、この個体での実測は未確認。
3. チラつきはドラッグ毎の `drawAll()` 全再描画が原因の公算。`setNavigateScroll` /
   `dragSleepSlider` は Description通りの部分描画・間引き済みだが未検証。
4. 「最初の画面に戻る」は、0除算クラッシュ（後述の修正済み）とは別件の可能性。
   再現時はシリアルログで例外・再起動の有無を確認すること。

## 実施済みの修正（いずれも実機で効果未確認）

- `add88bb` Navigate画面へ自動消灯メニュー（0〜59分・0〜24時間）とスクロール追加。
  本体側無操作タイマー（`DisplayPowerSync` アイドル期限、NVS `codex-ui` の
  `sleep_sec`、0で無効）。ポーリング取得なしのため取得間隔なし。
- `2c41bfb` 空きタップの種別確定、スクロールバーのタップページング、
  エンコーダボタン縮小（スクロールバー域から除外）。
- `faf5860` ドラッグ追従・タブ残像対応（後に不具合判明）。
- `700da9b` フィルタ凍結の修正（`TouchSampleFilter` 確定後も追従）。
- `0b5eb13` **長押しの0除算を修正**（接触継続の加算で `count_` が溢れて
  除算エラー→再起動していた。これが「クラッシュ」の有力原因。修正後は
  有界な移動平均＋離し時の最終再描画）。
  - 対応する退行テスト `testDragTrackingFollowsFingerWithoutRedelivery` 追加。
  - `DeviceUi::refresh()` 追加、`release()` でドラッグ後は最終再描画。

## 実機情報・手順

- COM7、CH340、MAC 68:09:47:85:d0:cc。退避:
  `.local/backups/micro-com7-20260923.bin`（4,194,304バイト、
  SHA-256 `1bd8d8bdf6a3e9543053c4d4c08072475bc623217c6eb37d18b6c7b5984e096b`、
  `.json` 付き）。`verify-flash` 照合済み。書き換えはアプリ領域のみ。
- ダウンロードモードは毎回 BOOT保持＋RST→解放（esptool終了で抜ける）。
  書き込みは BOOT保持中に `pio run -e cyd -t upload --upload-port COM7`。
- esptoolは `.platformio` 同梱 v4系（`_` 表記、`--before no_reset`）と
  venv v5系の使い分けに注意。この環境では `pio` パイプがデッドロックする
  ためバックグラウンドジョブ＋ログファイルで実行した。
- シリアル監視は pySerial を `port=None` で作り `dtr/rts=False` を先に設定して
  `open()` する（DTRパルスで再起動・誤作動するため）。
  参考スクリプトは `/Users/UserName/AppData/Local/Temp/opencode/codex_log.py`
  （リポジトリ外・Git管理外）。

## リポジトリ状態

- `main` は `origin/main` より5件先行（未push）。push・リリースの許可は未取得。
- ホスト検証: `ui-model` 17件、`python -m unittest discover -s test`、
  `pio run -e cyd` が成功。
- `esp32-ui-style.md` の他機（opencode v0.4.0、codex v0.4.0、povo v0.7.0）は
  リリース済み。残作業は本件のみ。

## 後任への提案

1. まずドラッグ中のシリアルログを取り、Press周期と `readDragPoint` の成否を
   確認する（デバッグ print を一時追加する場合は製品コードに残さない）。
2. 接触の猶予（ドラッグ専用）と描画の間引きを見直す。
3. 目視確認が取れたら `verification.md`・`CHANGELOG.md` を更新し、
   所有者にpush・リリースの許可を求める。
