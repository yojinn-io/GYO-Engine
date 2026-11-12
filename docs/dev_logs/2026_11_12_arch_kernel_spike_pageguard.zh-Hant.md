# 架構 v8：v8-arch-kernel-spike-pageguard（頁保護能不能當存取驗證的第三層）

日期：2026-11-12。Owner：架構（kernel 研究線）。計畫：[kernel README](../architecture/plans/kernel/README.md) 批次表的 `-spike-pageguard` 列、[事前宣告](../architecture/plans/kernel/spike-pageguard-declaration.zh-Hant.md)。報告：[spike-pageguard-report.zh-Hant.md](../architecture/plans/kernel/spike-pageguard-report.zh-Hant.md)。PR [#184](https://github.com/yojinn-io/GYO-Engine/pull/184)。
- 執行：實作者＝架構師會話開的子代理（Opus／xhigh，獨立工作樹 `.claude/worktrees/agent-a1bd3530dcfa71c3c`，分支 `worktree-agent-a1bd3530dcfa71c3c`，commit `ed9657f9`、`defb759c`，只 commit；cherry-pick 到本分支為 `85312dc2`、`a1e82e46`）；敵對檢查者＝Fable／xhigh 只讀；主會話協調、獨立重跑、寫報告。
- 性質：偵察。只動 `spikes/kernel/`；不改 engine、契約、workflow。

## 起因

spike 的範圍の限界 L1（寫了又寫回）與 L6（host 碰過欄後的繞路寫）是兩層驗證結構上攔不到的。使用者 2026-11-11 定了頁保護的形狀（只對寫、只獨占欄、只 (c) System 執行時），B-9 結案後歸「存取驗證（可關）」第三層。本批量它攔不攔得到、能不能繼續執行以 Err 回傳、三平台、成本。

## 做了什麼

頁對齊的欄儲存（`mmap`／`VirtualAlloc`，頁大小執行期查）、Run 進入全保護／(c) System 只解除宣告寫的欄／(b) 全解除／Run 結束回可寫、`sigaction(SIGSEGV|SIGBUS)`／VEH 處理器（查區間表→記進 `tlCurrent` 的 context→解除→返回）、跳板回 Err；19 個測試、7 條突變、Cost 量測；r2 依敵對檢查修 handler 串接、跨 World 身分、兩個測試缺口、(b) 成本。詳細在報告。

## 受理判定（照宣告原文）

件数の突き合わせ：一致——測試 19 新／33 既有，debug 52 過、release 51 過 1 skip；突變 7 新／41/41 killed；本機兩組態 ctest 2／0（實作者、敵對檢查者、主會話三方一致）；三平台待 L1。

- 判定 1 攔截：**不成立**（(b3) Resource 不在保護頁；宣告自相矛盾——判定列了 (b3)、範圍排除 Resource，架構師的錯）。L1、(b1)、(b2) 攔到；宣告形狀的 L6 攔到但第二層本來就攔得到，原始 L6 攔不到。使用者 2026-11-12 決定讀法 (i) 照原文（宣告與測試都不改）；兩讀法下對 A.6 的建議相同（L1 可補、L6 不能）。
- 判定 2：成立。判定 3：本機過，三平台待 L1。判定 4：成立（protect ≈ 2 µs、switch ≈ 7 µs、full ≈ 200 µs、zone_b ≈ 200 µs，門檻 10／50／1000）。判定 5：成立（五條各由宣告的性質殺掉）。

## 經緯

- 判定 3 三平台：**成立**（L1 run 43169204148，三平台 `kernel-spike-tests.xml` 各 tests 2／failures 0，頁大小 4096／4096／16384；artifact 存 `build/evidence/kernel-pageguard/l1-43169204148/`）。
- 第一輪 45 分鐘、r2 15 分鐘（估 6 h）。沒有碰到 §14 停止條件；判定 1 不成立依 §14 先報告使用者、不改測試。
- macOS `raise(SIGSEGV)` 的 si_code＝0（非 `SI_USER`）→ `SentByProcess` 修正，證據為修正前後兩份探測輸出（無 doctest 迴歸測試，需 death test）。本機保護故障是 SIGBUS。
- 敵對檢查 0 blocker／7 should／4 nit；S1（外來故障後 handler 永久卸除）以對抗方的探測程式重現（exit 138）→ r2 後兩次 Run 都攔到。
- 主會話的錯：HANDOFF 第一次記錄把判定 4 的 Debug／Release 組態標反（敵對檢查 S7），已改。
- 證據：主 checkout `build/evidence/kernel-pageguard/`（實作者兩輪 ctest／build／突變／探測、`adversary-report.md` 與探測、`verify-main-session-r2.txt`）。本機 Intel Mac。

## 架構報告

- Architecture Delta：無（spike 不進 build graph）。
- 給規格的 11 點（A.6 第三層的範圍、Resource 保護、(b) 成本、handler 共存、多 World、欄重新配置、`tlCurrent` 為空的歸類、第一層改 Err、頁粒度、平台層放 OS 子系統、第二層能否減）在報告末節；待 A.6 修訂時處理。
- Code smell：spike 的 `Zone` 預設 C 違反 S-6（spike 簡化，不進 Engine）。
