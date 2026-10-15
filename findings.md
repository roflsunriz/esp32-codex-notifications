# findings.md

このリポジトリ固有の実現手段と調査知見の正本です。作業前に全文を読み、行動規範は [AGENTS.md](AGENTS.md) と [COMMON-AGENTS.md](COMMON-AGENTS.md) に従ってください。移動元の本文は以下に保持しています。

## 共通方針の適用と具体的な例外

| 項目 | このリポジトリでの扱い |
| --- | --- |
| 対象の共通ルール | 検証の言語別適用と実機条件 |
| 理由と適用範囲 | ESP32/C++のファームウェアとしてPlatformIOとPython/CMakeの既存検証を使い、TypeScript/Bun用コマンドは適用しない。対象はcyd firmwareとホスト側検証。 |
| 同等の品質を保つ手段・検証 | CIのPythonテスト、CMake/CTest、`pio run -e cyd`、`pio check`と実機操作を分けて確認する。書込みと設定初期化は更新手順の条件を満たす場合だけ行う。根拠: `platformio.ini`、`.github/workflows/ci.yml`、[検証記録](verification.md)。 |

## 文書の役割と適用範囲

- 共通ルールの対象は「ローカル AGENTS.md との関係」「設計と実装方針」「検証」「ドキュメント」。この文書は本リポジトリの構成と既存運用に適用し、共通の行動規範を弱めません。
- ファイル配置・ツール・環境をここに集約する理由は、作業規範と実装上の事実を分け、次の作業者が根拠を確認できるようにするためです。下記の既存手段は同じ対象の設定・CI・検証記録と照合して使います。
- README は利用開始・操作、更新手順は更新と復旧、verification.md は検証手順と実測証跡、専門の設計・研究文書は詳細な根拠を担います。これらの用途は維持し、この文書から参照します。
- 更新・検証の代替は一律のBunコマンドに固定せず、下記の採用技術と既存CIに対応する検査を使います。実行不能や未検証の条件は成功扱いせず verification.md に残します。

<a id="agents-1"></a>

## Environment
<必要に応じて適宜書き足すこと。>

移動元: `AGENTS.md`。

<a id="agents-2"></a>

## 本体側の自動消灯（2026-09-23、操作はCOM7実機確認）

- DesktopのAuto-dim同期とは別に、本体側の無操作タイマー（`DisplayPowerSync` のアイドル期限、タッチ操作で延長）を備える。Navigate画面下部の0〜59分・0〜24時間スライダーで設定し、NVS `codex-ui` の `sleep_sec` へ保存する。0分0時間は無効（Desktop同期だけ）になる。ポーリング取得はないため取得間隔スライダーは付けない。
- Navigate画面はスクロール対応で、内容ドラッグで移動し、スライダーはドラッグで追従する。スクロールバーはエンコーダ域と重なるため判定を優先する。タッチ面の校正範囲は画面24..295のため、操作子はその範囲へ収める。
- 2026-09-23のコード追跡で、`TouchSampleFilter::push()` は安定点の確定時だけ `true` を返すため、それを毎ループの接触状態に使うとドラッグ条件が成立しないことを確認した。`DeviceUi::readTouch()` は `current()` の追従座標を毎回返し、`src/main.cpp` はその1回の読みをドラッグにも使う。接触終了はIRQで判定し、BLEキー解放にドラッグ専用の猶予を混ぜない（`test/ui-model-test.cpp`、`test/test_render_policy.py`）。実機での追従・描画確認は `verification.md` を参照。
- COM7実機でドラッグ追従は改善したが、`drawContent()` が中央領域を先に消してから描くため、スクロールとスライダーでちらついた（2026-09-23）。Navigate更新を小さな16-bit Spriteへ帯状に描いてから画面へ転送し、再書き込み後は所有者がちらつきなしと確認した。画面描画を変える場合は、`refresh()` と `drawNavigate()` の座標原点、ヘッダー・Bluetooth欄の描画順を同時に確認する（`src/device-ui.cpp`、`verification.md`）。

移動元: `AGENTS.md`。

<a id="agents-3"></a>

## タッチ調整（2026-09-14、実機未検証）

- 既存のBOOT長押し2点調整に、XPT2046の押圧閾値の実測・保存を追加した。`codex-touch` のversion 1の位置調整は閾値120で引き継ぎ、新しい位置・閾値は検証値付き単一`calib` blobへ保存する。校正中のみ閾値12で読み、保存失敗時は旧値へ戻す（`src/device-ui.cpp`、`lib/sensitive-xpt2046`）。抵抗膜に接触せずPENIRQが出ないペンは閾値変更だけでは認識できない。

移動元: `AGENTS.md`。

<a id="readme-1"></a>

## 検証済み環境

- ESP32-2432S028R / ESP32-D0WD-V3
- Windows 11 x64
- Codex Desktop `26.818.5229.0`
- Codex Desktop同梱 `@worklouder/device-kit-oai 0.2.1`
- BLEペアリング、Windows PnP列挙、`v.oai.rgbcfg`、`v.oai.thstatus`、`device.status` の往復

macOSでは同じBLEプロトコルのM5Stack Core2公開実装が検証されていますが、この基板と本ファームウェアの実機検証はWindowsで行っています。

移動元: `README.md`。

## 根拠と検証記録

- [README.md](README.md): 利用者向けの導入と操作。
- [verification.md](verification.md): 検証手順、実測結果、未確認事項。
- [how-to-update.md](how-to-update.md): 更新・復旧の実手順。
