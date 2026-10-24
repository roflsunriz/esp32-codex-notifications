# 調査知見

- 本体の無操作消灯はDesktopのAuto-dim同期とは別。NVS `codex-ui/sleep_sec` に保存し、0分0時間は本体timer無効。消灯中のpollingはないため取得間隔sliderを追加しない。
- `TouchSampleFilter::push()` は安定点確定時だけtrueで、毎loopの接触状態には使えない。dragは `current()` の追従座標と1回のreadを共有し、releaseはIRQで判定する。BLE key releaseへdrag猶予を混ぜない。
- Navigate画面を直接clear→drawするとscroll/sliderがちらつく。小さな16-bit Spriteへ帯描画して転送し、`refresh()`/`drawNavigate()` の原点とheader/Bluetooth欄の順序を揃える。
- touch calibrationは位置とpressure thresholdを検証付き単一blobへ保存し、失敗時は旧値へ戻す。抵抗膜に接触せずPENIRQが出ないpenはthreshold変更では直らない。
- Navigateの時・分は2列の縦ドラムで選び、即時消灯はDesktop Auto-dimとは別に `DisplayPowerSync` へ反映する。同じ押下は復帰へ流さず、保存済み `sleep_sec` は変更しない。リポジトリ内 `.pio-core` のesptool依存にACL破損がある場合は、破損キャッシュを修復するまで正常な隔離coreを指定して同じPlatformIO環境を検証する。
