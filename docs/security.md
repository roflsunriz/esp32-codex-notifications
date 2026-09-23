# セキュリティ監査

更新日: 2026-09-23

## Releaseビルド環境

CIとRelease workflowはGitHub-hosted runner上で次だけを実行します。

- `pio run -e cyd`
- `pio check -e cyd --skip-packages`
- CMakeネイティブテスト
- PythonパッケージテストとRelease asset生成
- GitHub CLIによるdraft Releaseの作成と公開

PlatformIO Home、ASGI server、HTTP endpointは起動せず、外部からHTTP requestを受けるportも開きません。

## pip-auditの限定除外

PlatformIO 6.1.19はWeb UI用にStarlette `<0.53` を要求します。Starlette 0.52.1には次の既知脆弱性があります。

| ID | 対象機能 | workflowからの到達性 |
| --- | --- | --- |
| `PYSEC-2026-161` | HTTP Host headerからのURL再構築 | HTTP serverを起動しないため到達不能 |
| `PYSEC-2026-248` | 不正なHTTP request pathのURL再構築 | HTTP serverを起動しないため到達不能 |
| `PYSEC-2026-249` | URL encoded formのresource limit | form parserを呼び出さないため到達不能 |
| `PYSEC-2026-2280` | `HTTPEndpoint`のmethod dispatch | endpointを登録しないため到達不能 |
| `PYSEC-2026-2281` | Windows `StaticFiles`のUNC path処理 | Linux runnerでStaticFilesを使わないため到達不能 |

workflowの `pip-audit` は上記IDだけを明示的に除外し、それ以外の脆弱性では失敗します。pipは `26.2` へ更新し、`PYSEC-2026-3721`を解消します。

次のいずれかを変更する場合、この除外を撤回して再評価します。

- PlatformIO Home、remote agent、Web UIをworkflowで起動する。
- Starlette、Uvicorn、Bottle、ASGI appを直接importする。
- runnerを外部HTTP requestを受ける構成にする。
- PlatformIOがStarlette修正版へ対応する。

## ファームウェア依存

ファームウェアへPythonパッケージは含まれません。ArduinoJson、TFT_eSPI、Arduino-ESP32、ローカルXPT2046実装は、ビルド・静的解析に加えて公開advisoryを確認します。Wi-Fi、HTTP server、OTA update endpointはファームウェアで有効化していません。

Arduino-ESP32 2.0.17が該当する次のadvisoryは、対象機能をリンクも初期化もしていないため製品経路から到達不能です。

| Advisory | 影響を受ける機能 | このファームウェア |
| --- | --- | --- |
| `GHSA-9vfw-wx65-c872` | `HTTPUpdateServer` / OTA Web updaterのCSRF | Wi-Fi、WebServer、OTAを使用しない |
| `GHSA-8cmm-3887-r32j` | `WebServer` multipart upload parser | WebServerとupload handlerを使用しない |

2026-09-23に[Arduino-ESP32の公式advisory一覧](https://github.com/espressif/arduino-esp32/security/advisories)を再確認した。追加のWebServer系（`GHSA-vqp8-ppw3-8mc7`、`GHSA-w887-pg2w-mfj2`、`GHSA-28hv-fwm3-rpcq`、`GHSA-5476-9jjq-563m`）、[NetBIOS](https://github.com/espressif/arduino-esp32/security/advisories/GHSA-92j9-c75g-2c5f)、OTAの公式サンプル、上流リポジトリのCI workflowに関する報告がある。本プロジェクトの`src/`、`include/`、`lib/`にはWi-Fi、WebServer、NetBIOS、OTAの利用がなく、PlatformIOの依存グラフにもこれらのライブラリはない。上流のDangerJS・Wokwi workflowも使用していない。これらを導入する際はadvisoryごとに影響版と修正版を再確認する。

Wi-Fi、WebServer、HTTP update、OTAのいずれかを追加する場合は、Arduino-ESP32 3.3.8以降へ移行し、BLE HID、TFT、タッチの実機回帰を完了するまでReleaseしてはいけません。
