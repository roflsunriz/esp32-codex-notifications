# 更新手順

## 前提

- 対象基板が ESP32-2432S028R（ILI9341 / XPT2046）であること。
- PlatformIO CoreまたはPlatformIO IDEが利用できること。
- 更新前のファームウェアと、必要ならタッチ調整値を再設定できること。

## 依存関係またはプロトコルを更新する場合

1. OpenAIの [Codex Micro公式文書](https://learn.chatgpt.com/docs/features/codex-micro) で操作仕様と状態の意味を確認します。
2. `docs/protocol.md` に列挙した公開実装の最新コードを確認し、観測事実と推測を分けます。
3. `platformio.ini` のESP32 platform、ArduinoJson、TFT_eSPIを更新します。XPT2046は `lib/sensitive-xpt2046` のローカル実装と上流v1.4の差分を確認します。
4. lockfile相当の `.pio` 解決結果だけに依存せず、版を `platformio.ini` に固定します。
   CI用Python依存は `requirements-ci.txt` へ固定し、`docs/security.md` の限定除外と到達性を再確認します。
5. 次の検証を実行します。

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
pio run -e cyd
```

6. 実機へ書き込み、Bluetoothの既存ペアリングを削除して再ペアリングします。
7. `docs/protocol.md` の互換性境界8項目を実機確認します。
8. 物理BOOTボタンを短く押し、表示、タッチ位置、再起動後の向きが一致することを確認します。1.5秒以上の長押しで2点の位置・押圧感度調整が起動し、再起動後も保持されることを確認します。
9. Codex MicroのBrightnessを100%、50%、10%、0%へ変更し、画面が段階的に暗くなることと、シリアルログの `UI backlight duty` が255、128、26、0になることを確認します。確認後は元の値へ戻します。
10. Codex MicroのAuto-dimを30秒へ設定し、画面とバックライトが消灯すること、最初のタッチが復帰だけに使われること、次のタッチが通常操作になることを確認します。Auto-dimをOffへ戻した場合に自動消灯しないことも確認します。
11. Codex MicroとのBLE接続を切り、切断表示から30秒後に画面が消灯することを確認します。30秒以内の再接続で消灯が取り消されること、切断中のタッチ復帰後は30秒で再消灯すること、消灯中の再接続で自動復帰することも確認します。
12. Navigateタブの空き領域を上下にドラッグしてスクロールし、分・時間のスライダーを左右にドラッグして値の追従と離した後の確定表示を確認します。ヘッダーとタブのちらつきや、操作中の再起動がないことも確認します。
13. 結果と意図を `CHANGELOG.md` の `Unreleased` へ記載します。

## 通常の書き込み

```powershell
pio run -e cyd --target upload
pio device monitor
```

`CODEX_CYD_READY` が表示され、ChatGPT DesktopのCodex Micro設定にデバイスが現れることを確認します。

## リリース

1. `src/codex-micro-ble.cpp` のファームウェアバージョン、`CHANGELOG.md`、`docs/releases/vX.Y.Z.md` を同じバージョンへ更新します。
2. CIが成功したmainのコミットへ注釈付きタグ `vX.Y.Z` を付けます。
3. タグをpushすると `Release firmware` workflowがテスト、静的解析、ビルド、mergedイメージ生成、SHA-256生成、provenance attestation、該当版のCHANGELOG節を含む本文生成、GitHub Release公開を順番に行います。
4. workflow成功後、Releaseのasset名、サイズ、`SHA256SUMS.txt`、attestationを確認します。

ローカルでReleaseと同じ配布物を作る場合は次を実行します。

```powershell
pio run -e cyd
python .\scripts\package_release.py --version vX.Y.Z
```

## 復旧

- 画面やBLEが起動しない場合は、最後に正常だったGitコミットへ戻して再ビルド・再書き込みします。
- タッチだけがずれる場合は、起動後に`BOOT`を1.5秒以上押して離して2点調整をやり直します。
- ChatGPT Desktopが検出しない場合は、OSのBluetooth設定から `Codex Micro` を削除して再ペアリングします。
- 書き込みできない場合は `BOOT` を押しながらリセットし、PlatformIOのuploadを再実行します。
