# AGENTS.md

# GYO-Engine Agent Rules

このリポジトリは現在、アーキテクチャを探索・進化させながら開発している。

現在の構造は最終設計ではない。
ただし、不確実性を理由に Agent が独自に大規模な再設計を行ってはならない。

本ルールの目的は Architecture を固定することではない。

目的は、

* Architecture Drift を可視化する
* Ownership と Dependency Direction を維持する
* Product Boundary を壊さない
* 実際の Implementation Pressure に基づいて Architecture を進化させる
* Feature 実装を理由とした不要な一般化や大規模再設計を防止する

ことである。

@agents/effort-gearbox.md

---

## 1. Core Principle

機能実装では、既存アーキテクチャを理解した上で、
必要最小限の変更を優先する。

### MUST

* 既存コードを確認してから変更する。
* 既存の責務（Ownership）と依存関係を確認する。
* Feature に直接必要な変更を優先する。
* 変更範囲をできるだけ局所化する。
* 既存の設計意図が不明な場合、それを「誤り」と仮定しない。
* Product / Module / Tool の owner を確認してから、その責務を変更する。

### MUST NOT

* Feature 実装を理由に、無関係なコードを整理しない。
* 単一の Use Case だけを根拠に新しい汎用 Abstraction を作らない。
* 将来必要になるかもしれないという理由だけで設計を一般化しない。
* 明確な根拠なしに既存 Module の責務を移動しない。
* 見た目を綺麗にするためだけの Architecture Refactoring を行わない。
* 既存の Ownership Boundary を暗黙に変更しない。

---

## 2. Architecture Is Allowed to Evolve

Architecture の変更自体は禁止しない。

Feature の実装、Architecture Review、Refactoring によって既存構造の問題が確認された場合、
Architecture を変更してよい。

ただし Architecture Change は、
実際に観測された Implementation Pressure に基づかなければならない。

### Valid evidence

* Responsibility の重複
* Ownership の曖昧化
* Dependency Direction の悪化
* 循環依存
* Lifecycle の衝突
* 同じ Adapter / Conversion の繰り返し
* 複数箇所に現れる同一 Special Case
* Module Boundary を越えた内部知識の漏洩
* Product の追加・複製・削除に公共層の変更が必要になる
* Editor / Application / Engine の責務が混在する
* Product 固有 Knowledge が reusable layer に漏れる
* Product 固有データの存在を共通処理が前提とする

Architecture Change SHOULD solve observed pressure,
not predicted pressure.

### Derivation Order：Feature → Contract → Architecture

任務を計画・分割するときは、次の順序で推導する。
三者は排他的な分類ではない。Contract も Architecture の一部であり、
この順序は推導と優先の標準過程を定めるものである。

1. **Feature**：何を作るかを先に決める。利用者（プレイヤー）から見える部分。
2. **Contract**：その Feature のために、Data や Wire で何を約束する必要があるか。
3. **Architecture**：その Contract を支えるために、構造をどう変える必要があるか。
   Contract が変わっても、Architecture が変わるとは限らない。
   支えきれない場合にのみ変更する。

目的：

* 事故、Contract の変更、Architecture の再構成が衝突したときに、
  三者が同時に止まり Feature が停滞する事態（難産）を防ぐ。
* 衝突した場合は Feature の方向を保ち、深い層ほど範囲を縮める。
* Contract と Architecture の作業は、それが支える Feature（次の版の Feature を含む）を明示する。
  支える Feature を示せない項目は延期する。

---

## 3. Architecture Drift Control

作業中に Architecture が変化する場合、
その変化を暗黙に行ってはならない。

次の変更が発生する場合は Architecture Delta として扱う。

* 新しい Top-level Directory
* 新しい Subsystem
* Module Ownership の変更
* Product Ownership の変更
* Dependency Direction の変更
* Public Interface の大規模変更
* 複数 Module にまたがる Responsibility 移動
* Build Graph / CMake Module 構造の変更
* Product Registration 構造の変更
* Runtime と Editor の Data Contract の変更
* 大規模な File Move
* 他の Component が権威を持つ状態の写し、またはそれに基づく判断の追加（下記「状態の権威」）
* Component の Design Invariants に反する変更（下記「Component の設計約束」）

