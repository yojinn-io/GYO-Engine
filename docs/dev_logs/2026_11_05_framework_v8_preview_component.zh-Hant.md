# framework v8：v8-framework-preview-component（3D 預覽元件抽到 framework，model_viewer 改用）

日期：2026-11-05～06。分支 `claude/v8-framework-preview-component`（基底本機 master `54948874`）。上段（framework 公開介面、Design Invariants、守衛正向清單）。
實作者：主會話，前半 Fable 5.1／high、後半 Opus 5.5／high（計畫指定 Opus；開工時沒有照計畫提出，見 HANDOFF）。審查者：Opus 5.5／high 子 Agent（唯讀）。§14.4 検証者：Opus 5.5／high 子 Agent。估 7～9 h，實際約 1 h（不含等待使用者）。

**狀態：** 完成。宣告（2026-11-05）與守衛的判定變更（2026-11-06，§14.4，含三種擋不住的寫法）都由使用者核准。L1 的結果在 PR 上看。

件數の突き合わせ：一致（P1 每個模型送 600 幀、記錄 600 筆，合計 2400，沒有計數的丟棄。突變：宣告 56、執行 56；第一次 55 killed＋1 wrong_failure，使用者核准修法後重跑 3/3；補強與修正後各重跑 configure 判定的突變 10/10、11/11；最終本批 25/25、D184 34/34，stale 0）

相關文件：[計畫與事前宣告](../architecture/plans/framework-preview/README.md)、[HANDOFF](../architecture/plans/framework-preview/HANDOFF.md)、[契約](../framework/preview.md)、[守衛的判定變更](../architecture/plans/framework-preview/guard-judgment-change.md)。

## 做了什麼

- **framework**：新 target `GYO::ToolFrameworkPreview`（核心，只在 Engine 的 SDL GPU backend 與 SDL renderer 都在時建）。
  - `Preview.hpp`：`OffscreenPreview`（`Create`、`BeginFrame(width, height, camera, clearColor)` → `RenderQueue*`、`EndFrame` → `TextureReadback`、`Device()`）。
  - `PreviewTexture.hpp`：`UploadFrame`（依幀尺寸建、換或沿用 SDL renderer 的串流貼圖；只收 `Rgba8SRgb`）。
  - `PreviewCamera.hpp`：`OrbitCamera`。
  - `FrameworkError.hpp`：`PreviewErrorCode`（`StartFailed`、`DrawFailed`、`UploadFailed`）。
- **model_viewer**：`PreviewRenderer` 只留 `ModelRenderer`、骨架細盒、`FrameModel`，`Render` 改成帶尺寸；`Resize`、`OrbitCamera`、`PreviewTexture`、`UploadFrame` 刪除，不留相容層。視窗、`measure-preview`、smoke 改用 framework 的。
- **測試**：`tests/framework/PreviewTests.cpp` PV1～PV6（PV1～PV4 標籤 gpu、PV5～PV6 cpu）；configure 時「有 Engine target 卻沒建 preview」就失敗。
- **守衛**：framework 守衛的核心清單加 4 個名字，另加每個核心 library 自己的連結清單（讀 genex、去引號、以變數定義 library 也算發現）；configure 時加「核心與視窗外殼連不到 render target」的檢查。model_viewer 守衛換成 `GYO::ToolFrameworkPreview`、拿掉 `GYO::Render`、`GYO::RenderBackendSDLGPU`。
- **突變**：`docs/architecture/plans/framework-preview/scripts/framework_preview_mutations.json` 25 條。既有改 7 條（`fw-guard-unlisted-engine-target`、`fw-shell-core-render`、`mvp-stale-frame`、`fw-guard-core-ui-dropped`、`es-guard-audio-backend-listed` 的錨點或預期；意圖不變）。
- **文件**：契約 `docs/framework/preview.md`（DI 1～5、Replacement Criteria 1～4、Layers 檢查 2 的表）；`docs/framework/architecture.md` 只在 tool part 與 Using it 加用法；`docs/model_viewer/architecture.md` 的 Window 節。

## 驗證與經緯

- **Engine 缺口檢查**：unlit（`MaterialDesc.shader` 預設 `builtin/unlit`）、順時針為正面（`PipelineDesc.clockwiseFrontFace`）、非 `doubleSided` 剔背面都是 Engine 的預設，沒有缺口，Engine 不改。
- **建置與測試**（使用者的 Mac，worktree 的 `build/target/_build/test`，RelWithDebInfo）：全量 `cpu|shader` 125/125、`gpu` 4/4（Metal）；審查後 framework 與 model_viewer 的 17 個測試全過；configure 時的新檢查在實際樹上通過。
- **審查**（Opus 5.5）：0 blocker、3 major、4 minor、4 nit，都照建議處理：
  - major 有三件：
    - 突變的預期訊息漏了 model_viewer 先觸發的 FATAL_ERROR。
    - 每個 library 的清單讀不到變數或 genex 的寫法。守衛改成讀 genex，另加 configure 時的檢查。
    - model_viewer 守衛的變更也要走 §14.4：宣告原本寫「收緊、事後報告」，是錯的。
  - minor 有四件：PV6 補 yaw＋pitch 的案例；`UploadFrame` 檢查格式；契約寫明 `EndFrame` 一定要呼叫；宣告 PV7 的一條突變換成兩條守規則本身的突變。
