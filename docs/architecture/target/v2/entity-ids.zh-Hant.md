# 預設的實體配發器（`engine/modules/entity_ids`）

更新：2026-11-24（新頁，`v8-arch-gyo2-world-gen` 第 1 步）。由來：[事前宣告](world-gen-declaration.zh-Hant.md)（`v8-arch-gyo2-world-gen`，2026-11-24 動工前核准）第 5.10 節、第 7.3 節。**Design Invariants 的最終文字在合併前核准時定稿**。

Owner：架構。程式：`engine/modules/entity_ids/`（`GYO::Engine::EntityIds`，target `GYO::EntityIds`，登記單位 `EntityIdsModule`）。行為測試：`tests/common/entity_ids/`。

## 一個例子

卡牌遊戲的作者只登記元件 `Card`、寫一個發牌的 System（參數 `Spawn<Bundle<Card>>`），沒有宣告配發器。產品預設帶 `entity_ids`（`config/modules.csv` 的 `default=yes`），它在每個 World 登記 `RangeAllotter` 為那個 World 的配發器，第一次 Run 之前放好範圍 `[1, 2^32−1]`；發牌拿到的編號是 1、2、3……。PvP 的作者要「單調、不重用」——預設的就是。要別的策略（例：回收編號），就在那個 World 不選這個 Module、登記自己的配發器型別。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::EntityIds
```

- 登記檔一列：`library_only=no`、`default=yes`、`depends` 空（core、ecs、orchestration 不登記，link 寫在 `engine/modules/entity_ids/CMakeLists.txt`）。
- 標頭：`RangeAllotter.hpp`（型別與逐欄雜湊）、`EntityIdsModule.hpp`（登記單位與 `EntityIdsOptions`）。
- `RangeAllotter` 原本在 Kernel（`engine/kernel/detail/Storage.hpp`），2026-11-24 搬到這裡：Kernel 只留 `EntityAllotter` concept 與配發的機制，不附任何策略型別（Kernel DI-KS 4；宣告 5.10 的三個理由）。

## 2. 設定

`EntityIdsOptions{first = 1, last = 0xFFFF'FFFF}`，經 `WorldDescription::options` 給；沒給用預設。0 留給作者當「沒有實體」；`last` 讓鍵的世代那一半保持 0（Kernel DI-KS 4：低 32 位元是 index）。

讀檔照存檔編號放回時，在 `ReplaceRequest::extra`（或描述的準備函式）用 `SetResource(RangeAllotter::Of(存檔的下一號, 上限))` 蓋掉預設值：準備函式的順序是帶過去 → 登記單位的準備（這個 Module）→ 描述的準備 → `extra`（O-7），所以後面的會蓋掉預設值；這個 Module 只在沒有值時放。

## Design Invariants

守衛欄：行為測試（`entity_ids.*` 的 ctest 群組、突變 `ei-` 與搬來的三個）照常是守衛。綁實作方式的守衛（`gyo_expect_links`）在 scenario 第 6 段通過之後才立（AGENTS §3「ガードを立てる時期」；宣告第 9 節第 4 步）。

<a id="ei-1"></a>
**EI-1 職責。** 每個選了它的 World：登記 `RangeAllotter` 為那個 World 的配發器；準備函式在沒有值時放 `EntityIdsOptions` 的範圍（預設 `[1, 2^32−1]`），已經有（帶過去的）就不蓋。不做回收、回滾登記（作者的責任，回滾 R-2 前提 6）、網路的編號對映、存檔。Kernel 與編排層不認得它。同時選它又登記另一種配發器是 Kernel 既有的 RE2，不另加檢查。守衛：`entity_ids.module`「with entity_ids an author spawns without declaring an allotter」「each World allots from its own range」「a carried-over allotter is not replaced」、`entity_ids.default`（產品的登記單位表由真的 `config/modules.csv` 產生）；突變 `ei-prepare-overwrites`、`ei-not-default`。

<a id="ei-2"></a>
**EI-2 範圍配發器。** `RangeAllotter`：一個範圍 `[first, last]`，單調、不重用、用完回 `nullopt`（Kernel 報 `AllotterExhausted`）、不回繞、沒有 `Reset`；`Owns` 是範圍內。逐欄雜湊（padding 不進 World 摘要）。守衛：`entity_ids.allotter` 兩案；突變 `we-j8-allotter-wraps`、`we-j8-allotter-reset`、`ks-hash-allotter-as-bytes`（隨型別從 Kernel 的清單搬來，id 不變）。

<a id="ei-3"></a>
**EI-3 依賴。** 只依賴 orchestration、kernel、base；不依賴回滾、權威、時間。守衛：`gyo_expect_links`（第 4 步立；在那之前靠本頁與審查）。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」（L17 的形式：不數次數，出現就停）。

- **RC-EI1**：有點名的產品在一個 World 的生命內用完 2^32 個編號（要回收的策略：Bevy 的 free list＋世代，`bevy_ecs/src/entity/mod.rs:775-850`）。
- **RC-EI2**：Kernel 要正確必須知道配發器的型別。
- **RC-EI3**：兩個以上的產品各自寫同一種自訂配發器。
- **RC-EI4**：職責清單（EI-1）要多一項。