Architecture Delta が必要な場合、変更理由を明確にする。

最低限、以下を説明する。

1. どの Feature または明示された Review / Refactoring の目的が変更を必要としているか
2. 現在の構造で何が問題になっているか
3. どの Architecture Boundary が変化するか
4. 影響する Module / Product / Tool
5. Dependency Direction がどう変化するか
6. Ownership がどう変化するか
7. より小さい変更で解決できない理由

Architecture Delta は禁止事項ではない。

ただし、暗黙の Architecture Drift は禁止する。

### Component の設計約束（Design Invariants）

複数の Agent と複数の版にまたがって守るべき設計意図は、
思考や会話の中だけに置かず、検査できる約束として文書に書く。

* 各 owner は、変えてはならない設計意図を、その owner の Architecture 文書（`docs/<owner>/` 配下。Engine は `docs/architecture/` 配下）に
  見出し `Design Invariants` の節として書く。
  見出しは文書の言語にかかわらず、この英語表記のまま変えない（翻訳による意味のずれと探し漏れを防ぐため）。
* 可能なものは、守衛となる Test も用意する。
* Component を変更する前に、その owner の `Design Invariants` 節を読む。
* 該当節に反する変更は、変更が小さくても Architecture Delta として扱い、ユーザーの同意を得る。
* 節そのものの変更・削除も、ユーザーの同意を要する Architecture Delta とする。
* 一般原則（「Ownership の変更は提出する」）だけでは、
  個々の変更がその Ownership 変更に当たるかを判断できない。
  判断できる形で書かれた約束だけが、局所的に妥当な修正の積み重ねによる Drift を止められる。
* 新しい Component を作るとき、または既存の Component に大きな責務を加えるときは、
  その Component を置き換える条件を、`Design Invariants` 節の中に `Replacement Criteria` として書く
  （例：Feature を追加するたびに同じ検証をやり直す必要がある、Design Invariants を保てない）。
  条件は、その Component に機能を積み上げる前に書く。積み上げた後では、失うものを惜しんで判断が歪むためである。
* 条件を満たしたときは、修正を続けずに、置き換えるか修正を続けるかをユーザーに提案する。

### 状態の権威（State Authority）

判定に使われる状態は、それぞれ一つの Component が権威（Authority）として所有する。

* 他の Component が持ってよいのは、自分の責務のための、再構築でき判定に参加しない状態だけである
  （接続、session、上限のあるキュー、設定の写し、診断の計数など）。
* 他の Component が権威を持つ状態の写しを持たない。
  それに基づく判断（重複除去、統合、古さの判定、衝突の解決、再送の予定など）を行わない。
* 判断基準：そのフィールドや判断を正しく行うために「権威を持つ Component がすでに何を持っているか」を
  知る必要があるか。必要なら、それは権威側に属する。
* 他の Component が権威を持つ状態に関わるフィールドや判断を追加すること自体が Ownership の変更であり、
  Architecture Delta である。局所的に妥当な修正であっても同じである。

---

## 4. Reversibility

設計がまだ探索段階であるため、
同等の選択肢がある場合は Reversible な変更を優先する。

### Prefer

* Local Adapter
* Existing Interface Extension
* Small Vertical Slice
* Local Data Transformation
* Product-local Registration
* Explicit Data Contract

over:

* 新規 Subsystem
* 大規模な Hierarchy
* 多数の File Move
* Repository 全体に影響する Abstraction
* Product 固有要件を吸収する汎用 Framework
* 任意条件を記述可能な設定言語

ただし、一時的な workaround を永続設計として固定しない。

局所的であることと、責務が正しいことは別である。

Product Boundary や Dependency Direction を破壊する workaround を、
「変更量が少ない」という理由だけで採用してはならない。

---

## 5. Process Evolution

開発・設計・運用の手順は、原則として次の順序で成熟させる。

