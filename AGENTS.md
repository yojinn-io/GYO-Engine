# AGENTS.md

# GYO-Engine Agent Rules

このリポジトリは現在、アーキテクチャを探索・進化させながら開発している。

現在の構造は最終設計ではない。
ただし、不確実性を理由に Agent が独自に大規模な再設計を行ってはならない。

---

## 1. Core Principle

機能実装では、既存アーキテクチャを理解した上で
必要最小限の変更を優先する。

MUST:

- 既存コードを確認してから変更する。
- 既存の責務（Ownership）と依存関係を確認する。
- Feature に直接必要な変更を優先する。
- 変更範囲をできるだけ局所化する。
- 既存の設計意図が不明な場合、それを「誤り」と仮定しない。

MUST NOT:

- Feature 実装を理由に、無関係なコードを整理しない。
- 単一の Use Case だけを根拠に新しい汎用 Abstraction を作らない。
- 将来必要になるかもしれないという理由だけで設計を一般化しない。
- 明確な根拠なしに既存 Module の責務を移動しない。
- 見た目を綺麗にするためだけの Architecture Refactoring を行わない。

---

## 2. Architecture Is Allowed to Evolve

Architecture の変更自体は禁止しない。

新しい Feature によって既存構造に実際の問題が発生した場合、
Architecture を変更してよい。

ただし Architecture Change は、
実際に観測された Implementation Pressure に基づかなければならない。

Valid evidence:

- Responsibility の重複
- Ownership の曖昧化
- Dependency Direction の悪化
- 循環依存
- Lifecycle の衝突
- 同じ Adapter / Conversion の繰り返し
- 複数箇所に現れる同一 Special Case
- Module Boundary を越えた内部知識の漏洩

Architecture Change SHOULD solve observed pressure,
not predicted pressure.

---

## 3. Architecture Drift Control

Feature 実装中に Architecture が変化する場合、
その変化を暗黙に行ってはならない。

次の変更が発生する場合は Architecture Delta として扱う:

- 新しい Top-level Directory
- 新しい Subsystem
- Module Ownership の変更
- Dependency Direction の変更
- Public Interface の大規模変更
- 複数 Module にまたがる Responsibility 移動
- Build Graph / CMake Module 構造の変更
- 大規模な File Move

Architecture Delta が必要な場合、変更理由を明確にする。

最低限、以下を説明する:

1. どの Feature が変更を必要としているか
2. 現在の構造で何が問題になっているか
3. どの Architecture Boundary が変化するか
4. 影響する Module
5. より小さい変更で解決できない理由

---

## 4. Reversibility

設計がまだ探索段階であるため、
同等の選択肢がある場合は Reversible な変更を優先する。

Prefer:

- Local Adapter
- Existing Interface Extension
- Small Vertical Slice
- Local Data Transformation

over:

- 新規 Subsystem
- 大規模な Hierarchy
- 多数の File Move
- Repository 全体に影響する Abstraction

ただし、一時的な workaround を永続設計として固定しない。

---

## 5. Dependency Rules

既に明確になっている Dependency Invariant は MUST preserve する。

例:

- Lower-level infrastructure MUST NOT depend on application-specific code.
- Application-specific code MAY depend on reusable engine infrastructure.

新しい Dependency Edge を追加する場合、
既存の Dependency Direction を確認する。

循環依存を新しく作ってはならない。

---

## 6. Top-level Structure

Top-level Directory は Architecture Concept とみなす。

新しい Top-level Directory を追加する場合、
単なるファイル整理ではなく Architecture Change として扱う。

Feature 固有データや単一 Asset Type を理由に、
安易に新しい Top-level Domain を作らない。

---

## 7. Refactoring

Refactoring は禁止しない。

ただし Feature 実装と無関係な Refactoring を同時に行わない。

Refactoring SHOULD:

- 現在観測されている Code Smell を解消する
- Responsibility を明確にする
- Dependency を単純化する
- 重複を削減する
- Feature 実装後に明らかになった構造上の問題を解消する

Refactoring MUST NOT:

- 「将来便利そう」という理由だけで抽象化する
- Repository 全体を一度に再構成する
- Feature Scope を不必要に拡大する

---

## 8. Architecture Fitness Checks

Architecture を固定するのではなく、
既知の重要な性質を監視する。

変更後に最低限確認する:

- 新しい循環依存が発生していないか
- 不自然な逆方向 Dependency が追加されていないか
- Top-level Architecture Concept が増えていないか
- 既存 Module の Responsibility が意図せず拡大していないか
- Feature-specific Knowledge が reusable engine layer に漏れていないか
- CMake dependency graph が意図せず変化していないか
- 同じ Responsibility が複数 Module に分散していないか

Architecture Fitness Check の失敗は、
必ずしも変更禁止を意味しない。

Review が必要な Architecture Delta が発生したことを意味する。

---

## 9. Code Review Rules

Review ではコードの正しさだけでなく、
Architecture Drift を確認する。

特に以下を指摘する:

- 新しく生まれた Responsibility
- Responsibility の移動
- 新しい Dependency Edge
- 新しい Architectural Concept
- 一時的処理が Architecture に昇格している箇所
- Existing Concept と重複する新しい Abstraction
- Feature Scope を超えた変更

Code Style や Formatting の問題より、
Architecture / Ownership / Dependency の問題を優先する。

---

## 10. Before Completing a Task

作業終了前に以下を確認する:

### Functional
- Build が成功する
- 必要な Test が成功する
- 要求された Feature が動作する

### Structural
- 不要なファイル変更がない
- 不要な Abstraction が増えていない
- Dependency Direction が悪化していない
- Architecture Drift が発生した場合、それが明示されている

### Report

Architecture に影響があった場合のみ、最後に簡潔に報告する:

- Architecture Delta
- 新しい Dependency
- Ownership Change
- 発見した Code Smell
- 将来 Refactoring を検討すべき点

Architecture に影響がなければ、
Architecture Report を無理に生成する必要はない。