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
* 公共層から具体的な Product 名を使って処理を分岐しない。
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

Code Style や Formatting の問題より、
Architecture / Ownership / Dependency の問題を優先する。

ただし、問題を発見しただけで
自動的に Scope 外 Refactoring を開始してはならない。

必要な場合は問題として報告する。

---

## 14. Before Completing a Task

作業終了前に以下を確認する。

### Functional

* Build が成功する。
* 必要な Test が成功する。
* 要求された Feature が動作する。

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

## 15. Final Rule

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
