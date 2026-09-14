# 検証手順

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
