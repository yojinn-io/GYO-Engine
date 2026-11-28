# 2026-11-28 頭頂 UI（名字與血條，`v8-arch-gyo2-overhead-ui`）

Owner：頭頂 UI 的任務卡。分支 `claude/gyo2-overhead-ui-f702b2`（PR #372，開向 `gyo2`）。作者 Opus 5.5／high。宣告：[overhead-ui-declaration.zh-Hant.md](../architecture/target/v2/overhead-ui-declaration.zh-Hant.md)（修訂 3，e32eaf02）。

## 前提（使用者的決定，經主會話轉達）

1. O1：敵方玩家不顯示血條，怪物顯示，參考 Overwatch。O2：名字用和房間同一個字串（之後 #376 換成 `PlayerLabel(userId, name)`）。
2. O3／O4：顯示名字、隊友也顯示；驗證用測試地圖裡的靶子。
3. O6 (b)：隊友只顯示名字；O7 (a)：靶子放 `pvp_arena`＋`sandbox`＋門的 scene 檔；O8 (a′)：`JudgeShots` 只記打到的實體。
4. 動工核准：使用者「開工」（HANDOFF `v8-arch-gyo2-overhead-ui-start-impl`）。#367（99e20ae3）、#376（1bfb527b）已合併並 merge 進這條分支。

## 各步

- **第 1 步**：`present/Overhead`（logic：`OverheadSettings`、`ParseOverheadSettings`、`HealthFraction`、`OverheadBarAlpha`）、`assets/object_fps_pvp/ui/overhead.json`（version 1，進 asset catalog）、`client/Overhead`（client：`PlaceOverhead`＝`WorldToViewport`→`ViewportToWorld2d`，隱藏原因 `NoView`／`Behind`／`OffScreen`／`Occluded`）。`LoadPresentation` 讀 `ui/overhead.json`。P1、P2（client 的純測試）、P3、P4、P5（logic）。
- **第 2 步**：Frame World 的 `Overhead`／`OverheadPart`、`OwnHits`、`ArenaWalls`（牆從 `ClientSound` 移出，聲音與名字共用；聲音行為不變）、`NoteOwnHits`、`TagPlayerOverheads`（名字＝`PlayerLabel(userId, PlayerName)`、沒有血條）、`ShowOverheads`（Main／PostUpdate，camera 的 `kUpdateSet` 與 transform 的 `kPropagateSet` 之後）。`PlayerSnapshot` 帶 `PlayerName`；`PoseCharacters` 把頭的中心留在 `ClientCharacters::heads`。C1～C3。
- 和宣告不同的地方：宣告第 11.1 節（九條）。
- 第一次跑全部 Client 測試時，單人的案例 assert：Solo World 沒有登記 `PlayerName`，`SnapshotOf` 照樣 `Get`。改成先看 `ComponentIdOf`。

## 突變（D184：新的、find 在改動範圍裡）

- 新增 15 個 `pvp-overhead-*`。第一輪 13 killed；`pvp-overhead-no-hold` 是 wrong_failure（打中當下那一條沒有標記），補上 `[pvp-overhead-hold]`；`pvp-overhead-own-labelled` survived：自己本來就沒有 `CharacterPart`，「不是自己」這個條件是多的，拿掉條件與這個突變（剩 14 個）。修正後 `pvp-overhead-no-hold` killed：14／14 killed。

## 檢驗（第 1、2 步）

- 本機（macOS，`test` preset）：全部建置通過；`ctest -L "cpu|shader"` 260 項全過。
- 沒跑：L1 三平台（PR 線完成時 dispatch）、實機看畫面（名字晃不晃、字的大小：交使用者 playtest）、設計審查與純淨 Code Review（合併前，第 5 步）。

## 停在第 2 步

- 第 3、4 步（靶子）要等門線（#375）的 `Placed`、scene、`sandbox` 合併；2026-11-28 查 `origin/gyo2`，三者都還沒有（門線最新是宣告的 85e15d27）。照主會話的指示停下回報，不自己發明 scene 格式。
