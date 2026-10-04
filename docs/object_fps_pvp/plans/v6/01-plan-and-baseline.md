# 第 01 批：計畫、分批與基線

狀態：文件完成，2026-10-04；等 PR 的 CI。先讀 [進度](README.md)、[交接](HANDOFF.md)。

## 目標與範圍

把 v5 延後的項目變成有驗收點、有 owner、有檔位的批次，並保存 v6 的起點。
**本批不改任何程式、proto、資產、建置或測試**，也不跑 GUI、真網路矩陣或長測。

## 做法

1. 盤點（2026-10-04）：兩個 Explore agent 在 master `05042fa` 上逐項確認 HANDOFF 第 1～11 項的現況。
   結果與更正寫進 [交接](HANDOFF.md) 和兩個 Engine 計畫夾。
2. 分批（ultracode，使用者核准）：
   - 3 個分批方案並行（high）：地基先行、垂直切片、以協議為中心。
   - 1 位評審（xhigh）：以「以協議為中心」案為主幹合成推薦案，並移植另兩案的優點，例如量測基線批、子批拆分、缺陷提前。
   - 1 次依 AGENTS.md 的對抗式檢查（xhigh）：6 個 major、12 個 minor，全部處理。
     最重要的一項是查證 SDL 3.4.0 的 Metal 後端：即使用不阻塞的 acquire，仍會呼叫最多阻塞約 1 秒的 `nextDrawable`。
     因此第 3 項改為「先拆開量測，再選修法」。
3. 使用者決定：見 [交接](HANDOFF.md) 的決策紀錄（D0～D13；D12、D13 在審查後追加）。
4. 落盤：以一份最終規格為單一依據，並行撰寫批次文件與 Engine 計畫，再由兩位審查者檢查一致性、事實與 AGENTS.md。

## 交付

- 本產品：[進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md)、第 02～14 批文件、[v6 契約骨架](../../protocol-v6.zh-Hant.md)。
- Engine：[輸入與呈現](../../../architecture/plans/input-and-present/README.md)、[基礎後續整理](../../../architecture/plans/foundation-followups/README.md)。
  這兩個計畫夾是第 3、11 項 Engine 部分，以及第 10 項 Engine／工具／測試／登錄列的正式來源。
  math-foundation HANDOFF 與 2026-10-04 健檢中原本指向本產品 HANDOFF 的連結，改為指向它們。
- [v5 README](../v5/README.md) 的接續文字改為指向本計畫。
- dev_log：[2026-10-04](../../../dev_logs/2026_10_04_pvp_v6_batch01.zh-Hant.md)。
- 忽略的證據（`build/target/_build/test/logs/`）：
  - `pvp-v6-batch01-20261004/`：建置、CTest、v5 證據雜湊。
  - 從 v5 worktree clone 的 10 個 v5 證據目錄（見 BASELINE）。

## 驗收點

| 層 | 內容 | 結果 |
|---|---|---|
| L1 | test preset 建置與全部 CTest | 54／54 通過，97.64 秒 |
| L1 | v5 證據複製後逐檔雜湊核對 | 1,325／1,325 相同 |
| L1 | v6 與兩個 Engine 計畫夾的相對連結全部存在（腳本檢查） | 315 個連結，0 失效 |
| L1 | HANDOFF 第 1～8、10、11 項的每個子項都有批次，或標「候選／不做／已解決」 | 審查者逐列核對 25 列，全部有落點 |
| L1 | `docs/architecture`、`docs/checkup` 沒有指向 `docs/object_fps_pvp` 的連結（D12） | 0 個 |
| L1 | `git diff --stat` 只有 docs | 是 |
| L1 | CI | PR [#39](https://github.com/yojinn-io/GYO-Engine/pull/39) 執行中 |
| L3 | 使用者確認分批與決定 | 2026-10-04 確認 |

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 建置與 CTest 已執行（只有 CPU 與 `render.sdl_gpu_mesh_smoke`） |
| CI 四平台 L1 | PR 時執行 |
| Linux lavapipe GPU | 未執行（本批只改文件；CI toolchain 列的共通 `render.*` 照常執行，不作為本批驗收依據） |
| Windows D3D12 實機 | 未執行（本批不需要） |
| Linux 實機 | 未執行（本批不需要） |
| macOS arm64 實機 | 未執行（本批不需要） |

## Architecture Delta

無。本批只新增與修改文件。新增的兩個 `docs/architecture/plans/` 子資料夾沿用 math-foundation 與 result-unification 的先例，不是新的 Top-level Directory。

## 完成條件與停止

- 上表的 L1 全部完成，PR 的 CI 通過。
- 更新 README、HANDOFF 與 dev_log，記下 PR 編號後停止。
- 不自動開始第 02 批或任何 Engine 批次。