- **守衛的 Red／Green**：framework 的舊守衛跑新自測 54 條中 11 條失敗，新守衛 54/54。model_viewer 的舊守衛跑新自測 15 條中 2 條失敗，新守衛 15/15。
- **宣告的突變判定不成立一次**（§14.1，先回報、再依使用者指示）：第一次 55/56。`fw-guard-window-render-genex` 是 wrong_failure：突變把 genex 寫在引號裡，守衛連引號一起報出來（有擋住，不是漏檢）。使用者「照參考修法」：守衛判斷前去掉引號，自測加 1 條；重跑這條與錨在同幾行的 2 條，3/3。
- **P1 重跑**（model_viewer 計畫的原宣告：PvP catalog 4 個 model、各 600 幀、1280×720、試 1 次、p95 ≤ 16.7 ms）：**通過**。

  | 模型 | p95（本批） | p95（#146） |
  |---|---|---|
  | `ual1_mannequin` | 7.454 | 5.957 |
  | `player_v20` | 5.252 | 4.726 |
  | `nailer_v2` | 5.663 | 5.214 |
  | `nailer_v2.world` | 4.067 | 4.150 |

  mannequin 多 1.5 ms；試 1 次分不出是變更還是機器狀態，不當判定。機器：Intel i9-9980HK、macOS 26.7.1、Metal、RelWithDebInfo（和 #146 相同）。
- **検証者**（Opus 5.5，3 輪）：
  - 第 1 輪：A1、A2、B1～B5 確認；A3～A5 部分確認（收緊與 configure 檢查的涵蓋說過頭、數字過時）。沒有文件沒寫到的放寬。
  - 使用者選「先補強再核准」：configure 檢查改成也讀 INTERFACE 連結、跟進 genex 裡具名的 target。
  - 第 2 輪：不成立。補強造成回歸（用實名直接連的 target 不跟進，因為看過的 entry 和展開過的 target 共用一張清單）；我的突變都走別名或 genex，沒發現。依 §14 停下，使用者選「修正並重新檢證」：清單拆成兩張，加突變 `fw-link-window-preview-variable`。
  - 第 3 輪：部分確認。回歸已修；另有名字含 `-`／`.` 的輔助 target、經變數的 archive 路徑或 `-l` 兩種繞法，寫進限界（configure 檢查已改兩次方法，不追第三次）。

- **L1 第一次**（run 42067582426）：linux-x64、macos-arm64 通過；windows-x64 編譯失敗（PreviewTests.cpp 的變數 `far` 和 `windef.h` 的巨集撞名，測試自身的錯）。改名為 `distant`，判定不變，PV6 的 4 條突變重跑 4/4。第一次的失敗保留在這裡（§14.1）。

## 和宣告不同的地方

- 基底：宣告寫 `a9ccad7d`，實際是本機 master `54948874`；PR 前併入 master（本批相關檔案沒有重疊）。
- PV7 的突變：`fw-guard-preview-target-dropped` 沒做，改成 `fw-guard-link-list-ignored`、`fw-guard-link-list-missing-ignored`（守規則本身）；審查後另加 5 條。
- model_viewer 守衛的變更從「事後報告」改成關卡 2 的 §14.4 核准對象。
- preview 明確連 `GYO::PlatformSDL`（標頭用到它；審查 nit）。

## 範圍的限界與留下的事

- `mvp-stale-frame` 在 model_viewer 的突變目錄裡，卻錨在 framework 的 `Preview.cpp`（跨 owner）：framework 改那段時要一起改。
- 守衛不讀 `#[[ ]]` 多行註解、目錄層級的 `link_libraries()`、`set_property(… LINK_LIBRARIES)`。視窗外殼不連 render 這一面由 configure 時的檢查補上，但仍有三種寫法擋不住（經別人的 PRIVATE 連結、名字含 `-`／`.` 的輔助 target、經變數的 archive 路徑或 `-l`），見判定變更文件；之後的候選。
- `OrbitCamera` 今天只有 model_viewer 用（D-pv-1，使用者接受）；下次架構健檢重看。
- voxel 地圖編輯器是第二個使用者，它的 window 批在自己的線上做。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-preview/`（git 不追蹤）：
`build_*.txt`、`ctest_*.txt`、`guard_red_green/`、`mutations_*.txt`（含第一次的 wrong_failure）、`run_mutations.sh`、`p1/`（`p1-frames.csv`、`p1-summary.txt`、`commit.txt`）、`verifier/`。
wrong_failure 的重現在會話暫存區，未保存。審査者與検証者的原始回報未保存。
