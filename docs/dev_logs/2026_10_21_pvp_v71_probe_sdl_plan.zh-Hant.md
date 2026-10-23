# PvP v7.1 worker-probe-repair、sdl-isolation-plan：修好 worker probe，排定 SDL 隔離的計畫

日期：2026-10-21。**完成**（worker-probe-repair：probe 修正 `d2844a5`、新突變 `5044702`、紀錄 `82d5f2b`；sdl-isolation-plan：草案 `b2024c3`、紀錄 `d0e5b57`、計畫核准 `56892f7`（D160））。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：尚未合併到 master。worker-probe-repair 與兩批的紀錄在記錄器分支 `claude/pvp-v71-after-re-evaluate`，worker-probe-repair 屬於 D159 的第二條 PR 線（系統其餘＋觀測＋測試）；計畫草案 `b2024c3` 在 PR [#85](https://github.com/yojinn-io/GYO-Engine/pull/85) 的分支 `claude/pvp-v71-sdl-isolation`。
批次表與決策點見 `docs/object_fps_pvp/plans/v7.1/after-re-evaluate.md` 第 9 節（記錄器分支）；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D158～D160（master 上的版本只到 D157，D158 以後在記錄器分支）。

## 做了什麼

- 背景（D159）：使用者核准 after-re-evaluate 的批次草案，決策點 C 選 (a)：把 worker-probe-repair 提到最前面。22 個 `worker` 突變中 20 個指向 `ClientConnection.cpp`，之後的批次會改到這個檔案，probe 壞著就判不了；決策點 B：sdl-isolation-plan 用 ultracode。
- worker-probe-repair：probe 在 HEAD 會失敗，原因兩個，都是 hitbox-fields（`91b0264`）加了契約欄位、mock 沒跟上，不是產品缺陷（Client 照 pv7 §7 拒收是對的）。
  - mock Welcome 沒填 `maximum_shot_damage`，Client 判成「Invalid Match combat rules」。
  - mock snapshot 的命中紀錄少了 `last_hit_part`；pv7 §7 要求 4 欄全 0 或全填，Client 把 snapshot 安靜丟掉，所以只看到「等待逾時」。
  - 第二個原因原本未知，照 §14.3 先建觀測：讓等待逾時的訊息帶出等待的行號，定位到「等 Playing」那一步。這個觀測留在 probe 裡。
- sdl-isolation-plan（ultracode，8 個 agent，約 38 分鐘）：盤點 2 位、獨立方案 3 個、評審合成 1 位、xhigh 公開介面審查 1 位（v7 D39）、對抗式檢查 1 次。產出 Engine 側 IP-3／IP-4 的 PLAN 草案（API、時間戳契約、日誌橋、子批、Design Invariants 與 Replacement Criteria、遷移清單、U1～U10），以及產品側的 `sdl-isolation.md`（工作清單、符號守衛、L2、估時、S1、S2）。

## 驗證與經過

- worker-probe-repair 的判定只收緊：probe 另外確認 Client 收到的最大傷害與命中部位。照 §14.4 事後報告，新突變 2／2 killed；本機 worker CTest 連跑 5 次 0 失敗；22 個 `worker` 突變重判 22／22 killed。四平台 L1 沒跑（worker 不在 CI 範圍，這次只改 probe 與突變目錄）。實際約 1 h（估 2～4 h）。
- xhigh 審查：有條件接受，三個設計前提成立，必修 6 項併入草案。
- 對抗式檢查的高嚴重度 3 項：時間戳的整合判定會被 clamp 吸收（突變殺不掉）；PR 結構和 v7 P4「一起合併」衝突，改成一個 PR；產品的符號守衛沒有範圍與突變，改成兩層並宣告突變。
- 另外發現 Ownership 漂移：產品的工作清單原本寫在 Engine 的 PLAN，違反 Engine 計畫的匿名規則，移到產品側的 `sdl-isolation.md`。
- 估時：Engine 37～46.5 h＋產品 19～26 h，約 56～73 h，是 after-re-evaluate 原估 26～34 h 的約 2 倍。照時間盒規則停下，交使用者決定。規劃本身約 1.5 h（估 4～6 h）。
- D160：使用者核准，接受新估時，各子批開始前重估、超過 1.5 倍就停；S1 選 (b)（保留 `Platform()`，只移除 `SdlGpuRenderDevice&` 的公開），S2 選「base 與頭各用自己的 probe」；設計寫成架構文件 `platform-sdl.md`、`input.md`（使用者要求）。
- sdl-isolation 的實作（ip3a 起）正在 PR #85 進行，它的開發日誌屬於那個 PR，不在本篇。

## 證據

兩批都沒有專用的證據目錄：worker-probe-repair 的結果記在 HANDOFF 與突變目錄（`tests/object_fps_pvp/mutations.json`）；sdl-isolation-plan 只改文件，審查紀錄在 `docs/architecture/plans/input-and-present/` 的 PLAN 與 HANDOFF（PR #85 的分支）。
