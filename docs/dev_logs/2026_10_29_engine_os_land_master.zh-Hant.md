# Engine os-land-master：OS 分支併 master

日期：2026-10-29。**完成到 PR**：把消費端發行後的 master 併入 OS 分支，做本機驗證與相關突變，推送並開 PR #104；PR 的四平台 L1 通過（[run 40832108929](https://github.com/yojinn-io/GYO-Engine/actions/runs/40832108929)）。合併待使用者同意。
這是收口（D78，決定 3 (a)）的最後一步；計畫與理由見 [收口提案](../architecture/plans/os-subsystem/closeout.md)，前一步見 [os-closeout 的 dev_log](2026_10_28_engine_os_closeout.zh-Hant.md)。

## 為什麼做

- D78 決定 3 (a)：只在 OS 分支的 os-wake-record（晚醒量測程式）與 os-threads-backend（執行緒命名與優先度搬進 engine/os），等消費端發行後以一條 PR 併 master，驗收用 CI 加守衛。
- 消費端的會話轉達使用者在發行會話的原話「這個可以開工了。」

## 做了什麼

- 先確認現況（`git fetch`、`gh pr view`）：
  - PR #96（發行線，`dd6e6b2b`）與 #101（人物模型換成 Player_V20）已合併，master 是 `62fd7ebf`。
  - `claude/os-windows-run` 仍是 `035ff14b`，已在本分支裡，不用再併。
- 本分支（`claude/trusting-joliot-f13b78`；在本工作樹以 `claude/os-closeout` 作業）併入 master（`87bad504`）：
  - 兩邊改動的檔案沒有重疊，沒有衝突。
  - master 那邊沒改 engine 與 tests/common，所以這兩處和併入前逐位元組相同（`git diff --cached --stat HEAD -- engine tests/common` 沒有列出）。
  - 和 master 的差異：35 個檔案，即 os-wake-record、os-threads-backend 與收口文件；apps 與 build/acceptance 沒有差異。
- 文件同步：
  - HANDOFF 的現況與未結事項，README 與 closeout.md 的狀態。
  - inactive_products.md 第 5 節：已隨 PR #96 進 master 的 10 批，以及隨本 PR 進 master 的 2 批，在狀態後面補上「後續」。
  - time.md 的 Baselines：補上 Windows 的基準（os-windows-run 的 W4，程式由 `4ffc7c65` 建置，Waiter 和現在相同；值取自 [Windows 實機的 dev_log](2026_10_28_engine_os_windows_run.zh-Hant.md)）；Linux 標成取消（D78）。原本兩者都寫「等下一次實機」。
- 盤點更正：closeout.md 寫「優先度要不要補讀回測試，由那條 PR 決定」。其實 `tests/common/os/ThreadTests.cpp` 已有優先度的讀回測試（macOS 讀 QoS、Windows 讀 thread priority、Linux 讀 nice），也有經 `RoleThread` 讀回的案例；run 40484797877 的四平台都通過，所以不必補。原因是盤點時沒有逐行確認（closeout.md 第 3 節原文也寫了「我沒有逐行確認」）。

## 驗證與經緯

- 本機（macOS Intel）：
  - core preset：configure、build、`ctest --preset core` 34／34 通過。
  - test preset（產品組態）：configure、build、`ctest --preset test` 第一次 92／93，`object_fps_pvp.presentation_evidence` 在 60 秒逾時。
    - 切分（AGENTS.md §14.1）：判斷為環境（主機負載），不是實作，也不是測試本身。
    - 根據：
      - 它是純 Python 的分析測試，不啟動建出的程式，檔案和 master 相同，merge-base 之後沒有人改過。
      - 10-28 的 test preset 中，它用 19.97 秒通過。
      - 逾時當時，主機的 load average 約 45（16 核），有其他會話在大量編譯。
      - 直接執行時 42／42 通過（負載下 82 秒）。
      - 負載降到約 11 之後，以 CTest 重跑通過（20.66 秒）。
    - 測試的判定（60 秒的上限）沒有改。
- 相關突變（D184：本 PR 改動檔案上的既有突變；用 test preset 的建置樹）：18 條，**12／12 killed**，Windows／Linux 專用的 6 條在 macOS 跳過。
  - os-subsystem 目錄：os-g6-stale、os-g8-link、os-g17-hygiene-coverage、os-c6-ansi-arguments、ti-x2-threads-links-more、th-x1-threads-os-public、th-x2-os-without-thread-library、th-x4-macos-background-class、th-x7-macos-name-claimed、th-x8-role-priority-lost 都是 killed。
  - time-threads-trace 目錄：tt1-stop-without-notify、tt2-role-backend-fixed 都是 killed。
  - 跳過：os-g18-windows-extra-link、pu-x10-windows-tests-without-manifest、th-x3-windows-name-byte-widened、th-x5-windows-interactive-highest（Windows），ti-x4-linux-name-not-cut、th-x6-linux-background-nice（Linux）。D78 之後本線不再為它們排實機。
  - runner 跑完後，`git status` 只列出文件，程式碼檔案都已還原。
- 推送：使用者說「推送：把 claude/os-closeout（2dd850ad）以 fast-forward 推到 origin/claude/trusting-joliot-f13b78，照草稿開非 draft 的 PR 到 master，讓 L1 跑。合併前再問我。」
  - 推送前 fetch：master 前進到 `19d8fd27`（PR #102，只改 v7.1 的 HANDOFF 與 STABLE_BASELINE），和本 PR 沒有重疊的檔案，所以沒有再併。
  - fast-forward 推送 `a9f6b6c5..2dd850ad`，開 PR #104（非 draft），並開 Auto-fix 監看。
- PR #104 的四平台 L1：[run 40832108929](https://github.com/yojinn-io/GYO-Engine/actions/runs/40832108929)（`2dd850ad`，pull_request）。`L1 /` 開頭的四個 job（windows-x64、macos-arm64、macos-x64、linux-x64）都是 success，CI gate 也是 success。Quick acceptance 在 PR 上不跑（`build/ci/common/ci_scope.py` 的 pull-request 範圍），併進 master 後由 push 事件跑。
- 範圍的限界：Windows 與 Linux 專用的 6 條突變沒有在對應的平台跑。

## 證據

- 本機 log：會話 scratchpad 的 `land-core.log`、`land-test.log`、`land-mutations.log`（不進 repo）。
- 決策：[HANDOFF](../architecture/plans/os-subsystem/HANDOFF.md) 的 D78 與 2026-10-29 的現況。
