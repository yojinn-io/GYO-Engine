# GYO：GitHub でバージョンを準備・公開する

[正體中文](releasing.zh-Hant.md) · [全体構成](architecture.md) · [描画の検証](rendering_architecture.ja.md#r09)

## 1. 日常開発と公開の入口

**GitHub Actions の `Prepare Release` を実行し、完全な検証と添付の準備が完了してから `Publish release` を押します。** 先に tag や Release を作る必要はありません。

| 操作 | 実行内容 | リモートの結果 |
|---|---|---|
| ブランチ push／pull request | Quick：3プラットフォームの engine／Editor baseline と CSV 選択 app の独立ビルド・検証 | Actions artifacts |
| 通常の cross-platform workflow を手動実行 | 同じ quick 検証 | Actions artifacts |
| **Prepare Release** を手動実行 | Baseline と選択 app × platform の完全な検証 | 成功後にバージョン tag、Draft Release、計算した添付集合を作成 |
| Tag push | 上記ビルドを起動しない | Release を自動作成しない |
| **Publish release** | 準備済みの Draft を公開 | 再ビルドしない |

両方の入口は `build-and-validate.yml` を共有し、quick と release は検証範囲が異なります。Tag はソースの commit を固定し、Release は説明とダウンロード添付を提供します。

`config/engine/projects.csv` はローカルと CI 共通の選択元です。各行の `enabled` とターゲット欄が両方有効な場合だけ app matrix に含めます。`description` と `version` は個人の記録であり、ここで入力する Release 版を決めません。App のない platform も engine と Editor をテストしますが、package は作りません。全 app が無効でも通常 CI は成功できますが、**Prepare Release** は空の成果物集合を事前検証で拒否します。

## 2. 初回利用の前に

`prepare-release.yml`、`build-and-validate.yml` と関連コードを先にリポジトリーのデフォルトブランチへマージすると、GitHub に **Run workflow** が表示されます。選ぶソースブランチにもこの workflow が必要です。通常の公開では `master` などのデフォルトブランチを選べます。

ソースブランチにさらに新しい workflow 定義の変更がある場合は、先にデフォルトブランチへマージしてからバージョンを準備します。GitHub が未知の workflow commit を含む tag／Release の作成に課す権限制限を避けるためです。

そのリポジトリーの Actions 実行と Release 編集ができるアカウントを使います。Build job は読み取り権限だけを持ち、最後の Draft job だけが `contents: write` を持ちます。すべての検証が成功してから実行できます。

## 3. GitHub で公開する手順

1. 公開するコードをソースブランチへ push し、日常の quick CI 結果を確認します。
2. リポジトリーの **Actions → Prepare Release → Run workflow** を開きます。
3. ブランチ選択欄でソースブランチを選びます。実行開始時の完全な commit SHA を固定するため、その後の新しい commit は今回の package に混ざりません。
4. **version** に `v1.0.1` などを入力します。候補版は `v1.1.0-rc.1` などを使い、プレリリースとして扱う場合は **prerelease** を選択して **Run workflow** を実行します。
5. 3プラットフォームの baseline と全選択 app × platform の完全な検証を待ちます。まず Actions artifacts を生成し、すべての必須検査に成功した後、最後の job が tag、Draft Release、添付を作成します。
6. 実行結果の **Summary** を開き、Draft Release のリンクを選びます。
7. バージョン、ソース commit、CSV から計算した全添付、プレリリース設定を確認し、タイトルと変更内容を編集します。
8. 確認後に **Publish release** を押します。既存の添付を公開し、完全なビルドをもう一度起動することはありません。

## 4. 完全な検証と添付

Release profile は3プラットフォームの engine と UI editor を必ずテストし、同じ source commit の CSV から app × platform matrix（Windows x64、Linux x64、macOS ARM64）を生成します。各組み合わせは独立したビルドディレクトリーでその app だけを選び、Editor を無効にして build、test、install、manifest 検証、package 化を実行します。Editor を app package に含めず、GPU／shader が不要な app は対応処理を実行しません。

各 app は必須の配布版 startup テストを登録します。共通 runner は package 外から、生成した `share/gyo/apps/<name>/manifest.json` のコマンドを profile／platform／GPU 条件に従って実行します。失敗、タイムアウト、必須証拠の欠落で package 化を停止します。Object_FPS が gameplay、内容欠落、Linux quick の GPU 1件と release の8件を所有します。[App 検証ガイド](../apps/object_fps/docs/acceptance.ja.md)を参照してください。

各組み合わせは archive と checksum を1つずつ持ち、**添付数は選択した組み合わせ数の2倍**です。

```text
gyo-<name>-<platform>.tar.gz
gyo-<name>-<platform>.tar.gz.sha256
```

Archive は `gyo-<name>` という単一ルートに、その app と必要な依存関係を含みます。CSV の `object_fps` はアンダースコアを保持し、`object-fps` に変換しません。Release は同じ source SHA の CSV から期待集合を再構築し、不足・余分・重複した package を拒否します。アップロード前に checksum、archive の安全性、必須内容、app、platform、source SHA、release profile、完全な検証証拠を照合します。Quick の証拠は release の代わりになりません。必須 job の失敗・キャンセル・スキップ時は Draft を作成しません。

Windows／macOS hosted CI は物理 GPU を検証せず、Linux Lavapipe はソフトウェア Vulkan です。各 app の手動実機結果を別に記録し、ビルド成功から推定しません。CSV、能力解決、manifest、CI のデータの流れは[設計文書](architecture.md#build-project-management)を参照してください。

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

以下はリファクタリング前の履歴であり、CSV matrix の hosted 検証を示しません。以前のフローでは3つの workflows の actionlint 1.7.12 と両言語ガイドの package 検査に合格しました。当時の自動復旧修正後、`tools/ci` の全80件に合格し、実際の Bash による gate の16状態組み合わせと、書き込み成功後に応答がタイムアウトする復旧ケースを含みます。GitHub API は mock を使い、リモート Release は作成していません。この結果はリモート API の復旧や hosted Draft 準備の成功を示すものではありません。同じ Actions 実行の再実行では artifacts を置き換えられますが、Release 添付は不足分だけを補い、上書きしません。
