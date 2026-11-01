# PvP v8：v8-product-anim-bottlenecks（Match 端姿勢的成本與決定性的量測）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 4～6 h，實際約 3 h。

收支：一致（四平台各 24 個姿勢 × 2000 次＝48000 次取樣、3 次載入、兩個雜湊各 2 次，全部完成；Windows 的記憶體照宣告記 n/a）

**狀態：完成。** 三項都照[事前宣告](../object_fps_pvp/plans/v8/v8-product-anim-bottlenecks-declaration.md)（`v8-product-anim-bottlenecks-declaration-approve`）判定，沒有觸發停止。證據：`anim-bottlenecks-2026-10-31.json`（使用者的 Intel Mac，主 checkout 的 `build/evidence/v8-anim-bottlenecks/`，git 管理外；判讀用的數字在下面的表）（CI run 41176350626 的四平台摘要）。

## 做了什麼

- 新的報告用程式 `tests/object_fps_pvp/AnimBottlenecks.cpp`（`gyo_object_fps_pvp_anim_bottlenecks`），CTest `object_fps_pvp.anim_bottlenecks`，`RUN_SERIAL`。載入路徑和 Client 相同；姿勢用 `SamplePlayerPresentationPose`（含武器，偏高的上限）。只在載入失敗或自我檢查失敗時不通過。
- `tests/object_fps_pvp/anim_bottlenecks.py`：B2 的記憶體。先跑 `--baseline`（只讀目錄），再跑 `--load-only`（一次完整載入），子程序峰值記憶體的差就是載入的增量；最後跑完整的報告。
- 平台名由 CMake 傳入（macOS 交叉建置用 `CMAKE_OSX_ARCHITECTURES`）。

## 結果

| 平台 | B1：12 次取樣（ms） | 最慢的姿勢 p99（µs） | B2：載入最慢（s） | B2：記憶體增量（MB） | H-load | H-pose |
|---|---|---|---|---|---|---|
| **linux-x64（判定）** | **0.44** | 37（aim −1） | **0.23** | **93** | `3f93c68b…` | `1706233a…` |
| windows-x64 | 1.89 | 157（move0_shoot） | 0.37 | n/a | `3f93c68b…` | `c17a2b4c…` |
| macos-arm64 | 3.91 | 326（reload_25） | 0.24 | 118 | `3f93c68b…` | `1706233a…` |
| macos-x64（CI，Rosetta） | 3.72 | 310（reload_25） | 0.66 | 119 | `3f93c68b…` | `dc754425…` |
| Intel Mac（本機） | 1.60～1.72 | 134～143 | 0.36～0.38 | 122～124 | `3f93c68b…` | `dc754425…` |

其他：`UAL1_Standard.fbx` 23.8 MB、`Player_V20.fbx` 0.8 MB；轉換後 15 個 clip 的 key 佔 2.85 MB。H-pose 涵蓋 29568 個 float（77 根骨頭 × 16 × 24），等於預期。

## 判定

- **B1：通過。** linux-x64 為 0.44 ms ≤ 1.67 ms → `v8-product-anim-authority` 不加省成本的工作。macOS 的 CI 數字（3.7～3.9 ms）超過門檻，但照宣告只記錄；同樣的姿勢，CI 的 macOS runner 比 linux-x64 慢約 8～9 倍；本機 Intel Mac 1.6～1.7 ms，也接近門檻。Match 跑在 Linux，這兩個平台不影響判定。
- **B2：通過。** 0.23 s ≤ 2 s、93 MB ≤ 200 MB → `v8-product-hitbox-data` 讓 Match 直接用 `UfbxModelLoader` 讀 FBX。
- **B3：有平台不同。**
  - 同平台兩次全部相同（`repeat_equal=1`）→ 「Match 可重現」在四平台都成立。
  - H-load 四平台相同；H-pose 有三組：linux-x64＝macos-arm64、windows-x64、macos-x64（CI 與 Intel Mac 本機相同）。所以不一致出在**取樣**（Slerp 等的浮點運算），不在 FBX 的載入轉換。
  - 不只 macos-x64 不同（Windows 也不同），macos-x64 的例外不適用。
  - → `v8-product-hitbox-golden-policy` 照計畫的建議：含動畫命中的情境只做同平台兩樹比對；跨平台 golden 只留不含動畫命中的情境。

## 經過（AGENTS §14）

- **第 1 次 CI（run 41175112356）**：四平台 L1 都在 `os.boundary_guard` 失敗。工具用了 `getrusage`、`sys/resource.h` 與平台巨集，違反 OS 層的 Design Invariants（R1～R3，[os.md](../architecture/os.md)）。改成 C++ 端不碰 OS、記憶體由 Python 包裝程式量。
  - **量法的變更（看過本機結果之後）**：宣告寫「同一個程序內以 `getrusage` 取峰值增量」，改成「基準與完整載入各一個程序，兩者峰值相減」。量的仍是載入增加的峰值記憶體，門檻與判定平台不變。這是第 1 次改量法，原因是守衛擋下，不是判不出來。
- **第 2 次 CI（run 41176350626）**：新工具四平台都跑完（上表的數字來自這次）；L1 仍在 linux-x64、windows-x64 失敗，失敗的是上一批的 `object_fps_pvp.hitbox_survey --smoke`（json 型別錯誤，兩台 Mac 通過）。
  - 原因：`NodesUnique` 對暫時物件 `Nodes()` 的 `items()` 做 range-for，暫時物件在迴圈開始前就被銷毀（未定義行為）。屬實作的缺陷（Test 工具本身）。
  - 本機（Intel Mac）重現不出失敗，所以沒有在本機看到 Red；CI 的兩個平台的失敗就是重現的證據。修正：先存成區域變數再迭代。
  - 順便修正：macos-x64 的摘要印成 `Darwin-arm64`（交叉建置時 `CMAKE_SYSTEM_PROCESSOR` 是主機的）；artifact 名是對的，數字的平台歸屬不受影響。
- **第 3 次 CI（run 41177444612）**：見下面的「L1」。

## L1

第 3 次 CI（run 41177444612）：四平台 L1 全部通過（linux-x64、macos-arm64、macos-x64 各 104／104，windows-x64 108／108），CI gate 通過。

這次也重跑了量測（不是判定用的試行，只當重現性的參考）：linux-x64 的 B1 0.41 ms、B2 0.20 s／93 MB；四平台的 H-load、H-pose 和第 2 次完全相同。macos-x64 的平台名已正確印成 `Darwin-x86_64`。

## 範圍的限界

- B1 的數字來自 CI 的共用 VM，不是 Linux 實機；量的是 Client 現在的取樣函數（含武器）。`v8-product-hitbox-acceptance` 在 Match 上再量。
- B3 只說明「取樣在平台間不逐位元一致」，沒有找出是哪個運算；之後若需要跨平台 golden，再從取樣的數學下手（不在本線範圍）。