~~~text
手動 → Script → Tool → Platform
~~~

まず手順、入力、出力、失敗条件を明確にし、
手動で理解・再現できる状態にする。

Script はその手順を再現し、
Tool は確認された反復作業を支援する。
Platform 化は、実際の複数用途と運用上の必要性が確認された場合に検討する。

### MUST

* 自動化する前に、対象の手順と責任範囲を明確にする。
* 手動で確認した手順を、Script や Tool でも追跡できる形で維持する。
* 処理の入力、出力、失敗条件を明示する。
* 次の段階への移行は、観測された反復作業や運用上の必要性に基づいて判断する。

### MUST NOT

* 将来の可能性だけを理由に、段階を飛ばして一般化しない。
* 手順の不明確さを Tool や Platform の内部へ隠さない。
* 既に確立された手順を、毎回手動からやり直すことを要求しない。
* すべての Script や Tool が Platform へ進化することを前提にしない。

必要性が確認できる段階で止めてよい。
Tool 化や Platform 化そのものを目的にしてはならない。

---

## 6. Dependency and Product Boundaries

Engine は再利用可能な機能を提供し、
Application と設計支援ツールはその機能を利用する。

基本的な Dependency Direction は以下とする。

```text
Application ──→ Engine
Editor      ──→ Engine
```

Engine は具体的な Application や Editor に依存してはならない。

Architecture の分離は、
Directory の配置や命名だけで判断しない。

Product の追加・複製・削除によって検証する。

### MUST

* Lower-level infrastructure MUST NOT depend on application-specific code.
* Application-specific code MAY depend on reusable engine infrastructure.
* Editor MAY depend on reusable engine infrastructure.
* 新しい Dependency Edge を追加する場合、既存の Dependency Direction を確認する。
* Product 間依存が必要な場合は明示する。
* Product 固有 Knowledge を reusable Engine layer へ漏らさない。

### MUST NOT

* Engine から具体的な Game / Application を参照しない。
* Engine から具体的な Editor を参照しない。
* 公共層での具体的な Product 名による分岐の禁止は §9 を参照。
* 暗黙の Product 間依存を作らない。
* 循環依存を新しく作らない。

---

## 7. Product Ownership and Removability

各ゲーム・ツールには、明確な owner を定める。

owner は単一 Directory と一致する必要はない。

コード、アセット、設定、テスト、受け入れ検証は、
責務ごとに異なる Directory へ配置してよい。

ただし、それぞれがどの owner に属するかを識別できなければならない。

Product Boundary の重要な検証方法は、

> その Product を完全に削除したとき、
> 関係のない Product と Engine が成立するか

である。

### MUST

* owner の登録情報と専用コンテンツをすべて削除した後も、Engine と依存関係のない他の Product が成立すること。
* 専用 Test と Acceptance Validation は、対応する owner が選択された場合のみ組み込む。
* Product の追加・複製・削除は、その Product のコンテンツと登録データの変更で完結させる。
* 完全な削除では、無効な登録情報、専用支援コード、配置済みの残存ファイルも整理する。
* 共通 Test は必要な機能を自ら宣言する。
* 共通 Test は共通または合成の Test Data を使用する。
* 共通 Test は特定 Product の存在を前提にしない。
* Product 間依存が存在する場合、その依存関係を登録または契約として明示する。

### MUST NOT

* 通常の Product 削除に、共通 Build の修正を必要とする構造を作らない。
* 通常の Product 削除に、共通 Packaging 処理の修正を必要とする構造を作らない。
* 通常の Product 削除に、共通 Acceptance Validation Runner の修正を必要とする構造を作らない。
* 通常の Product 削除に、共通 Workflow の修正を必要とする構造を作らない。
* 削除済み Product のために空 Directory を残さない。
* 削除済み Product のために Dummy Target を残さない。
* 削除済み Product のために仮設定を残さない。
* 削除済み Product のために Compatibility Branch を残さない。
* Engine の初期化や検証のために、特定 Game / Tool の維持を要求しない。
* 公共処理から参照されている専用コンテンツを「削除済み」と扱わない。

