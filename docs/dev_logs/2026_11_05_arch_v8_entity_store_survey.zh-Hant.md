# Engine 架構：v8-arch-entity-store-survey（實體容器的參照調查：Bevy ECS 與 EnTT）

日期：2026-11-05。分支 `claude/v8-arch-entity-store`（自 master `318b4e66` 切出）。主會話 Fable 5.1；實作者子 agent Opus 5.5／high（general-purpose）；敵對檢查者子 agent Fable 5.1／xhigh（xhigh-reviewer，唯讀；與實作者不同模型，依 `agents/navigator.md` 避免盲點重疊）。估時 2.5～3.5 h，實際約 2.5 h（實作者約 55 分、檢查者約 40 分、修正約 30 分，其餘為主會話的偵察、文件與核對）。

**狀態：batch 1 完成，停下等使用者核准。** 上段任務（Architecture 判斷、資料形式），不用小模型。任務卡與批次一覽見 [計畫 README](../architecture/plans/entity-store/README.md)，經過見 [HANDOFF](../architecture/plans/entity-store/HANDOFF.md)。

## 做了什麼

- 新文件 [survey.zh-Hant.md](../architecture/plans/entity-store/survey.zh-Hant.md)（321 行）：以 Bevy `v0.20.1`（`a0577b5c`）與 EnTT `v3.16.0`（`b4e58bdd`）的 pinned shallow clone 為據，逐機制判定「採／改裝後採／不採」，每個判定附 tag＋路徑＋行號的 permalink。以 `entity-model.zh-Hant.md` §1（已有 EnTT／flecs 調查）為起點，只補 Bevy 與逐機制深挖；flecs 不在範圍。
- 架構師的暫定立場（Engine 只收容器：index＋generation、SoA column、id 序決定性迭代、Commands 延後、change tick）以**假說**寫入並檢驗，不當結論。
- 三個必須以程式碼確認的點：
  - 3a「archetype 與 EntityId 升冪 digest 衝突」：**成立**。despawn 與加減元件都 `swap_remove`、搬到新 table 尾端；Query 不保證順序；`Entity` 的 `Ord` 先比 generation。
  - 3b「Commands 可在結構上守住 E-12」：**部分成立**。延後與決定性套用順序可採；但順序是入隊序不是 id 序、`spawn` 在入隊時取號；Bevy 的 Commands 守的是存取衝突不是階段紀律（exclusive system 與 World 直接 API 可隨時改結構）。E-12 要的單一延後入口＋固定階段套用，在 C++ 可用型別分離表達。
  - 3c「change tick 服務 pv10 每收件者分送」：**不採（第一版）**。一條 changed tick＋分送層的每收件者每實體送出狀態在技術上成立且不違反 E-8；不採的理由是 C++ 沒有 `DerefMut` 攔截（每個寫入點手動標記、漏一處即永久漏送）、entity-model §1.4 已核准不採、沒有觀測到的消費者（AGENTS §2）。
- 與假說不一致處（給 batch 2）：generation 與 E-3 矛盾（世代內不回收→恆 0）；change tick 與 §1.4 矛盾；「index」暗示 Engine 配 id，與 E-2 不合；「容器不配 id、鍵由外部給」沒有參照先例（Bevy 的 `spawn_at` 與 EnTT 的 `generate(hint)` 都仍以容器為 id 權威），根據只有已核准的 E-2。Bevy 本身沒有「只收容器」的先例（容器與執行期同 crate、不可分別編入）；先例只有 EnTT。
- 「不採」的 12 項整理成 Replacement Criteria 候選（survey §4.1），觸發條件改成可檢查的（引用 RC-1 的門檻等）。檢查者抽查 permalink 後，實作者再在 clone 上逐條核對引用行號；定義 120 個、使用全部對應。
- 計畫 README、HANDOFF 新建；本 dev_log。

## 判定

| 判定 | 結果 |
|---|---|
| 敵對檢查 | must 2／should 7／note 8，全部處置（survey「敵對檢查的處置」節）。三個 §3 判定：3a 確認；3b、3c 收窄（判定不變，理由改寫） |
| permalink 核對 | 檢查者抽查約 70／111 條（§3 全部與帶判定各列），行號相符；1 條偏移（`resource.rs:87-90`→`87-91`）已改 |
| 數字核對 | 檢查者核對 46 個 crate、位元寬、tick 常數等，無未標記的自創數字；未量測的推論在 §6 標「推論（未量）」 |
| 語言 | 正體中文；簡體字清單掃描 0 件；表頭與 HANDOFF 的日語用詞已改 |
| 建置／測試 | 不適用（只有文件） |

## 經過

- 偵察時發現 `entity-model.zh-Hant.md` §1 已有 EnTT／flecs 調查，向使用者提出「不重做、只補 Bevy 與逐機制深挖」，一併取得 Fable 檢查者、xhigh 檔位與 clone 參照程式的核准後開始。
- 任務卡寫的 `BlobVec`／`ThinColumn` 在 v0.20.1 不存在（實為 `BlobArray`／`ThinArrayPtr`），以 clone 為準。
- 檢查者兩個 must 都是「理由錯、判定對」：(1) 把「Rust 借用檢查」與「階段紀律」混為一談；(2) 把參照的「繞過配發器」誤讀為「外部 id 權威」的先例。兩者若不改，會直接寫錯 batch 2 的 Design Invariants 的證據句。
- 檢查者指出 survey 六次引用 `docs/architecture.md:129` 當不收 world／app／plugin 的根據，而同一句也禁 ECS；已在 survey §5 寫明那一行自己要在 Delta 的 PR 內由圖書管理員修改，避免選擇性引用。

## 證據

- 主 checkout `build/evidence/entity-store/`（git 管理外）：`survey-implementer-report-20261105.md`（實作者最終報告）、`survey-adversarial-check-20261105.md`（檢查者報告原文摘錄）。
- 參照程式的 clone 在會話 scratchpad，不保存；耐久的出處是 survey §7 的 permalink（tag＋commit）。

## 給 batch 2（`v8-arch-entity-store-delta`）的事

- 經調查後留在容器的是四件：外部給的 id 鍵、每元件一欄依 id 排序、依 id 的交集與迭代、生成／刪除待辦依 id 序套用。與 pvp 現行 `EntityColumns.hpp`（三個自由函數）＋E-12 待辦幾乎同形。
- 使用者 2026-11-05 已決定搬進 Engine（案 (b)），本批不重開；但 batch 2 要正面回答 AGENTS §1／§2 的「只有一個消費端，根據是觀測到的壓力還是預測」，並在容器長大前寫好 Replacement Criteria。
- 「不帶 generation」要帶前提：E-3 只管 Match 段，玩家段靠 Gateway 的單調配發（entity-model §5.3）。
