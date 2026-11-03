# PvP v8：v8-product-hit-gui-aim（P2：GUI 驗收工具瞄軀幹）

日期：2026-11-01～11-03。分支 `claude/pvp-v8-after-pv8`。上段｜實装者 Opus 5.5／high。估時 2～4 h，實際約 6 h（含兩次停下、改用 Docker、找出 `aim()` 的競態）。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hit-presentation-declaration.md)的 P2（`v8-product-hit-gui-aim` (a)）。G5、G6 依 `v8-product-hit-gui-aim-l2` 移到 [hitbox-acceptance](../object_fps_pvp/plans/v8/v8-product-hitbox-acceptance-declaration.md)，再依 `v8-product-hitbox-g5g6-docker` 改在 Docker 跑。

## 做了什麼

- **`gui_aim.hpp`（新）**：GUI probe 對還活著的對手軀幹的瞄準，建在 `hit_aim.hpp` 之上。房間的 arena 必須是部署的 `pvp_arena.json`，不同就拋出例外。
- **`combat_latency.hpp`**：SDL 的 17 格排程每幀轉向對手軀幹（只有 `run_timing.py --gui --combat` 會用到，見限界）。
- **`native_window.hpp`**：
  - 狀態檔多一個 `torso_aim`；
  - 觀察用：每個玩家多記戰鬥紀錄（受傷次數、時間、部位）、蹲姿、動畫狀態，用來找出這次的原因。
- **`gui_main.cpp`**：`hit_aim.hpp` 在檔案層級 include（其餘的 probe 標頭是在匿名 namespace 裡 include 的）。
- **`run_native_window.py`、`run_gameplay_gui.py`**：
  - 射擊前轉向 `torso_aim`；
  - `aim()` 每次移動之後，等到 `mouse_consumptions` 增加、而且連續兩次讀到相同的 yaw／pitch，才量下一次。
- **單元測試**「the probes' torso aim is judged Body from every product spawn」：產品 arena 四個出生點兩兩組合，加上受擊動作中每個 Tick 的目標，用姿勢模型判定，全部是 Body（1033 個斷言）。presentation 測試的 include 路徑因此加上 `build/acceptance/object_fps_pvp`。

## 判定與經緯

1. **Mac 上不能跑**：G5、G6 的 runner 靠 X11／XTest。宣告寫「Intel Mac」是我沒查。停下，使用者決定移到 L2（`v8-product-hit-gui-aim-l2`），之後又提議用 Docker（`v8-product-hitbox-g5g6-docker`）。
2. **試行 1（Docker）**：
   - G5 失敗：`run_gameplay_gui.py` 有自己的 `aim()`，pitch 固定是 0，P2 漏改。原宣告對 G5 的描述也錯了：G5 是原生的 X11 操作，不是 SDL 的 17 格排程。
   - G6 的 V3、V4 通過，之後容器缺 `xwd`。
3. **試行 2**：補上 `run_gameplay_gui.py` 的瞄軀幹，image 加 `x11-apps`。
   - G5′ 的三槍判成腿。
   - G6′ 的 V3、V4 通過，之後缺 PIL。
   - 我當時推論「`hit_aim.hpp` 的瞄準點有缺陷」，這是**錯的**。
4. **觀察跑次**（加每幀的姿勢輸入，不算判定）：
   - probe 算出的 `torso_aim` 是 0.0625，正確；相機卻停在 0.125。
   - 原因是 `aim()` 送出移動後只等 80～100 ms 就讀狀態檔，在約 27 fps 時讀到舊值，同一個修正送了兩次。
5. **試行 3**：`aim()` 改成等移動被吃掉、角度穩定，image 加 PIL。
   - **G5″ 通過**：四槍都是軀幹，間隔 10、13、11 Tick；死亡、重生、重新加入都對。
   - G6″ 的 V1～V4、V6 通過，之後讀不到 `join-native-state.json`：輸出資料夾是 Docker 從 macOS 掛進去的 bind mount，probe 用改名換檔在這裡不是原子的。判為環境，照宣告停下。
6. **使用者決定改輸出位置後重跑 G6**：輸出寫在容器內，結束後再複製出來。**G6‴ 通過**，V1～V8 全部通過。

失敗的試行都保留：主 checkout 的 `build/evidence/v8-hitbox-l2-docker/`（`G5`、`G6`、`G5p`、`G6p`、`G5-observe`、`G5pp`、`G6pp`、`G6ppp` 與各自的 `.out`）。

## 環境（Docker）

- `build/evidence/v8-hitbox-l2-docker/` 的 `Dockerfile`、`build.sh`、`run_g.sh`：
  - ubuntu 24.04，套件和 CI 的 Linux 列相同，另加 openbox、x11-apps、python3-pil；
  - Xvfb 加 llvmpipe（軟體 Vulkan）；`ci-linux` preset 在容器內建置，`--parallel 4`（16 個平行時記憶體不足）。
- 量不到的：
  - 實體 GPU 與驅動、真的桌面與合成器；
  - 約 18～27 fps 的軟體繪圖下的計時；
  - Docker Desktop 的 VM。

## 範圍的限界

- **SDL 排程的瞄軀幹（`combat_latency.hpp`）沒有驗證**：只有 `run_timing.py --gui --combat` 用到，那條路徑卡在 HUD 檢查的未結事項。
- 試玩（K8）還沒做，要實機。
- 每幀的觀察欄位留在 `native_window.hpp`，供之後找原因使用；它們不參與判定。