Product の削除に公共層の変更を必要としないことは、
Architecture が満たすべき性質である。
既存の結合を修復するための公共層変更を禁止するものではない。

既存構造がこの性質を満たしておらず、その修復がタスクの範囲に含まれる場合は、
必要な公共層変更を行い、理由と影響を Architecture Delta として明示する。
範囲外の問題は報告し、独断で修復範囲を拡大しない。

Build 対象から外すだけでは、完全な削除とはみなさない。

完全な削除では、

* owner に属する Content
* owner Registration
* owner-specific Test
* owner-specific Support Code
* owner-specific Validation
* owner への有効な Reference

を整理する。

---

## 8. Editor and Application Data Contract

汎用の設計支援ツールは、
明示された Data Contract を通じてゲームの Content を読み書きする。

Application はその Data Contract を利用する。

Application は、
Data を生成した Tool を知る必要はない。

概念的な関係は以下とする。

```text
Editor ───────┐
              │
Script ───────┼──→ Data Contract ──→ Application
              │
Manual Data ──┘
```

Editor と Application の関係を、

```text
Application → Editor
```

にしてはならない。

### MUST

* Game と Tool が共有する Format を Data Contract として扱う。
* Version を Data Contract の一部として扱う。
* Validation Rule を Data Contract の一部として扱う。
* Contract に適合する Data は、手作業、Script、別 Tool でも生成できるようにする。
* Editor を削除した後も、既存の有効な Data を Application が利用できるようにする。
* Runtime は Editor の存在を前提にしない。

### MUST NOT

* Editor が生成した Data を読み込むために、Application に Editor の Link を要求しない。
* Editor が生成した Data を読み込むために、Application に Editor の Initialization を要求しない。
* Editor 専用実装を Game Source Code へ埋め込まない。
* 通常の Content 編集を Game の C++ 書き換えで実現しない。
* 通常の Content 編集を CMake 書き換えで実現しない。
* 通常の Content 編集を起動処理の書き換えで実現しない。
* Runtime を Editor の内部構造へ依存させない。
* Runtime を開発環境へ依存させない。

ゲームの C++ に直接依存する Preview や Diagnostic Program は、
そのゲームに明示的に依存する専用支援として扱う。

独立した登録 owner を持ってよいが、
対象ゲームとの依存関係を登録データに明示する。
用途上の帰属と登録 owner の識別子を混同しない。

それらを Data Contract のみに依存する
汎用 Tool として扱ってはならない。

---

## 9. Common Processing and Product Data

共通コードは、

* Contract の解釈
* Dependency の構成
* 共通処理の実行

を担当する。

Product ごとの差異は、
その Product の Registration / Description Data で表現する。

概念的には以下を目指す。

```text
Common Logic
     │
     ├── interprets ──→ Product Registration
     │
     └── executes ────→ Declared Dependencies / Roles / Validation
```

共通コードが Product の内部事情を知ってはならない。

### MUST

Product ごとに必要な以下の情報は、対応する Data に記述する。

* Product Selection
* Mode
* Dependency
* Role
* Acceptance Validation
* Product-specific Component Selection

同じ事実を手動で維持する場所は、一か所に限定する。

Dependency の欠落や競合は、
Contract に基づいて明確な Error にする。

### MUST NOT

* 公共層で具体的な Game 名によって動作を分岐しない。
* 公共層で具体的な Tool 名によって動作を分岐しない。
* 命名規則だけを利用して Product の意味を推測しない。
* 新しい Product を追加するために公共層へ Product 専用 Branch を追加しない。
* 専用 Branch を Data へ移しただけで、共通コードがその Product の内部事情を理解する構造を残さない。
* 単一の例外を処理するために、任意条件を記述できる Configuration Language を作らない。
* 単一の例外を処理するために、不要な Generic Platform を作らない。

Data-driven であること自体は Architecture Boundary ではない。

「if 文を JSON に移しただけ」の構造は、
Product Separation を達成したとはみなさない。

