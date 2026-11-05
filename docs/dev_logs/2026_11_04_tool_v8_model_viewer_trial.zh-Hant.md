# 工具 v8：v8-tool-model-viewer-trial

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146）。

- 段：下段（執行與記錄）。主會話 Opus 5.5。
- 估時：L2 約 1 h。實際約 40 分（含寫量測命令）。

## 做了什麼

- 新增量測命令 `gyo_model_viewer measure-preview`（只在 gui 變體）：每幀量一次「取樣、`ModelRenderer` 更新、`Renderer::Render`、讀回、上傳」，每幀一列 CSV，最後印出每個模型的 p50／p95／p99／max。
- 寫了 L3 的試用步驟：[model-viewer-trial.md](../tools/plans/v8/model-viewer-trial.md)。

## 驗證與經過

- 件數の突き合わせ：一致（P1 每個模型 600 筆，P2 的事件檔 1 個）。
- **P1 通過**（試 1 次，沒有重試）：每個模型的 p95 都 ≤ 6.0 ms，最大是 5.957 ms（`ual1_mannequin`），門檻是 16.7 ms。
  - 建置是 RelWithDebInfo，不是宣告的 Release。最佳化不比 Release 多，所以結果只會偏慢。
  - 有兩個模型沒有片段，用預設姿勢量。
- **P2 通過**：PvP 唯一的事件檔往返後逐位元組相同。
- **L3**：你試用後的結論是「model viewer 可以沒問題了」。逐項結果沒有回報，所以只記這句結論。
  - 第 3 項的存檔，從工作樹的檔案看得到有作用（`magazine_in` 2.35 → 1.8870078722635908 s）。你決定保留（「保留 1.887」），已提交。
  - 第 5 項「遊戲裡聽到」做不到：PvP 不讀 `gyo.event_sounds`。
  - audio_editor 的部分，等你決定它的邊界之後再試。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/trial/`，有：
  - `p1-frames.csv`、`p1-summary.txt`、`p2-roundtrip.txt`、`commit.txt`；
  - `l3-window.txt`。

## L1 的失敗與修正（2026-11-05）

- L1（run 41852235377，`e9d7ee7c`）的 linux-x64 與 windows-x64 失敗：`os.boundary_guard` 指出 `PreviewMeasure.cpp` 用了 `std::ofstream`（docs/architecture/os.md 的 R4：工具的檔案存取要經 engine/io）。
- 原因：加了 `measure-preview` 之後，我只跑了量測，沒有跑整套 `cpu|shader`，所以本機沒抓到。實作的錯，守衛是對的。
- 修正：CSV 先在記憶體裡組好，最後用 `WriteReplacing`（engine/io）一次寫出。量測的內容與計時的範圍都沒變；P1 的數字是修正前量的，只有寫檔的方式不同。
- 修正後：本機 `cpu|shader` 119／119 通過；用 5 幀試跑，CSV 的列數正確。

## 合併前的完整突變（D184，2026-11-05）

- 件數の突き合わせ：一致。model_viewer 41 條（貼圖的 3 條用 dummy 視訊驅動加軟體渲染器跑），framework 外殼 33 條，共 74／74 killed。
- 經過：
  - 第一次 model_viewer 的部分是 0／0。我在 zsh 裡傳 id 清單時沒有拆成多個參數，結果一條都沒選到；不是突變的結果。改正後重跑。
  - 重跑後 `mvp-speed-twice` 是 stale：`-events` 改了 `Advance` 的那一行，卻沒有重跑這條（D184 的「find 在改動範圍」我漏算了）。更新 find 後 killed。
- 證據：主 checkout `build/evidence/tool-v8-model-viewer/pre-merge/`。

## 發現（不在本批範圍）

- PvP 讀 `gyo.event_sounds`，遊戲裡的換彈聲才會跟著工具配的聲音變。這是 PvP 那一側的候選工作。
