# GYO：GitHub でバージョンを準備・公開する

[正體中文](releasing.zh-Hant.md) · [全体構成](architecture.md) · [描画の検証](rendering_architecture.ja.md#r09)

## 1. 日常開発と公開の入口

**GitHub Actions の `Prepare Release` を実行し、完全な検証と添付の準備が完了してから `Publish release` を押します。** 先に tag や Release を作る必要はありません。

| 操作 | 実行内容 | リモートの結果 |
|---|---|---|
| ブランチ push／pull request | Quick：3プラットフォームのビルドと内容読み込み、Linux の shader 描画1件 | Actions artifacts |
| 通常の cross-platform workflow を手動実行 | 同じ quick 検証 | Actions artifacts |
| **Prepare Release** を手動実行 | 3プラットフォームの完全な検証 | 成功後にバージョン tag、Draft Release、6件の添付を作成 |
| Tag push | 上記ビルドを起動しない | Release を自動作成しない |
| **Publish release** | 準備済みの Draft を公開 | 再ビルドしない |

両方の入口は `build-and-validate.yml` を共有し、quick と release は検証範囲が異なります。Tag はソースの commit を固定し、Release は説明とダウンロード添付を提供します。

## 2. 初回利用の前に

`prepare-release.yml`、`build-and-validate.yml` と関連コードを先にリポジトリーのデフォルトブランチへマージすると、GitHub に **Run workflow** が表示されます。選ぶソースブランチにもこの workflow が必要です。通常の公開では `master` などのデフォルトブランチを選べます。

ソースブランチにさらに新しい workflow 定義の変更がある場合は、先にデフォルトブランチへマージしてからバージョンを準備します。GitHub が未知の workflow commit を含む tag／Release の作成に課す権限制限を避けるためです。

そのリポジトリーの Actions 実行と Release 編集ができるアカウントを使います。Build job は読み取り権限だけを持ち、最後の Draft job だけが `contents: write` を持ちます。すべての検証が成功してから実行できます。

## 3. GitHub で公開する手順

1. 公開するコードをソースブランチへ push し、日常の quick CI 結果を確認します。
2. リポジトリーの **Actions → Prepare Release → Run workflow** を開きます。
3. ブランチ選択欄でソースブランチを選びます。実行開始時の完全な commit SHA を固定するため、その後の新しい commit は今回の package に混ざりません。
4. **version** に `v1.0.1` などを入力します。候補版は `v1.1.0-rc.1` などを使い、プレリリースとして扱う場合は **prerelease** を選択して **Run workflow** を実行します。
5. 3プラットフォームの完全な検証を待ちます。まず Actions artifacts を生成し、すべての必須検査に成功した後、最後の job が tag、Draft Release、添付を作成します。
6. 実行結果の **Summary** を開き、Draft Release のリンクを選びます。
7. バージョン、ソース commit、6件の添付、プレリリース設定を確認し、タイトルと変更内容を編集します。
8. 確認後に **Publish release** を押します。既存の添付を公開し、完全なビルドをもう一度起動することはありません。

## 4. 完全な検証と添付

Release profile は3プラットフォームのビルド、CPU/headless、shader 契約、core-only、独立配布とファイル欠落の失敗検査、配布版 startup／gameplay smoke、Linux Lavapipe の描画診断8件を含みます。必須 job が1件でも失敗、キャンセル、スキップした場合、Draft 作成へ進みません。

Draft には次の3アーカイブと各 `.sha256`、計6件の添付が必要です。

```text
gyo-object-fps-windows-x64.tar.gz
gyo-object-fps-windows-x64.tar.gz.sha256
gyo-object-fps-linux-x64.tar.gz
gyo-object-fps-linux-x64.tar.gz.sha256
gyo-object-fps-macos-arm64.tar.gz
gyo-object-fps-macos-arm64.tar.gz.sha256
```

アップロード前に checksum、package の必須内容、ソース commit、platform、`build_metadata.json` の release 検証記録を確認します。Windows／macOS の hosted CI は物理 GPU を検証せず、Linux の Lavapipe はソフトウェア Vulkan です。公開前の実機確認は[描画の検証ガイド](rendering_architecture.ja.md#r09)を参照してください。

## 5. 失敗、再実行、既存バージョン

Draft 準備で HTTP 500／502／503／504、ネットワークのタイムアウト、接続リセット、応答の途中切断が起きた場合は、初回を含め最大4回まで自動復旧を試みます。通常の待機は2／4／8秒です。サーバーが `Retry-After` を返した場合はその指示に従い、60秒を超える指定なら早めに再試行せず停止して報告します。権限、データ検証、バージョン競合は直ちに失敗します。

復旧時は毎回 tag、Draft、添付を読み直し、ソースと checksum を確認して不足分を判断します。直前の POST をそのまま再送しません。GitHub に未完了の `starter` 添付が残った場合も、この回数制限内で待機して再確認します。完了しなければ対応が必要な添付を報告し、自動削除や上書きはしません。

| 状況 | 対応 |
|---|---|
| ビルドやテストが失敗 | 修正して新しい実行を準備します。失敗した実行は tag／Draft を新規作成しません |
| 一時的なダウンロード／アップロード失敗で、同じソースを使いたい | 元の実行ページで **Re-run failed jobs** を選びます。元のイベント SHA は変わりません |
| 旧 publish 実行が504で失敗 | **Re-run failed jobs** はその実行の古い workflow を使い、今回の復旧修正は適用しません |
| 再実行時に以前の artifacts が期限切れ | 元の実行で **Re-run all jobs** を選び、同じイベント SHA で作り直します |
| Draft ができたが添付が一部しかない | 元の実行を再実行します。既存添付を検証して不足分だけを追加し、手書きのタイトルと説明を保持します |
| 同名 tag が存在 | 今回と完全に同じ commit を指す場合だけ許可し、既存 tag は移動しません |
| Draft はあるが、その tag が削除された | ソースを推測せず拒否します。確認済みの元の tag を復元するか、新しいバージョン番号を使います |
| 同じバージョンが公開済み | このフローは変更を拒否します。新しいバージョン番号を使います |
| ブランチ更新後に新しく **Run workflow** を実行 | 新しい SHA を選びます。同じバージョン番号で以前の commit を置き換えることはできません |

今回の修正を使うには、先にマージして新しい commit から **Prepare Release** を実行します。元のバージョン tag が別の SHA を指している場合は、新しいバージョン番号を入力してください。番号を再利用するために元の tag を移動・削除しません。

同じバージョンを同時には実行せず、実行中の処理を新しい要求でキャンセルしません。GitHub は待機中の要求の順序を保証せず、後の要求が未開始の要求を置き換える場合があります。既存添付の内容やソース検証が一致しない場合はエラーとし、上書きしません。`prerelease` 入力は新規 Draft にだけ適用します。既存 Draft のプレリリース設定、タイトル、説明は保持するため、Draft ページで確認して編集できます。

## 6. 範囲と検証状況

新フローは既存の Actions 実行を書き換えず、古い commit の workflow も変更しません。古い tag や実行の **Re-run** は当時の定義を使うため、以前の tag／`release.published` トリガーが残る場合があります。

今回の実装とローカルテストは、GitHub で Release を作成・公開した証拠にはなりません。各バージョンの検証証拠は、実際の **Prepare Release** の Summary、3プラットフォームの結果、Draft の添付で確認してください。

以前のフローでは3つの workflows の actionlint 1.7.12 と両言語ガイドの package 検査に合格しました。今回の自動復旧修正後、`tools/ci` の全80件に合格し、実際の Bash による gate の16状態組み合わせと、書き込み成功後に応答がタイムアウトする復旧ケースを含みます。GitHub API は mock を使い、リモート Release は作成していません。この結果はリモート API の復旧や hosted Draft 準備の成功を示すものではありません。同じ Actions 実行の再実行では artifacts を置き換えられますが、Release 添付は不足分だけを補い、上書きしません。