残された専用 Tool が削除された Game へ明示的に依存している場合は、
その Tool の Selection または Dependency Data を修正する。

公共コードの Special Case によって
Dependency の欠落を隠してはならない。

---

## 10. Top-level Structure

Top-level Directory は Architecture Concept とみなす。

新しい Top-level Directory を追加する場合、
単なる File 整理ではなく Architecture Change として扱う。

Feature 固有 Data や単一 Asset Type を理由に、
安易に新しい Top-level Domain を作らない。

Directory Structure は Ownership の表現方法の一つではあるが、
Ownership そのものではない。

次の事実だけを理由に Architecture Separation が成立したと判断してはならない。

* Directory が分かれている
* Target が分かれている
* Namespace が分かれている
* CMake Option で Build を無効化できる
* File 名に Product 名が付いている

実際の Dependency / Ownership / Removability を確認すること。

---

## 11. Refactoring

Refactoring は禁止しない。

ただし、依頼された作業と無関係な Refactoring を同時に行わない。

### Refactoring SHOULD

* 現在観測されている Code Smell を解消する。
* Responsibility を明確にする。
* Ownership を明確にする。
* Dependency を単純化する。
* 重複を削減する。
* Feature 実装後に明らかになった構造上の問題を解消する。
* Product Boundary の漏洩を修正する。
* Removability を妨げる Dependency を解消する。

### Refactoring MUST NOT

* 「将来便利そう」という理由だけで抽象化する。
* ユーザーが指定した作業範囲を超えて、Repository 全体を独断で再構成する。
* Feature Scope を不必要に拡大する。
* Product Separation を理由に不要な Framework を導入する。
* 単一の Special Case を理由に Plugin System や DSL を作る。

Architecture Review や大規模 Refactoring 自体が明示されたタスクの場合は、
合意された範囲で実施してよい。
その場合も、観測された問題と Architecture Delta を明示する。

必要最小限の変更とは、要求された目的を満たす範囲で判断する。
明示された構造上の問題を未解決のまま残す理由として、
変更量の少なさを優先してはならない。

既存の例外や両立しない要求を発見した場合は、

* Dependency
* Ownership
* 影響範囲
* 解決が難しい点

を説明する。

問題を隠してはならない。

また、それを理由に無関係な大規模 Refactoring を行ってはならない。

---

## 12. Architecture Fitness Checks

Architecture を固定するのではなく、
既知の重要な性質を継続的に監視する。

Build、Tool、Packaging、Acceptance Validation、
Product Registration、Runtime Data Contract を変更した後は、
最低限以下を確認する。

### Dependency

* 新しい循環依存が発生していないか。
* 不自然な逆方向 Dependency が追加されていないか。
* 明示されていない owner 間 Dependency を追加していないか。
* Feature-specific Knowledge が reusable Engine layer に漏れていないか。
* CMake Dependency Graph が意図せず変化していないか。

### Ownership

* 既存 Module の Responsibility が意図せず拡大していないか。
* 同じ Responsibility が複数 Module に分散していないか。
* Ownership が不明な File が追加されていないか。
* Product-specific Support Code の owner が識別できるか。

### Product Removability

* owner を削除するために公共層の修正が必要になっていないか。
* 公共処理に削除した owner の Content への Reference が残っていないか。
* 依存関係のない他の Product に影響していないか。
* 削除後に Dummy Target / Empty Directory / Compatibility Branch が残っていないか。
* 不要になった専用 Support Code が残っていないか。
* 無効な Registration が残っていないか。

### Product Add / Clone

* Product の追加が Product Content と Registration Data の変更だけで完結するか。
* Product の複製が公共層の修正を要求しないか。
* 新しい Product のために共通コードへ専用 Branch を追加していないか。

### Editor / Runtime Separation

* Editor がなくても Game と既存の有効な Data を利用できるか。
* Runtime が Editor Implementation を参照していないか。
* Runtime が Tool 内部構造を理解していないか。
* Data Contract 以外の暗黙依存が追加されていないか。

### Architecture Concepts

