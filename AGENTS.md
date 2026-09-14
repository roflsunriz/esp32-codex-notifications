# AGENTS.md

## 作業開始前の必須手順（最優先・例外なし）

1. エージェントは、調査、計画、コマンド実行、スキル利用、ファイル編集、コミット、プッシュを始める前に、必ずリポジトリ直下の `.\COMMON-AGENTS.md` を開き、先頭から末尾まで全文を読む。
2. `COMMON-AGENTS.md` はGit管理外のシンボリックリンクである。`git`や既定のignore設定が有効な`rg --files`の検索結果だけで、ファイルが存在しないと判断してはならない。PowerShellでは最初に次を実行する。

```powershell
Get-Content -Raw -LiteralPath .\COMMON-AGENTS.md
```

3. 読み取りに失敗した場合、出力が省略された場合、または末尾まで読めたことを確認できない場合は、一切の作業を開始せず、パスとシンボリックリンク先を確認して全文を再取得する。必要なら分割して末尾まで読む。
4. 全文を読了するまで、ローカル `AGENTS.md` だけを根拠に作業を続けてはならない。読了後は `COMMON-AGENTS.md` を最優先の指針とし、読了直後の最初の進捗報告で全文を読了したことを明示する。
   このファイルでは `esp32-codex-notifications` 固有の補足だけを記載する。

## 目的
- ChatGPT (Codex) Desktop App の通知類や操作を簡便にするため、esp32-2432s028r ili9341 esp-wroom-32 tft lcd 開発ボード に操作ボタンや通知などを表示する。
- Github上の他公開リポジトリで既に Work Louder Codex Micro / Creators Micro 2 と ChatGPT (Codex) Desktop App の公式連携の非公開APIを解析した結果があるのでそれを事前に調べてその結果を元に esp32 に実装する。

## Environment
<必要に応じて適宜書き足すこと。>

## タッチ調整（2026-09-14、実機未検証）

- 既存のBOOT長押し2点調整に、XPT2046の押圧閾値の実測・保存を追加した。`codex-touch` のversion 1の位置調整は閾値120で引き継ぎ、新しい位置・閾値は検証値付き単一`calib` blobへ保存する。校正中のみ閾値12で読み、保存失敗時は旧値へ戻す（`src/device-ui.cpp`、`lib/sensitive-xpt2046`）。抵抗膜に接触せずPENIRQが出ないペンは閾値変更だけでは認識できない。
