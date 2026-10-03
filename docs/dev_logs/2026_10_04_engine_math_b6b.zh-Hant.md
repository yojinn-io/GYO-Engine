# Engine Math 基礎統一 B6b：pvp 表現層改用 GYO::Math

日期：2026-10-04。Owner：`object_fps_pvp`（app_support 與 `SnapshotTimeline`）。
狀態：CI 四平台通過，PR [#25](https://github.com/yojinn-io/GYO-Engine/pull/25) 待使用者合併（分支 `claude/math-foundation-b6b`，疊在 B6a 分支 `a9a5e85` 上）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b6b-pvp-表現層)，本文只記經過。

## 經過

1. 使用者在 B6a 進行中暫離，指示把剩下的批次照計劃做完：每批 commit、開 PR、跑 CI，有問題就修正。指示中沒有合併，所以 B6b 疊在 B6a 的分支上開 PR，等使用者回來決定。
2. 盤點在 B6a 驗證期間進行：2 個 agent 分別盤點已編譯的非 `match_domain` 程式，以及 acceptance 與測試，再由 1 個 agent 檢查遺漏。
   - runtime_host、ipc、client_network 沒有向量或角度運算。
   - acceptance 與測試的量測計算是獨立 oracle，決定維持原寫法。
3. 實作：
   - 逐位元相同的替換：插值、角度、min／max、`IsFinite`、牆的中心與尺寸、FOV 常數。
   - 會漂移的四項：
     - muzzle 與第三人稱武器位置改用 renderer 的 `ComposeEulerXYZ`；
     - mount 四元數改用 `Math::Normalize`；
     - double 的 `hypot` 改用 `Length`。
   - 新增 characterization 測試，以及正式內容的比對。characterization 共用的 helper 移到 `CharacterizationSupport.hpp`。
4. 驗證（ultracode，2 個 agent）：
   - **表現層 digest**：isolation 版在約 4190 萬筆資料上與 base 逐位元相同。base 對 branch 的差異只在四項漂移，locomotion 的離散狀態都相同。
   - **對抗式審查**：沒有推翻正確性，只指出測試的 1 個 minor 與 1 個 nit，都已處理。
   - digest agent 另外發現：測試輸入的 `std::pow(10, x)` 在 clang -O2 被換成 `exp10`。改用執行期的底數。
5. 本機驗收：test 46／46、gateway Go 測試通過；未編譯 29 檔 syntax-only 全部通過。

## 留給下一步

- PR 與 CI 四平台驗收。
- B6c：29 個未編譯檔的 helper 遷移。盤點已在 B6b 進行期間完成。