* Top-level Architecture Concept が不要に増えていないか。
* 新しい Subsystem が単一 Use Case だけのために作られていないか。
* Configuration が不要な Generic Language に進化していないか。

Directory の移動、Build の無効化、Hardcode の隠蔽だけでは、
Architecture Fitness Check を通過したとはみなさない。

Architecture Fitness Check の失敗は、
必ずしも変更禁止を意味しない。

Review が必要な Architecture Delta が発生したことを意味する。

---

## 13. Code Review Rules

Review ではコードの正しさだけでなく、
Architecture Drift を確認する。

特に以下を確認・指摘する。

* 新しく生まれた Responsibility
* Responsibility の移動
* Ownership の変更
* 新しい Dependency Edge
* 新しい Architectural Concept
* Product Boundary を越える Knowledge
* 一時的処理が Architecture に昇格している箇所
* Existing Concept と重複する新しい Abstraction
* Feature Scope を超えた変更
* Product-specific Branch が公共層へ追加された箇所
* Editor / Runtime Boundary の破壊
* Product Removability を阻害する Reference
* 他の Component が権威を持つ状態の写し、またはそれに基づく判断の追加
* Component の Design Invariants に反する変更

Code Style や Formatting の問題より、
Architecture / Ownership / Dependency の問題を優先する。

ただし、問題を発見しただけで
自動的に Scope 外 Refactoring を開始してはならない。

必要な場合は問題として報告する。

---

## 14. Test and Defect Handling

Test は作業を検証する重要な根拠だが、Test 自身も仮説である。
Test と実装が一致しないときは、どちらが（あるいは前提や計測経路が）誤っているかを先に判断する。
Test が正しいと決めつけて実装を合わせることも、実装が正しいと決めつけて Test を合わせることもしない。

同じ問題で Test・計測・判定の方法を 2 回変えても結論が出ない、または見積もりを大きく超えた場合は、
続けずにユーザーに報告する（使った時間、分かったこと、次の試行で変わる判断）。

### 14.1 Test の結果の扱い

* 再実行で結果が変わる Test は、原因がどれに当たるか（複数ありうる）を切り分けて報告する：
  実装（Defect）、Test 自身（Script・Analyzer の誤り）、環境（ネットワーク・ホストの状態など）。
  切り分けられない場合は、その事実を報告する。
* 受け入れ判定に使う試行（事前宣言した計測など）の失敗は、後の成功で上書きせず、記録に残す。
  探索のための試行や、不要と判断して取りやめた Test は、この対象ではない。取りやめた場合は理由を書く。
* 計測に基づく受け入れ判定（閾値、試行回数、再試行の規則）は、結果を見る前に宣言し、ユーザーの承認を得る。
  結果を見た後に緩めない。宣言した判定が不成立のときは、再実行や修正の前にユーザーに報告する。

### 14.2 Test の根拠

* Test の根拠は、可能な限り Agent の理解以外から得る（観測された現象、実データの Replay、Contract の規則など）。
* Agent の理解だけに基づく Test を受け入れの唯一の根拠にする場合は、
  実装を壊したときにその Test が失敗すること（突然変異検査など）で検出力を示すことが望ましい。

### 14.3 Defect の対応

1. 現象を再現する。
2. 現象を表す Assertion で失敗する Test として固定し、失敗（Red）を確認する。
3. その Test を変えずに実装を修正し、成功（Green）を確認する。
4. 修正が Feature Scope や Architecture に影響する場合、§3 に従い Architecture Delta として報告する。
5. その Test を Regression Test として残す。

* 確率的にしか再現しない場合は、試行回数と判定を宣言し、修正の前後を同じ宣言で比較する。
* 既知の現象を入れても計測に現れない、または計測が 0 件のときは、測り方を変えて再現させる前に、
  観測経路が途中で捨てていないか（送った数・届いた数・計数した破棄の収支）を確かめ、ユーザーに報告する。
* 再現できない（Test として固定できない、必要な試行回数が現実的でない場合を含む）ときは、
  その事実と観測した証拠を記録し、修正の前に観測手段（診断・計数）を追加して、そこで止めて報告する。
  「修正済み」とは報告しない。
