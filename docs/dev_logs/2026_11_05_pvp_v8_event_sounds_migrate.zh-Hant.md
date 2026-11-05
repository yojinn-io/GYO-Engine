# PvP v8：v8-product-event-sounds-migrate（換彈聲改讀 framework 的 gyo.event_sounds）

日期：2026-11-05。分支 `claude/pvp-v8-after-pv8`。主會話 Opus 5.5／high；審查者子 agent Opus 5.5／high（general-purpose，唯讀）。估時 4～6 h，到停下時約 1 h。

**狀態：程式與本機判定完成；E4 的 L1 與 E6 的實聽未做。** E5 的 D184 範圍有 1 條突變活下來，這條在 master 上本來就活著（見「判定」）。照 AGENTS.md §14.1 先回報，沒有重跑也沒有改它。使用者（2026-11-05）：「這個問題開一個fable模型的fix card來處理。」這條交給 fix 會話（`fix-product-use`，Fable 5.1，自 master 開分支），這一批照常提交。宣告與修訂紀錄：[事前宣告](../object_fps_pvp/plans/v8/v8-product-event-sounds-migrate-declaration.md)。

## 做了什麼

- 資料：
  - 新檔 `weapons/nailer_v2/viewmodel/viewmodel.event_sounds.json`（`gyo.event_sounds` v1：`magazine_in` → `gun_reload`、sync 0.33、gain 0.8），登錄為 `text`；
  - `nailer_v2_viewmodel.json` 加 `event_sounds_asset_id`；
  - `sfx.json` 升第 3 版，刪 `animation_events` 段與 `reload` cue。
- 程式：
  - `src/App/WeaponEventSounds.cpp`（在 `app_support`）：解析並核對 anim_events id、綁定、排程 `ScheduleEventSound`（呼叫 framework 的 `EventSoundOffsetSeconds`）；
  - `WeaponPresentationDefinition` 讀 event_sounds；
  - `PvpSound` 在武器載入後綁定，片段用混音器的執行期載入（16 格），並播放事件聲音；
  - 換彈段改呼叫上面這些；
  - `SoundCues` 刪掉自己的事件對應、偏移函式與 `reload` cue。
- 建置：`app_support` 改成 PUBLIC 元件 `FRAMEWORK`，PRIVATE 連 `GYO::ToolFrameworkJson`。Match 不連 framework。
- log 與診斷（`-log` (a)）：
  - `PvP sound summary` 刪 `reload=`、行尾加 `event_sounds=`；
  - `sound-cues.csv` 的事件聲音記成 `event.<事件名>`。
- 素材命名：`test_asset_catalog.py` 的定義檔後綴加 `.event_sounds.json`，`assets.zh-Hant.md` 第 3 節同步。這條是一般測試，不是 Design Invariant，放寬的只有這個後綴。
- 測試：
  - `presentation_cpu` 加 E1～E3：經正式的 `LoadWeaponPresentationDefinition`，E3 用真實的換彈片段長度；
  - 加 `WeaponEventSoundsTests.cpp`：用真實檔案和真實 `gun_reload` 長度測綁定與拒絕；
  - 刪 `SoundCuesTests` 的偏移案例與 `animation_events` 拒絕案例，去向見宣告的表。

## 判定

| 項 | 結果 |
|---|---|
| E1 產品資料能綁定 | 通過（`presentation_cpu` 與 `WeaponEventSoundsTests`；E 開頭 5 案例 100 斷言） |
| E2 壞資料被拒 | 通過（另一個 anim_events id、未知事件、sync 超過聲音長度、未知聲音、event_sounds 版本 2；`sfx.json` 版本 2 與帶 `animation_events` 段） |
| E3 聽到的不變 | 通過：開始時刻等於舊公式、gain 0.8；`apps/object_fps_pvp` 裡 `EventSoundOffsetSeconds` 的定義 0 件 |
| E4 | 本機 `cpu|shader` 115／115 通過；**L1 未跑**（還沒推送） |
| E5 | 本批 9 條全部 killed。**D184 範圍 24 條中 23 條 killed，1 條 survived：`v8-asset-layout-guard-product-use`**（不成立；使用者決定交給 fix 會話） |
| E6 實聽 | **未做**（要使用者） |

`v8-asset-layout-guard-product-use` 的事實：
- 突變的 `file` 是 `test_asset_catalog.py`，這批為了加後綴改了這個檔，所以落進 D184 範圍。
- 這批改的那一行和突變的 find（`PRODUCT_ROOTS = ...`）無關。
- 它在 hitbox-line 的 K3 時已經 survived，master 上也一樣：主 checkout 的 `build/evidence/v8-hitbox-acceptance-mutations/master-10.out`、`full.out`。
- 列在 HANDOFF「6 條過時或存活的突變另開一批」裡。

## 經過

- 量測前修訂了 7 點做法，判定不變。寫在宣告的修訂紀錄。其中第 1 點：第一版開了新的產品目標 `event_sounds`，審查指出理由之一（「`cpu` 測試連不到 `app_support`」）是錯的，撤回，改放 `app_support`。
- 第一次全量 CTest 有 2 個失敗，都在預期內：
  - `asset_catalog`：新檔名不合命名規則，加後綴後通過；
  - `mutation_tool`：6 條舊突變的 find 已經不存在，退休後通過。
  記錄保留在 `ctest_cpu_shader_1_before_naming_and_mutants.txt`。
- 第一輪突變（審查前，32 條）：新 8 條全部 killed，D184 23／24（同一條 survived）。保留在 `run1/`。審查後程式有改，以最終程式重跑 33 條：32／33，結果同上。
- 審查：0 個阻擋；2 個重要（理由錯誤、沒有走正式載入路徑的測試），都已處理；小問題與 nit 的處理見宣告修訂第 7 點。

## 證據

主 checkout 的 `build/evidence/v8-event-sounds-migrate/`（Intel Mac）：
- CTest 輸出：`ctest_cpu_shader_*.txt`；
- E 案例的輸出：`e1_e3_cases.txt`；
- 偏移函式定義的 grep（0 件）：`e3_grep_offset_definitions.txt`；
- 最終突變的輸出、報告與負載：`mutations_final.txt`、`mutations_final.json`、`mutations_final_load.txt`；
- 第一輪：`run1/`。

審查者的原始回報沒有保存，結論記在宣告的修訂紀錄第 7 點。

## 範圍的限界

- `PvpSound` 的綁定與播放、`PvpApplication` 的呼叫處沒有單元測試，由 E6 實聽與審查守。
- event_sounds 的錯誤在圖形階段才報，以前 `sfx.json` 的錯誤在內容階段報。
- framework 的 `docs/framework/event-sounds.md` 第 118 行這批之後就過時了，只回報，不在這批改。
