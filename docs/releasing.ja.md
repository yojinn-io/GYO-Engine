# GYO：CI、配布チャネル、リリース手順

[繁體中文](releasing.zh-Hant.md) · [全体構造](architecture.md) · [描画の検証](rendering_architecture.ja.md#r09)

この文書は現在の CI とリリースの流れを説明します。記述の根拠は `.github/workflows/*.yml`、`.github/actions/native-setup`、`build/ci/common/*.py`、`CMakePresets.json`、`build/acceptance/*/checks.json`、`engine/config/projects.csv`、`engine/config/tools.csv` です。食い違いを見つけた場合はコードが正であり、この文書を直します。

- 1〜9 節：仕組みと規則
- 10 節：実例（そのまま実行できるコマンド付き）
- 11〜13 節：製品検証の詳細、失敗と再実行、今後の課題

## 1. 全体像

検証は 4 段階です。L1 は merge を、master 完全実行は snapshot を止めます。Release の前提（完全実行が成功した master commit）は運用規則 4 で守るもので、workflow は検査しません。L4 の証拠参照は Release の入力として、build より前に必要です。

| 段階 | いつ実行するか | 内容 | 止めるもの |
|---|---|---|---|
| **L1**（merge gate） | Ready の pull request、master への push、Cross-platform CI の手動実行 | Platform ごとに 1 job。`ci-<os>` preset で registry 有効ゲーム、既定ツール、Engine、tests を build し、`cpu`／`shader` label の tests を実行します。Linux 行はさらに host shader tests、Lavapipe による共通 GPU tests、core baseline、Go の vet／test／race、Go service の cross build を実行します。封装はしません | PR の merge（`CI gate`） |
| **master 完全実行**（L1 + Quick） | master への push、Cross-platform CI の手動実行 | L1 に加え、`build-and-validate.yml` を `profile: quick` で全製品に実行します。Toolchain 4 行とゲームの各 platform 行が封装、install 後の Quick 検証、archive 化を行い、Actions artifacts に upload します | Snapshot の公開（全 train） |
| **Release** | **Prepare Release**（手動） | 選んだ train について、toolchain baseline 4 行とその train の行を `profile: release` で build・検証します。全行で tests、host shader tests、Release checks を実行し、Linux 行で Go checks を実行します | Tag と Draft の作成 |
| **L4**（実機） | 人が実機で実行 | ゲームが `release_evidence` で宣言した項目（実機 GPU、実ディスプレイなど）。CI は実行しません | ゲーム train の Prepare Release（証拠参照がなければ build 前に失敗）と、公開者の確認 |

```text
feature branch ──PR──> L1 ──CI gate──> merge ──> master 完全実行 ──CI gate──> snapshot（train ごと）
                                                       │
                                     完全実行が成功した master commit
                                                       │
                                        Prepare Release（1 train）
                                        Release 検証 ─> tag + Draft ─> 人が Publish release
```

- master 完全実行では、どの製品のどの行が失敗しても `CI gate` が失敗し、その commit の snapshot は **全 train で** 公開されません（snapshot job は `CI gate` と Quick の両方の成功を前提にします）。
- Prepare Release は train ごとに独立します。ゲーム train は toolchain baseline とそのゲームの行だけを build するので、別のゲームの失敗では止まりません。Toolchain baseline の失敗はすべての train を止めます。
- 公開（**Publish release**）は常に人の操作です。Tag の push を公開の入口にはしません。

## 2. 製品と owner

製品は registry データから導出します。Workflow と共通コードに製品名は書きません。

| 製品 | Archive（各 `.sha256` 付き） | Platform | Owner |
|---|---|---|---|
| 設計ツール群（toolchain） | `gyo-toolchain-<platform>.tar.gz`（root `gyo-toolchain`） | `windows-x64`、`linux-x64`、`macos-arm64`、`macos-x64` の 4 つを常に生成 | `engine/config/tools.csv` で `release` が有効なツール（現在は UI Editor の GUI 既定 variant）と必要な runtime libraries |
| ゲーム | `gyo-<game>-<platform>.tar.gz`（root `gyo-<game>`）。`object_fps_pvp` は client（`main`）と `match` を含む | `projects.csv` でそのゲームの `enabled` と platform 列が有効なもの。`macos` 列は `macos-arm64` と `macos-x64` の両方を有効にします | `apps/<game>`、`assets/<game>`、`build/acceptance/<game>`、`projects.csv` の行 |
| ゲームの Go service | `gyo-<game>-<role>-<platform>.tar.gz`。`object_fps_pvp` は `gyo-object_fps_pvp-gateway-linux-x64.tar.gz` と `...-windows-x64.tar.gz` | `gyo_app_add_go_service(... PLATFORMS ...)` で宣言した platform だけ（gateway は Linux と Windows） | そのゲーム（`apps/<game>/go.mod`） |

- 現在の registry で有効なゲームは `object_fps_pvp` だけです（`object_fps`、`object_fps_v2` は `enabled=0`）。`tools.csv` で `release=true` のツールは `ui_editor` だけで、`object_fps_preview` はゲームに依存するローカルツールなので配布しません。
- `services/gyo_gateway` は Engine 層の Go module（owner `engine`）です。HTTP、session、framing の再利用可能な仕組みを提供し、どのゲームにも依存しません。CMake は全構成でこれを記録するため、L1 の Linux 行と toolchain 行がゲームなしでも vet／test／race を実行します。単独の archive にはしません。
- Train は registry から導出します。`tools` train（toolchain 4 platform）と、有効なゲームごとに 1 train（例：`object_fps_pvp`）です。ゲーム train にはそのゲームの native package と、Linux 封装行が `go-services.json` に記録した service archive が入ります。同じ release の client、match、gateway は同じ commit と version から作ります。`tools`、`toolchain` はゲーム id として使えません。
- 配布物は実行可能な製品と必要な依存物です。Engine SDK や source archive は作りません。Toolchain にゲームの source／assets は入らず、ゲーム包に CI、tests、acceptance executable、editor、元の美術ソースは入りません。Engine は各製品に静的リンクし、必要な third-party dynamic runtime を配置します。
- ゲームが 0 件でも（CSV が header だけ、全ゲーム無効でも）toolchain の 4 platform を公開できます。選択されたゲームの欠落、compile 失敗、必須検証の失敗は、その実行全体を失敗させます。

## 3. Trigger と実行内容

| 操作 | Workflow | 実行されるもの | 表示される check |
|---|---|---|---|
| Feature branch への push | なし | CI は動きません（Cross-platform CI の push trigger は master だけ） | — |
| Draft PR の作成・push | Cross-platform CI、Package trial | `Select CI scope`（CI policy tests と範囲判定、理由 `draft`）。L1／Quick は skip、`CI gate` は意図した skip として成功。Package trial は label がなければ build しません | `Select CI scope`、`CI gate`、`Select trial packaging`（skip された job も表示） |
| `docs/` 配下だけを変える PR | 同上 | 理由 `docs-only`。L1 を skip し、`CI gate` は成功 | 同上 |
| コードを変えた Ready PR に `docs/` 配下だけを変える commit を push | 同上 | 理由 `docs-increment`。より前の commit で成功した L1 を再利用して L1 を skip し、`CI gate` は成功。条件は下記。満たさない場合は理由 `pull-request` で通常どおり L1 を実行 | 同上 |
| Ready の PR（作成、push、Ready への変更、reopen） | 同上 | 理由 `pull-request`。`L1 / <platform>` を 4 行実行。Quick と封装はしません | `Select CI scope`、`L1 / windows-x64`、`L1 / linux-x64`、`L1 / macos-arm64`、`L1 / macos-x64`、`Quick acceptance`（skip）、`CI gate`、`Select trial packaging`（open／push／reopen 時） |
| Open な PR に `package` label | Package trial | Quick 封装（全製品）を Actions artifacts として upload。Label がある間は push のたびに再 build | `Trial packages / ...`（required ではない） |
| **Package trial** の手動実行 | Package trial | 選んだ branch で同じ Quick 封装 | — |
| master への push（merge） | Cross-platform CI | L1 + Quick + `CI gate`。成功すれば `Snapshot / <train>` が train ごとに snapshot を公開。L1 が compiler cache を保存 | `L1 / ...`、`Quick acceptance / ...`、`CI gate`、`Plan snapshot trains`、`Snapshot / tools`、`Snapshot / object_fps_pvp` |
| **Cross-platform CI** の手動実行 | Cross-platform CI | L1 + Quick + `CI gate`（理由 `integration`）。Snapshot は公開しません。master で実行した場合だけ compiler cache を保存します | 同上（snapshot なし） |
| **Prepare Release** | Prepare Release | 1 train の Release 検証、tag、Draft | `Validate train, version and capture source commit`、`Full release acceptance / ...`、`Prepare verified draft and downloads` |
| Tag の push、**Publish release** | なし | 何も build しません。Publish は既存 Draft を公開するだけです | — |

- 範囲判定（`build/ci/common/ci_scope.py`）は job の中で行い、trigger の path／draft filter は使いません。そのため `CI gate` はすべての PR event で報告されます。
- `docs-only` の判定対象は `docs/` で始まる path だけです。Root の `README*.md`、`AGENTS.md` などを変えた PR は L1 を実行します。Rename は分割して判定するため、`docs/` の外から `docs/` へ移したファイルも非文書の変更として扱います。
- `docs-increment`：PR head の 1 つ前の commit から first-parent をたどり（最大 20 個）、次の条件をすべて満たす最初の commit の L1 を再利用します。
  - 4 行の `L1 / <platform>` の最新 check run がすべて GitHub Actions の `success`。skip、cancel、失敗、未完了は数えません。
  - その commit から head までの変更が `docs/` 配下だけ（判定方法は `docs-only` と同じ）。
  - 現在の master の先端がその commit の祖先。master は前進するだけなので、その commit を検証した時の merge 結果はその commit 自身であり、現在の merge 結果は head です。両者のコードは完全に同じです。
  - master が進んだ、途中にコード変更がある、コードの push の L1 が新しい push で cancel された、check run を読めなかった、のいずれかなら通常どおり L1 を実行します。最悪でも 1 回多く実行するだけで、未検証のコードを skip することはありません。
  - `Select CI scope` の Summary に、再利用した commit または再利用しなかった理由が表示されます。
  - Check run の読み取りに `checks: read`、履歴をたどるために完全な履歴（checkout の `fetch-depth: 0`）が必要です。master の履歴を書き換えない（force push しない）ことが前提です。
- Draft の判定には実行開始時に読む PR の現在の状態を使います。古い Draft 実行を re-run した場合、PR がすでに Ready なら L1 を実行し、まだ Draft なら再び skip します。
- 同じ PR への新しい push は古い実行を取り消します。master の実行は互いに取り消しません。
- Base branch の変更（`edited`）では再実行しません。Merge queue（`merge_group`）は未対応で、有効化する前にその trigger と範囲判定を追加します。
- Package trial は close／merge 済みの PR では build せず、`package` 以外の label を追加しても実行中の trial を取り消しません。

## 4. `CI gate`：唯一の required check

Branch protection（または ruleset）で master の required status check に **`CI gate` だけ**を設定することを推奨します。設定は GitHub の Settings → Branches（または Rules → Rulesets）で行い、リポジトリのファイルには含まれません。

- `CI gate` は常に実行され、`Select CI scope` が成功し、選ばれた段階（L1、Quick）がすべて `success`、選ばれなかった段階が `skipped` の場合だけ成功します。失敗、取消、意図しない skip は失敗です。
- `L1 / <platform>` や Quick の各行を個別に required にしないでください。Draft、docs-only、docs-increment では skip されるため、個別に指定すると merge できなくなります。
- Package trial の check は required にしません。

## 5. Platform と検証水準

| Platform | Runner／host | Preset／compiler | CPU tests の実行 | GPU | 備考 |
|---|---|---|---|---|---|
| `linux-x64` | `ubuntu-24.04` | `ci-linux`、gcc-14 | native | Xvfb + Mesa Lavapipe（software Vulkan）で共通 GPU tests と製品の GPU checks | L1 の host shader tests、core baseline、Go の vet／test／race、Go service の cross build はこの行だけ |
| `windows-x64` | `windows-2025` | `ci-windows`、MSVC（`msvc-vs2026`） | native | なし | 封装 jobs の toolchain 行は core baseline も実行し、Release では独立コピー検証（`app_copy_integration.py`）も実行 |
| `macos-arm64` | `macos-15`（arm64） | `ci-macos`、Xcode 16.4 の AppleClang、deployment target 13.3 | native | なし | 封装 jobs の toolchain 行は core baseline も実行 |
| `macos-x64` | `macos-15`（arm64 host で cross build） | `ci-macos-x64`（`CMAKE_OSX_ARCHITECTURES=x86_64`、deployment target 13.3） | Rosetta 2 による変換実行 | なし | 実機の Intel Mac による検証なし。Runner で Rosetta 2 が使えなければ build 前に失敗。Core baseline は host 行が担当 |

- 実物の GPU での検証は CI にありません。Software Vulkan と Windows／macOS hosted runner の結果は、実機 GPU での動作を意味しません。ゲームが必要とする実機確認は L4（8 節）で扱います。
- 封装 jobs の macOS 行は、shader bundle を使う構成で offline Metal compile が使えることを確認します。
- Go service は Linux 行で `CGO_ENABLED=0` により各 platform 向けに cross build します。CI は service の実行ファイルを起動しません。
- Linkage 検査は macOS package 内の実行ファイルと native library が target architecture だけを含むことを要求します。
- 各行の Summary は build host／target と CPU 実行方式を表示し、archive の `build_metadata.json` も `cpu_execution`（`native` または `rosetta2`）を記録します。Snapshot と Draft の説明には、archive ごとの検証水準（native／cross build、実行した checks の数、Rosetta 2、GPU checks の有無、実機 GPU なし）が自動で入ります。

## 6. Cache と初回の挙動

- **Compiler cache（sccache v0.18.0）**：`native-setup` が全 native job の compile を sccache 経由にします。Cache の key は platform、toolchain、compiler の version、preset ごとに分かれ、`macos-x64` の target object が arm64 と混ざることはありません。**保存するのは master 上の L1 だけ**（master への push、または master での手動実行）です。PR の L1、Quick、trial、Prepare Release は最新の seed を読むだけです。
- **Host tools**：Shader compiler などの host tools は runner の platform（host）ごとに `actions/cache` で保存します。`macos-arm64` と `macos-x64` は同じ arm64 native tools を共有し、target architecture を host tools の build に持ち込みません。
- **Go**：`actions/setup-go` が各 module の `go.mod`／`go.sum` を key に module cache を保存します。

初回や toolchain 更新後に予想されること：

- master の L1 が一度も完了していない間、または compiler／Xcode／preset が変わった後は seed がなく、PR の L1 や封装 jobs はすべてを compile し直します。macOS 行は並列数 2 で最も遅くなります。各 native job の上限は 120 分です。
- 新しい toolchain の seed は、次の master push の L1 が保存して以降に効きます。PR だけを何度実行しても seed は作られません。
- `macos-x64` 行は Rosetta 2 がない runner では install を試み、使えなければ build 前に明示的なエラーで失敗します。Pinned の Xcode 16.4 が runner image にない場合も同様に失敗します。
- **Run workflow**（手動実行）は workflow が既定 branch にある場合だけ表示されます。新しい workflow を追加する PR では、merge 後に初めて手動実行できます。選んだ source branch にも同じ workflow と支援コードが必要です。
- `package` label がリポジトリになければ、最初に一度作成します（10.3 節）。
- 最初の snapshot では削除対象の古い snapshot はありません。

## 7. 配布チャネルと version

| チャネル | 入口 | 内容と保持 | Profile |
|---|---|---|---|
| **Trial** | Open な PR の `package` label、または **Package trial** の手動実行 | Actions artifacts だけ：`gyo-package-<product>-<platform>`、`gyo-service-<product>`、`diagnostics-<product>-<platform>`。**14 日**保持。Tag や release は作りません | Quick |
| **Snapshot** | `CI gate` が成功した master push | Train ごとに 1 つの prerelease（Latest にはしない）。Tag は `<train>-snapshot-<yyyymmdd>-<sha7>`（日付は commit の UTC 日付で、命名専用）。Train ごとに公開順で直近 **5 個**を残し、それより古い snapshot release とその snapshot tag を削除します（他の tag には触れません） | Quick |
| **正式リリース** | **Prepare Release**（手動）→ 人が **Publish release** | Train ごとの Draft。Tag は `tools-vYYYY.M.N`（例：`tools-v2026.10.1`）または `<game>-vX.Y.Z`（例：`object_fps_pvp-v5.0.0`） | Release |

- master push の Quick artifacts も trial と同じ名前・保持期間で Actions に残ります。
- Snapshot の説明には、テスト用の自動 snapshot でありサポート対象の release ではないことが書かれます。古い run を re-run した場合、その commit がより新しい snapshot の commit の祖先なら公開しません。
- **Tools の version** は暦 version `vYYYY.M.N` です。月に先頭の 0 を付けず、N は 1 から始めます（例：`v2026.10.1`、`v2026.10.2`）。`-rc.1` などの suffix も使えます。
- **ゲームの version** は `v` 付きの SemVer です（例：`v5.0.0`、`v5.1.0-rc.1`）。
- 製品 version は通信 protocol の version と独立しています。Protocol の互換性は tag で表しません。`projects.csv`／`tools.csv` の `version`、`description` 列は備考で、release version を決めません。
- Archive にはコード署名をしていません（Draft と snapshot の説明に「Archives are not code-signed.」と明記されます）。

## 8. ゲーム train の L4 証拠

- 項目はゲーム自身が `build/acceptance/<game>/checks.json` の任意フィールド `release_evidence` で宣言します。各項目は `name`（識別子）、`description`（1 行、300 文字以内）、`platforms`（4 platform の部分集合）の 3 つだけを持ちます。適用されるのはその train で有効な platform だけです。
- `object_fps_pvp` は `gui_visible_latency`（実ディスプレイでの GUI 可視遅延の短時間測定）と `physical_gpu_visual`（実機 GPU での画面確認）を 4 platform で宣言しています。
- Self-hosted runner は置かず、CI はこれらを実行しません。人が実機で実行し、結果を issue、PR、discussion、artifact のいずれかに記録して、その参照（1 行、500 文字以内、`-` で始まらない）を Prepare Release の `l4_evidence` に入力します。
- 項目を宣言したゲームで `l4_evidence` が空なら、Prepare Release は build 前の最初の job で失敗します。逆に、項目のない train（`tools` など）で `l4_evidence` を入力しても失敗します。Snapshot は L4 証拠を使いません。
- Draft の説明には「Real-device evidence (L4)」節が入り、証拠参照（URL は autolink、それ以外は code span として表示し、Markdown として解釈しない）と各項目のチェックリストが並びます。CI が確かめるのは参照があることだけで、証拠の内容は公開者が確認します。

## 9. 運用規則

1. **Tag は動かさない。** Prepare Release は Draft を作る時点で tag を作成し、同名 tag は同じ commit を指す場合だけ再利用します。Source commit を変える時は新しい version を使います。公開済みの version は変更しません。
2. **公開は手動。** Draft を確認してから人が **Publish release** を押します（または `gh release edit <tag> --draft=false`）。Publish は再 build しません。
3. **master は当日中に直すか revert する。** master 完全実行が失敗すると全 train の snapshot が止まり、次の Prepare Release の前提も崩れます。
4. **Prepare Release は、完全実行（L1 + Quick、`CI gate`）が成功した master の commit からだけ実行する。** Workflow 自体は任意の branch を受け付け、この条件を検査しません。実行前に 10.5 節の手順 1 で確認します。
5. `.github/`、`build/ci/`、`build/acceptance/`、封装 CMake を変える PR は、L1 が封装と install 後の検証を実行しないため、merge 前にその branch で **Cross-platform CI** を手動実行するか `package` label で trial を実行し、Quick 封装が通ることを確認します。
6. 本機の tests や workflow の静的検査は、遠隔の各 platform や Draft の成功を意味しません。公開する revision の Actions Summary、必須 jobs、検証報告、添付を根拠にします。`docs/dev_logs/` の過去の記録は当時の commit の結果です。

## 10. 実例

コマンドは `gh` CLI（`gh auth login` 済み、リポジトリの clone 内で実行）を前提にします。`<n>`、`<run-id>`、日付や SHA は実際の値に置き換えてください。

### 10.1 通常の PR：Draft から merge まで

```sh
git switch -c feature/ui-text-wrap
# 変更して commit
git push -u origin feature/ui-text-wrap          # CI は動かない
gh pr create --draft --base master --fill        # Draft PR
```

1. Draft PR の作成と以後の push では、`Select CI scope`（理由 `draft`）と `CI gate`（成功）だけが実行されます。`Select trial packaging` も表示されますが、label がないので build しません。
2. レビューの準備ができたら Ready にします。

   ```sh
   gh pr ready <n>
   gh pr checks <n> --watch
   ```

   `L1 / windows-x64`、`L1 / linux-x64`、`L1 / macos-arm64`、`L1 / macos-x64` と `CI gate` が実行されます。Head commit を変えずに Ready にした場合、新しい `CI gate` は L1 が終わるまで現れず、その間は Draft 実行の成功が表示されたままです。**新しい `CI gate` が報告されるまで merge や auto-merge の有効化をしないでください。**
3. 以後の push は同じ PR の古い実行を取り消して L1 をやり直します。
4. `.github/`、`build/ci/`、`build/acceptance/`、封装 CMake を変えた場合は、merge 前に封装経路も確認します（10.3 節、または次のコマンド）。

   ```sh
   gh workflow run cross-platform.yml --ref feature/ui-text-wrap
   ```

5. Merge します。

   ```sh
   gh pr merge <n> --merge
   ```

6. master push の完全実行を確認します。Snapshot の公開もこの実行で行われます。

   ```sh
   gh run list --workflow cross-platform.yml --branch master --event push --limit 3
   gh run watch <run-id> --exit-status
   ```

### 10.2 文書だけの PR

```sh
git switch -c docs/release-notes
# docs/ 配下だけを変更して commit
git push -u origin docs/release-notes
gh pr create --base master --fill
gh pr checks <n>
```

`Select CI scope` の Summary に `Reason: docs-only` と表示され、`CI gate` はすぐに成功します。Root の `README.md` などを一緒に変えると `docs-only` ではなくなり、L1 が実行されます。文書だけの PR に `package` label を付けると trial 封装が実行されるため、付けないでください。Merge 後の master push は通常どおり完全実行と snapshot を行います。

### 10.3 PR の試用パッケージを取得する

Label を使う方法（open な PR だけ）：

```sh
gh label create package --description "Build trial packages for this pull request"   # 初回だけ
gh pr edit <n> --add-label package
gh run list --workflow package-trial.yml --branch feature/ui-text-wrap --limit 1
gh run watch <run-id> --exit-status
```

手動実行する方法（input はなく、`--ref` で branch を選びます）：

```sh
gh workflow run package-trial.yml --ref feature/ui-text-wrap
```

完了した run から artifact を取得し、checksum を確認します。

```sh
gh run download <run-id> -n gyo-package-toolchain-macos-arm64 -D trial
gh run download <run-id> -p 'gyo-package-object_fps_pvp-*' -D trial
gh run download <run-id> -n gyo-service-object_fps_pvp -D trial
cd trial && shasum -a 256 -c gyo-toolchain-macos-arm64.tar.gz.sha256      # Linux では sha256sum -c
```

Artifact は 14 日で消えます。Label を付けたままにすると push のたびに全製品の Quick 封装が走るので、不要になったら `gh pr edit <n> --remove-label package` で外します。

### 10.4 master の snapshot を探してダウンロードする

```sh
# tools train の snapshot（公開日時の新しい順。Draft は除外）
gh release list --exclude-drafts --limit 50 --json tagName,publishedAt \
  --jq '[.[] | select(.tagName | startswith("tools-snapshot-"))] | sort_by(.publishedAt) | reverse | .[] | "\(.publishedAt) \(.tagName)"'

# ゲーム train の snapshot
gh release list --exclude-drafts --limit 50 --json tagName,publishedAt \
  --jq '[.[] | select(.tagName | startswith("object_fps_pvp-snapshot-"))] | sort_by(.publishedAt) | reverse | .[] | .tagName'

# 1 つの snapshot から必要な archive だけ取得（tag は例）
gh release download tools-snapshot-20261002-1a2b3c4 -p 'gyo-toolchain-macos-arm64.tar.gz*' -D snapshot
gh release download object_fps_pvp-snapshot-20261002-1a2b3c4 -p 'gyo-object_fps_pvp-gateway-*' -D snapshot
cd snapshot && shasum -a 256 -c gyo-toolchain-macos-arm64.tar.gz.sha256
```

Tag 末尾の 7 文字は master commit の SHA です。説明欄の「CI verification」表で archive ごとの検証水準を確認できます。各 train は直近 5 個しか残らないため、長く保持したいものは正式リリースにします。

### 10.5 Tools のリリースを準備して公開する

1. 対象の master commit で完全実行が成功していることを確認します（規則 4）。

   ```sh
   git fetch origin && git rev-parse origin/master
   gh run list --workflow cross-platform.yml --commit "$(git rev-parse origin/master)" --event push \
     --json databaseId,conclusion,url
   gh run view <run-id> --json jobs --jq '.jobs[] | select(.name == "CI gate") | .conclusion'
   ```

   `conclusion` は Snapshot jobs を含む run 全体の結果なので、規則 4 の条件（`CI gate` の成功）より厳しくなります。Snapshot の公開だけが失敗した場合は、2 行目で `CI gate` job が `success` であることを確認します。Prepare Release は実行時点の `--ref master` の先頭 commit を固定するので、その後に master が動いて新しい commit の実行が終わっていない場合は、完了を待ってから進みます。

2. Prepare Release を実行します。Input は `train`、`version`、`prerelease`（既定 `false`）、`l4_evidence`（tools では空のまま）です。

   ```sh
   gh workflow run prepare-release.yml --ref master -f train=tools -f version=v2026.10.1
   sleep 5
   gh run list --workflow prepare-release.yml --event workflow_dispatch --limit 5 \
     --json databaseId,headSha,displayTitle,createdAt
   gh run watch <run-id> --exit-status
   ```

   `gh workflow run` は run ID を返さず、run の登録にも数秒かかるため、一覧から `displayTitle` が `Prepare Release / tools / v2026.10.1 / master` の run を選び、その `databaseId` を `gh run watch` に渡します。`headSha` が手順 1 の commit と一致することも確認します。最初の job の Summary に train、tag、source commit が表示され、続いて toolchain 4 行を Release profile で build・検証し、成功すれば tag `tools-v2026.10.1` を作って Draft を準備します。

3. Draft を確認します。

   ```sh
   gh release view tools-v2026.10.1
   gh release download tools-v2026.10.1 -D review
   cd review && for f in *.sha256; do shasum -a 256 -c "$f"; done
   ```

   添付は `gyo-toolchain-{windows-x64,linux-x64,macos-arm64,macos-x64}.tar.gz` とそれぞれの `.sha256` の 8 ファイルです。説明には自動生成の変更履歴と「CI verification」表が入ります。Commit、version、添付、説明を確認します。

4. 公開します（人の操作）。GitHub の Draft 画面で **Publish release** を押すか、次を実行します。

   ```sh
   gh release edit tools-v2026.10.1 --draft=false
   ```

RC として出す場合は `-f version=v2026.10.1-rc.1 -f prerelease=true` とします。

### 10.6 ゲームのリリースを L4 証拠付きで準備する

1. 10.5 節の手順 1 で master commit を確認します。
2. 実機で宣言項目（`gui_visible_latency`、`physical_gpu_visual`）を実行します。具体的な方法は各項目の `description` とゲームの文書に従います。結果を 1 つの issue にまとめます。

   ```sh
   gh issue create --title "object_fps_pvp v5.0.0 L4 evidence" --body-file l4-evidence.md
   ```

3. Issue の URL を `l4_evidence` に入れて Prepare Release を実行します。

   ```sh
   gh workflow run prepare-release.yml --ref master \
     -f train=object_fps_pvp -f version=v5.0.0 \
     -f l4_evidence=https://github.com/yojinn-io/GYO-Engine/issues/<n>
   ```

   `l4_evidence` を省くと、最初の job が `Train object_fps_pvp requires real-device (L4) release evidence for: gui_visible_latency, physical_gpu_visual` で始まるエラー（全体は `Release validation failed: ` が前に付き、issue・PR・discussion・artifact に記録して link を `l4_evidence` に入れるよう指示が続きます）で失敗し、build は始まりません。

4. Toolchain baseline 4 行と `object_fps_pvp` の 4 行（Release profile）、Linux 行の gateway cross build が成功すると、tag `object_fps_pvp-v5.0.0` と Draft が作られます。添付は `gyo-object_fps_pvp-<platform>.tar.gz` 4 つ、`gyo-object_fps_pvp-gateway-linux-x64.tar.gz` と `...-windows-x64.tar.gz`、それぞれの `.sha256` の計 12 ファイルです。Toolchain の archive はゲームの Draft に入りません。
5. Draft の「Real-device evidence (L4)」節で各項目を証拠と照合してから **Publish release** を押します（または `gh release edit object_fps_pvp-v5.0.0 --draft=false`）。

### 10.7 新しいゲームやツールをリリースに加える

**ゲーム**（例：`my_game`）。変更するのはそのゲームのコンテンツと登録データだけです。

1. `apps/my_game/`（`CMakeLists.txt`、`project.json`、source）と `assets/my_game/` を用意します（[ゲームの作成とコピー](creating_apps.md)）。
2. `engine/config/projects.csv` に行を追加します。

   ```csv
   my_game,My game,,1,1,1,1
   ```

3. 必要なら `build/acceptance/my_game/checks.json` で install 後の checks を宣言し、実機確認が必要なら `release_evidence` を加えます。

   ```json
   {
     "version": 1,
     "checks": [],
     "release_evidence": [
       {
         "name": "physical_gpu_visual",
         "description": "Client on a physical GPU: start and play one round; no missing geometry, shader or presentation errors",
         "platforms": ["windows-x64", "linux-x64", "macos-arm64", "macos-x64"]
       }
     ]
   }
   ```

4. Go service を持つ場合は `apps/my_game/go.mod` を置き、ゲームの `CMakeLists.txt` で宣言します。ゲームは Linux で有効でなければならず、`PLATFORMS` はそのゲームで有効な platform の部分集合でなければなりません（違反は configure で失敗します）。

   ```cmake
   gyo_app_add_go_service(gateway PACKAGE ./gateway/cmd PLATFORMS linux-x64 windows-x64)
   ```

自動で変わること：L1 が有効な platform でゲームを build・test し（`ci-<os>` preset は `GYO_APPS=AUTO`）、Go module を Linux 行で検査します。master の Quick に `my_game` の行が加わり、archive が `gyo-package-my_game-<platform>` として upload されます。`my_game` train の snapshot（`my_game-snapshot-...`）が公開され始め、Prepare Release が `train=my_game` を受け付けます。Service archive は `go-services.json` を通じてその train に入ります。Workflow や `build/ci` の変更は不要です。ゲームを外す時は CSV 行を無効にするか、そのゲームの内容と登録をまとめて削除します。

**ツール**（例：`level_editor`）：

1. `tools/level_editor/`（`CMakeLists.txt`、`project.json`）を用意します。Release ツールの既定 variant は `packageable: true` で、`requires_apps` は空でなければなりません（[ツールの登録と選択](tool_projects.md)）。
2. `engine/config/tools.csv` に行を追加します。

   ```csv
   level_editor,Level editor,1,true,true,true,true,true,true
   ```

3. `build/acceptance/level_editor/checks.json` で、選択した各 release platform について `quick` と `release` の両方で登録済み executable または probe を実行する check を宣言します。不足は configure で失敗します。

自動で変わること：`default=true` なら L1 で build・test され、`release=true` の platform では `gyo-toolchain-<platform>` に入ります。Archive 検証は tool owner 集合の完全一致を要求し、期待集合は registry から導出されるため、次の snapshot と `tools` train の Draft から自動的に含まれます。

### 10.8 L1 を開発機で再現する

L1 の 1 行は「`ci-<os>` preset で configure → build → `cpu|shader` label の tests」です。Apple silicon の Mac では次のとおりです。

```sh
cmake --preset ci-macos
cmake --build --preset ci-macos
ctest --preset ci-macos              # label cpu|shader。tests が 0 件ならエラー
```

`macos-x64` 行（Rosetta 2 が必要）：

```sh
softwareupdate --install-rosetta --agree-to-license   # 未導入の場合
cmake --preset ci-macos-x64
cmake --build --preset ci-macos-x64
ctest --preset ci-macos-x64
```

Linux（gcc-14）では `ci-linux`、Windows（Visual Studio の開発者 shell、`cl`）では `ci-windows` を同じ形で使います。CI と同じ compiler がない場合は、同じ label filter を持つ `test` preset で代用できます。

```sh
cmake --preset test && cmake --build --preset test && ctest --preset test
```

Linux 行だけが追加で実行する部分：

```sh
# Host shader tests（shader host tools を build した構成のみ）
ctest --test-dir build/target/_build/ci-linux/host-tools -C Release -L shader --output-on-failure --no-tests=error
# 共通 GPU tests（Vulkan と表示環境が必要。CI は Xvfb + Lavapipe）
xvfb-run -a ctest --test-dir build/target/_build/ci-linux -L gpu -R '^render\.' --no-tests=error --output-on-failure
# Core baseline
cmake --preset core -B build/target/_build/ci-linux-core -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
cmake --build build/target/_build/ci-linux-core
ctest --test-dir build/target/_build/ci-linux-core --output-on-failure --no-tests=error
```

Go（configure 後の `gyo-build.json` に記録された module と service を使います。PATH 上に Go が必要です）：

```sh
python3 build/ci/common/go_checks.py plan  --build-info build/target/_build/ci-macos/gyo-build.json
python3 build/ci/common/go_checks.py check --build-info build/target/_build/ci-macos/gyo-build.json          # vet と test
python3 build/ci/common/go_checks.py check --build-info build/target/_build/ci-macos/gyo-build.json --race   # CI は Linux で race も実行
python3 build/ci/common/go_checks.py build --build-info build/target/_build/ci-macos/gyo-build.json --output-dir build/target/_build/ci-macos/go-services
ctest --test-dir build/target/_build/ci-macos -L go     # ゲームの Go module を CTest から実行（race は Linux host のみ）
```

`Select CI scope` が最初に実行する CI policy tests：

```sh
python3 -m unittest discover -s tests/common/ci -v
```

本機で通っても、遠隔の 4 platform の L1 が通った証拠にはなりません（規則 6）。

## 11. 製品の組立と検証（詳細）

### 組立

本機と CI は同じ製品組立を使います。`build/assemble_runtime.py` は `assets/<game>/content.json` に従って準備済み資産を検証・配置し、generic CMake hook から自動で呼ばれます。Offline shader compiler は `engine/render/shaders/pipeline` が所有します。通常のゲーム build に CI／tests や package acceptance は不要です。

組立と package 検証は `build/content_contract.py` を共有し、C++ runtime と Editor の native parser は共通 fixtures で検証します。Catalog には整数の `version: 1`、有効な entry、内容集合全体で一意な ID が必要です。資産ディレクトリがなければ空内容として同期し、ディレクトリがあって descriptor が欠落・不正なら失敗して直前の成功出力を保持します。Object_FPS は実際に使う builtin shaders だけを配布し、カスタム shader の検証サンプルは共通 GPU テストが所有します。

### Manifest と install 後の検証

- 生成される product manifest は `share/gyo/products/<product>/manifest.json`（schema 3）です。空でない build configuration を要求し、executable を `owner.role` で登録して、owner、role、path、runtime_dependencies を記録します。ほかに required_files、native_files、owner ごとの checks を持ちます。依存物の和集合はコピー対象の決定だけに使い、linkage は実行ファイルごとの要求で検査するため、SDL GUI と SDL 不要の CLI を同じ package に置けます。
- 共通 runner は `build/acceptance/common/run_package_checks.py`、native linkage 検査は同じ場所の `validate_package.py` です。専用規則は `build/acceptance/<owner>` に置きます。
- CMake が生成する `<build>/packages/<product>/<configuration>/acceptance-context.json` を `--context` で runner に渡します。Context は manifest と build configuration に結び付き、`@CHECK_ROOT@`、`@PROBE:<role>@` の外部位置だけを解決し、検証コマンドや集合を変えません。`@EXECUTABLE:<role>@` は owner ごとの製品を参照し、検証とログの名前は `owner.name` です。Runner は製品の一時コピーへ probe を置けますが、正式な実行ファイルを上書きしません。製品には context、probe、Python 検証コードを含めません。Runtime は `bin/assets/<game>` だけを読みます。
- Quick と Release の範囲、GPU suite の引数は owner の checks が決めます。Linux toolchain 行はゲームがなくても Xvfb／Lavapipe で共通エンジンの GPU 描画テストを実行し、toolchain を含む全製品は自身の契約が宣言した GPU checks も独立に実行します。必要な能力の欠落、timeout、異常は失敗です。[Object_FPS の外部検証](object_fps/acceptance.ja.md)も参照してください。

### Archive と Draft の検証

- 封装、snapshot、Draft 準備は、固定した checkout の registry から train の期待集合を再計算します。Product／platform、tool owner 集合、checksum、archive path の安全性、manifest、必要内容、profile の証拠を確認し、service archive は service record（`go-services.json`）と完全に一致させます。欠落、余分な製品、一部のツールだけを含む toolchain、重複 identity は拒否します。正式 Draft は Release の証拠だけを受け付け、snapshot は Quick の証拠を受け付けます。
- `acceptance.json` の `package_sha256` は製品内の全ファイル内容とシンボリックリンク先を結び付け、封装時に生成する root の `build_metadata.json` だけを除外します。検証と封装の間に内容が変われば拒否し、archive 検査でも同じ digest を独立に再計算します。製品隔離は登録済み実行ファイル、native files、コンテンツの一覧に従い、接頭辞や拡張子によらず未登録ファイルを拒否します。シンボリックリンクは宣言済み native files に限り、参照先を package 内に制限します。

## 12. 失敗と再実行

- Prepare Release は最初に完全な source SHA を固定するため、後続の branch 更新は混入しません。必須 job の失敗、取消、意図しない skip は Draft の準備を止めます。一部 platform の成功を全体の成功として扱いません。
- 一時的な HTTP 500／502／503／504、timeout、connection reset、途中切断は初回を含め最大 4 回まで復旧を試みます。通常の待機は 2／4／8 秒で、`Retry-After` が 60 秒を超える時は停止して報告します。権限、データ、version の衝突は直ちに失敗します。再試行のたびに tag／Draft／添付を読み直し、source と checksum を確認して不足分だけを補います。
- 一時的な失敗：**Re-run failed jobs** で同じ実行の SHA を保ったままやり直します。
- Artifacts の期限切れ：元の実行の **Re-run all jobs** を使います。
- 一部だけ添付された Draft：再実行で既存の添付を検証して不足分を追加し、手書きの title と説明を保持します。
- 作成した直後の tag はまだ読めないことがあります（GitHub の読み取りは書き込みより少し遅れます）。その場合は 1／2／4／8 秒の間隔で読み直し、それでも読めない時だけ失敗します。読めた tag が別の commit を指す時は直ちに失敗します。
- 同名 tag は同じ commit を指す場合だけ許可し、tag の移動や不一致の添付の上書きはしません。既存 Draft の tag が消えている場合は失敗します（tag を元に戻すか、新しい version を使います）。
- 未完了の `starter` 添付は有限回再確認し、解決しなければ自動削除せずに報告します。
- 古い実行の再実行はその commit の workflow を使い、新しい修正を取り込みません。同じ train と version の Prepare Release は並行実行せず、新しい要求で実行中の準備を取り消しません。既存 Draft の prerelease、title、説明は保持します。
- 公開済みの snapshot に添付の不足があれば変更せずに失敗します。同じ commit を再公開するには、その snapshot を手動で削除します。正式 release に昇格した snapshot は変更しません。
- Draft job と snapshot job だけが `contents: write` を持ち、build jobs は read-only です。Workflow ファイルを変える branch から tag や Draft を作る時に 403／404 になる場合は、Contents と Workflows の write 権限を持つ token を明示的に設定する必要があります。

## 13. 今後の課題

- **Tools とゲームの間の Data Contract 検査**：Tools train とゲーム train は独立に version を付けますが、ツールが書き出す data とゲームが読む data の互換性を宣言・検査する仕組みはまだありません。
- **コード署名**：Archive と実行ファイルは署名していません（TODO）。
- **Merge queue**：`merge_group` trigger は未対応です。
- **Prepare Release の source 条件**：規則 4（完全実行が成功した master commit からだけ実行）は運用上の約束で、workflow は検査していません。