* 前提（Contract、Architecture、計測の基準）が誤っている疑いがあり、
  正しい Test を書けない、または Defect であることを証明できない場合は、
  修正を進めず、その状況をユーザーに報告する。
* 原因が特定できない状態で「修正済み」と報告しない。

### 14.4 Test を変える場合

* 既存 Test の判定（期待値、条件、許容差、skip、実行対象からの除外など）を変えるときは、
  その理由を変更の説明に書く。
* Test を通すためだけに判定を緩めない。実装の中で Test の入力を識別して分岐しない。

受け入れ判定または Design Invariants の守衛 Test の判定を変えるときは、次を平易な言葉で示す。
ユーザーに Test のコードを読ませない。

1. その判定が守っていた性質
2. 緩める面：以前は失敗とした場合のうち、変更後は通す場合
3. 締める面：以前は通した場合のうち、変更後は失敗とする場合
4. 緩める面がある場合、守っていた性質が他の判定で引き続き守られているか（その判定の場所）
5. 根拠と証拠の場所

* 緩める面がない（締めるだけ、または同等）場合は、Agent が変更し、事後に 1〜5 を報告する。
  緩める面がないことは、旧判定が失敗とした入力（既存の失敗例と、旧判定が検出した突然変異）を
  新判定もすべて失敗とすることで示す。示せない場合は、緩める面があるものとして扱う。
  あわせて、新判定の検出力を突然変異検査で示す。
* 緩める面がある場合は、1〜5 を示してユーザーの承認を得る。
  承認を求める前に、突然変異検査と、提案した Agent とは別の審査者による検証
  （1〜5 の記述を実際のデータで確かめる）を行う。
* Design Invariants の守衛 Test は、緩める面の有無にかかわらず、1〜5 を示してユーザーの承認を得る。
* 閾値、試行回数、再試行の規則は §14.1 に従う。
  事前宣言した判定は、この節によっても、結果を見た後に緩めない。

---

## 15. Before Completing a Task

作業終了前に以下を確認する。

### Functional

* Build が成功する。
* 必要な Test が成功する。
* 要求された Feature が動作する。
* 変えた Test の判定と、切り分けた失敗の原因を報告した（§14）。

### Structural

* 不要な File 変更がない。
* 不要な Abstraction が増えていない。
* Dependency Direction が悪化していない。
* Ownership が曖昧になっていない。
* Product-specific Knowledge が公共層へ漏れていない。
* Editor / Runtime の Data Contract が壊れていない。
* Architecture Drift が発生した場合、それが明示されている。

### Product Fitness

変更が Product / Tool / Build / Packaging / Validation に関係する場合：

* Product を削除するために公共層変更が必要になっていない。
* Product の追加・複製が Registration と Product Content で完結する。
* Product-specific Test / Validation が owner 選択時だけ有効になる。
* 共通 Test が特定 Product の存在を前提にしていない。
* Owner 不明 File が残っていない。
* Invalid Reference が残っていない。

### Report

Architecture に影響があった場合のみ、最後に簡潔に報告する。

* Architecture Delta
* 新しい Dependency
* Ownership Change
* Product Boundary Change
* Data Contract Change
* 発見した Code Smell
* 将来 Refactoring を検討すべき点

Architecture に影響がなければ、
Architecture Report を無理に生成する必要はない。

---

## 16. Final Rule

Agent の役割は Architecture を完成させることではない。

依頼された Feature の実装、Architecture Review、Refactoring を進めながら、

* 既存 Architecture を尊重する
* 実際に発生した Pressure を観測する
* Architecture Drift を隠さない
* Ownership と Dependency Direction を維持する
* Product の独立性と Removability を維持する

ことが役割である。

不確実な設計を勝手に確定してはならない。

同時に、既存構造に明確な問題が現れている場合、
問題を隠すためだけに既存 Architecture を維持してはならない。

観測された問題を説明し、
必要最小限の Architecture Delta として扱うこと。
