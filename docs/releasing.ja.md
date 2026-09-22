# GYO：バージョンの準備と公開

[繁體中文](releasing.zh-Hant.md) · [全体構造](architecture.md) · [描画の検証](rendering_architecture.ja.md#r09)

GitHub Actions の **Prepare Release** で必要な製品を検証し、Draft の添付と説明を確認してから **Publish release** を押します。公開単位は Engine 統合であり、ゲームの有無で公開可否を決めません。

## 1. 製品集合

各対応 platform は toolchain 製品を必ず生成し、その内容は `engine/config/tools.csv` の enabled、release、platform フラグで選びます。現在は UI Editor の GUI 既定 variant と必要な runtime libraries です。公開ツールには packageable、ゲーム非依存、対象 platform の quick／release 両方の実行検証を要求し、空集合や検証不足は失敗です。ローカル CLI と Object_FPS preview は含めません。`engine/config/projects.csv` は追加ゲームを選びます。両 registry は CMake の共通 CSV parser から選択結果を export し、description/version は備考として扱います。

| 製品 | Archive | Archive root |
|---|---|---|
| 固定の設計ツール群 | `gyo-toolchain-<platform>.tar.gz` | `gyo-toolchain` |
| CSV が選んだゲーム | `gyo-<game>-<platform>.tar.gz` | `gyo-<game>` |

Platform は `windows-x64`、`linux-x64`、`macos-arm64` です。Archive ごとに同名の `.sha256` を付けます。Apps がない場合、CSV が header だけの場合、全ゲーム無効の場合も、3 platform の toolchain と checksum を公開できます。未登録ディレクトリは選びません。選択したゲームの欠落、compile failure、必要な検証の失敗は統合全体を失敗させます。

配布物は実行可能な製品と必要な依存物です。Engine SDK や source archive は生成しません。Toolchain にゲーム source/assets を含めず、ゲーム包に CI、tests、acceptance executable、editor、元の美術ソースを含めません。エンジンは静的リンクし、必要な third-party dynamic runtime を配置します。

## 2. 入口と操作

| 操作 | 結果 |
|---|---|
| Branch push／pull request／通常 workflow の手動実行 | Quick 統合と Actions artifacts |
| **Prepare Release** | SHA を固定し、全必須製品の完全検証後に tag、Draft、添付を準備 |
| Tag push | 自動公開の入口にはしない |
| **Publish release** | 既存 Draft を公開し、再コンパイルしない |

初回は workflow と必要なコードを既定 branch に入れ、GitHub に **Run workflow** を表示させます。選択する source branch にも同じ支援が必要です。Actions 実行／Release 編集権限を用い、build jobs は read-only、最後の Draft job だけ write 権限を持ちます。

1. 公開予定の source を push し、Quick の結果を確認します。
2. **Actions → Prepare Release → Run workflow** で source branch を選びます。
3. `v1.0.1` などの version を入力し、必要なら prerelease を選びます。
4. 3 platform の固定 toolchain と全 CSV ゲームの必要検証を待ちます。
5. Summary から Draft を開き、commit、version、全添付、説明を確認します。
6. 公開する時に **Publish release** を押します。

最初に完全な source SHA を固定するため、後続の branch 更新は混入しません。必須 job の失敗、取消、意図しない skip は Draft 準備を止めます。一部 platform の成功を統合成功としません。

## 3. 組立と検証

本機と CI は同じ製品組立を使います。`build/assemble_runtime.py` は `assets/<game>/content.json` に従って準備済み資産を検証・配置し、generic CMake hook から自動呼出しされます。Offline shader compiler は `engine/render/shaders/pipeline` が所有します。通常のゲーム build に CI/tests や package acceptance は不要です。

組立と package 検証は `build/content_contract.py` を共有し、C++ runtime と Editor の native parser は共通 fixtures で検証します。Catalog には整数の `version: 1`、有効な entry、内容集合全体で一意な ID が必要です。資産ディレクトリがなければ空内容として同期します。ディレクトリが存在して descriptor が欠落・不正なら失敗し、直前の成功出力を保持します。Object_FPS は実際に使う builtin shaders のみを配布し、カスタム shader の検証サンプルは共通 GPU テストが所有します。

生成される product manifest は `share/gyo/products/<product>/manifest.json` です。Schema 3 は空でない build configuration を要求し、context の configuration も一致させます。Executable は `owner.role` で登録し、各 entry が owner、role、path、runtime_dependencies を持ちます。ほかに required_files、native_files、owner ごとの checks を記録します。依存物の和集合はコピー対象の決定だけに使い、linkage は実行ファイルごとの要求で検査するため、SDL GUI と SDL 不要の CLI を同じ package に配置できます。共通 runner は `build/acceptance/common/run_package_checks.py`、native linkage 検査は同じ場所の `validate_package.py` です。専用規則は `build/acceptance/<owner>` に置きます。

診断 executable は owner が role を明示登録します。CMake が生成する `<build>/packages/<product>/<configuration>/acceptance-context.json` を `--context` で runner に渡します。Context は manifest と build configuration に結び付き、`@CHECK_ROOT@`、`@PROBE:<role>@` の外部位置だけを解決し、検証コマンドや集合を変えません。`@EXECUTABLE:<role>@` は owner ごとの製品を参照し、検証・ログ名は `owner.name` です。Runner は製品の一時コピーへ probe を置けますが正式実行ファイルを上書きしません。製品には context、probe、Python 検証コードを含めません。Runtime は `bin/assets/<game>` だけを読みます。

Quick と Release の範囲や GPU suite は owner の checks が決めます。Linux toolchain job はゲームがなくても Xvfb／Lavapipe で共通エンジンの GPU 描画テストを実行します。Toolchain を含む全製品は、自身の契約に宣言された GPU checks も独立に実行し、共通 baseline との排他的な分岐にしません。必要能力の欠落、timeout、異常は失敗です。Software Vulkan と Windows/macOS hosted の結果は実物 GPU の検証ではありません。[Object_FPS の外部検証](object_fps/acceptance.ja.md)も参照してください。

封装と Draft 準備は同じ source SHA から完全な期待集合を再計算します。Product/platform、tool owner 集合、checksum、archive path の安全性、manifest、必要内容、Release profile の証拠を確認します。欠落、余分な製品、一部のツールだけを含む toolchain、重複 identity、Quick-only 証拠を拒否します。

`acceptance.json` の `package_sha256` は製品内の全ファイル内容とシンボリックリンク先を結び付け、封装時に生成する root の `build_metadata.json` だけを除外します。CPU／GPU 検証と封装の間の内容変更を拒否し、archive 検査でも同じ digest を独立に再計算します。製品隔離は登録済み実行ファイル、native files、コンテンツの一覧に従い、接頭辞や拡張子によらず未登録ファイルを拒否します。シンボリックリンクは宣言済み native files に限り、参照先を package 内に制限します。

## 4. 失敗と再実行

一時的な HTTP 500／502／503／504、timeout、connection reset、途中切断は初回を含め最大4回復旧します。通常の待機は2／4／8秒で、`Retry-After` が60秒を超える時は停止して報告します。権限、データ、version conflict は直ちに失敗します。再試行ごとに tag／Draft／添付を読み直し、source と checksum を確認して不足だけを補います。

- 一時的な失敗は **Re-run failed jobs** で同じ実行の SHA を保持します。
- Artifacts の期限切れは元の実行の **Re-run all jobs** を使います。
- 一部だけ添付された Draft は既存添付を検証して不足分を追加し、手書きの説明を保持します。
- 同名 tag は同じ commit の場合だけ許可し、tag 移動や不一致の添付上書きをしません。
- 公開済み version は変更しません。Source commit が変わる時は新しい version を使います。
- 未完了の `starter` 添付は有限回再確認し、解決しなければ自動削除せず報告します。

古い実行の再実行は古い workflow を使い、新しい修正を取り込みません。同 version は並行実行せず、新要求で実行中の公開を取消しません。既存 Draft の prerelease、title、description は保持します。

## 5. 証拠の扱い

本機 tests や workflow 静的検査は、遠隔3 platform／Draft 成功の証拠ではありません。公開 revision の Actions Summary、必須 jobs、検証報告、添付を確認してください。Historical dev logs は当時のパスと結果の記録であり、今回の構造変更の成功を意味しません。
