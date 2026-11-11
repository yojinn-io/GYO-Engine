# Kernel 研究線：GYO 的組合協定（設計圖）

更新：2026-11-11。Owner：架構（架構師會話）。**狀態：設計稿，未定案、未開工。**
起點是使用者的願景稿「GYO-Engine 架構思想研究」（2026-11-10）與「GYO 架構設計思想」Concept Draft v0.2（2026-11-11；Bevy ECS／Scheduler、Service Mesh、Control／Data Plane）。

使用者 2026-11-10：**基盤的協定不能等壓力出現才改**；功能層的擴充才等壓力。
使用者 2026-11-11：**要的是 Bevy 那種抽象能力（World／System Contract／Scheduler／Executor），帶入現有 GYO，不是 GYO2；架構改造完成前，功能線停。** 2026-11-11 定：停法 (a)（已開工的卡做到下一個關卡停）；規格與 spike 同時開。

## 方法

不列功能，找不變量：讀 Bevy 與兩個網路 plugin 的原始碼，問「這些功能都靠哪幾條不變量才成立」，再逐條問 GYO 有沒有、GYO 的約束（決定性、狀態的權威、Engine 中性、C++ 無反射與借用檢查）會不會讓它變形、變形後還守不守得住效果。

證據（只讀審閱的報告，每條有檔名行號）：主 checkout `build/evidence/kernel-study/`（`bevy_core.md`、`replicon.md`、`lightyear.md`，`render.md`、`assets.md`、`platform.md`、`capabilities.md`、`os_io.md`）。原始碼在 `~/Code/GitHome/{bevy, bevy_replicon, lightyear}`（使用者 2026-11-10 放入）。

## 文件

- [mechanism.zh-Hant.md](mechanism.zh-Hant.md)：**本線的基準**。對 v0.2 的理解、機制骨架、契約與控制面（含使用者提供的審查修正與架構師兩條主張）、網路作為 System、規則變機制、不解決什麼、改造的性質與風險、判準。三張圖在 `diagrams/`。
- [networking.zh-Hant.md](networking.zh-Hant.md)：網路側的設計稿——網路要成為機制上的 System，機制必須提供的八條；網路層自己的設計圖 N1–N12；對 GYO 現況的對照。
- [blueprint.zh-Hant.md](blueprint.zh-Hant.md)：**輸入**（2026-11-11 降級）——Kernel 八條不變量、各層補充的不變量（R／A／P／C／O／N）、GYO 刻意的偏離（候選 DI-K1～K7）、反例清單、現況對照、Replacement Criteria。它沒有契約分類與 Executor 的分離，遷移順序一節假設了改造路線；以 mechanism 為準。
- [HANDOFF.md](HANDOFF.md)：記錄器。

## 批次（2026-11-11 核准）

```text
v8-arch-kernel
├── -spec    上段｜ultracode：提案 Opus×3／high、統合 Fable／high、對抗 Opus／xhigh｜約 1 天｜mechanism → 正式規格，每部件一節
├── -spike   上段｜實作 Opus／xhigh、審查 Fable／xhigh｜2～3 天｜獨立子目錄最小 C++ 原型，證明簽名推存取與受限句柄
└── -audit   上段｜Opus／high｜等 -spec｜現有模組逐一「留／包／重寫」，給中間層成本
```

| 批 | 內容 | 產出 | 判定（事前宣告） |
|---|---|---|---|
| `-spec` | 把 mechanism 落成規格：World、System 契約（兩半）、Registry、Scheduler（三種分析結果、歧義處置）、Executor（策略與執行緒親和）、延後佇列與事件（同步點）、多 World 與 schedule 組合、Host 保留的職責；每節：資料模型／操作／不變量／C++ 實現手段／Bevy 對照與不採的理由；Design Invariants 與 Replacement Criteria 草案；不含遊戲型別的驗證方式 | `spec.zh-Hant.md`；對抗檢查附末 | 對抗檢查者找不到「規格允許、但會破壞 (c) 區四處契約或獨占寫入」的反例；找到則回提案。不改程式 |
| `-spike` | 獨立子目錄（候選 `spikes/kernel/`，不進任何 target 的依賴），只做兩件：(1) 從函式參數型別在編譯期推出 `{reads, writes}`，含 Component、Resource、事件、延後佇列句柄；(2) 受限句柄＋debug 建置的 World 存取核對，未宣告的 id 即斷言。測試用的型別只存在測試裡 | 原型程式＋測試＋`spike-report.zh-Hant.md`（兩個編譯器的差異、做不到的事） | (1) 10 個以上不同簽名的 System，推出的集合與人手寫的期望逐一相同（static_assert＋執行期），L1 三平台（MSVC、gcc／clang、AppleClang）全綠；(2) 3 種繞路（存參考、拿裸指標、經未宣告 id 讀 World）在 debug 建置各被斷言攔下，release 不攔並寫明；(3) 突變：移除任一宣告 → 對應測試失敗。任一不成立＝spike 結論「做不到」，照 §14 停下回報，不補救 |
| `-audit` | 拿 `-spec` 去讀現有每個模組：有沒有自己的生命週期或更新迴圈；實際存取能不能被一份契約框住；是不是純函式 | `audit.zh-Hant.md`：逐模組留／包／重寫＋理由＋行數 | 每個結論附檔名與行號；對抗檢查抽 5 個模組複核 |

## 與其他線的關係

- 決定性等級（PR #177，`docs/architecture/determinism.md`）：(c)／(b) 區的定義；mechanism §3.3 的四處契約引用它。
- 第三方評估卡（PR #176）：每個候選方案要能當機制上的 System；選庫在責任邊界確認之後。
- 場景線（`docs/architecture/plans/scene/README.md`）：第一個非玩家實體與邏輯連線，是機制第一個真實的壓力來源。
- pv10 需求（`docs/object_fps_pvp/plans/v8/pv10-requirements.md`）：網路設計稿的輸入。
- AGENTS §2 的修訂候選：「基盤の協定は先に設計する」（交風紀委員，待使用者同意）。
