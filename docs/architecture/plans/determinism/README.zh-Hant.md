# 決定性的等級與守衛擴張（`v8-arch-determinism-levels`）

更新：2026-11-11。Owner：Engine（架構）。任務卡由架構師會話（例行架構掃描）於 2026-11-10 開出，起因是使用者在 side chat 提出
「GYO 的決定性要哪一級，各模組為它付了什麼」（第三方評估 PR #176 的 D0，排在 D1、D2 之前）。
**狀態：契約草稿完成並經對抗檢查修正（[determinism.md](../../determinism.md)，英文），停下等使用者決定 DET-0～DET-3。**（2026-11-12 圖書管理員補註：後續：2026-11-12 DET-0～DET-3 定為暫停，見 kernel HANDOFF 的 `v8-arch-kernel-pending-paused`） 只寫文件：不改程式、旗標、golden。

## 這是什麼

- 契約：三級（(a) 同程序、(b) 同二進位同平台、(c) 跨平台逐位元）的定義；決定性邊界（四條「在內」的規則、六區、各區等級）；
  每個模組為 (c) 付了什麼、現在的守衛、缺口；第三方的規則；候選的 Design Invariants 與 Replacement Criteria；對抗檢查的逐條回應（§9）。
- 本 README：守衛擴張的計畫（E0～E5）、`-ffp-contract=off` 順序的修正方案、Kernel 設計圖 DI-K5 的改寫文字、要使用者決定的事。

## 結論（一句話）

不是全部一級，是一條邊界：被 Client 預測的模擬核心、host 判斷、工具↔執行期的契約資料、Client↔Match 在 join 時比對的內容身分要 (c)；呈現層 (b) 即可；
「被判定但不被預測」的中間地帶（有動畫的命中、未來的 AI）現在實際上是 (b)，要不要升到 (c) 由使用者決定（DET-1）。
**但預測核心今天也不是 (c)**：11/9 場次有輸入、狀態、位置都相同而 `sweep_fraction` 差 1～4 ULP 的步驟（Windows x64 對 macOS x64，路徑上沒有 libm），原因未查明，
照 §14.3 先建偵測（E0，DET-0），再談擴張。

架構師前提的修正：11/9 的 Match 在 macOS x64 不在 Linux；「兩端一致」是 99.7 % 逐位元相同、其餘是輸入不同，**加上**少數 ULP 級的真差異；
離線分析器不重算模擬，不是 (c) 的理由；L1 現在是三平台（macos-x64 自 2026-10-31 只做本機建置）。

## 要使用者決定的事

見契約 §0（DET-0 先做偵測、DET-1 中間地帶、DET-2 第三方一律邊界外且 ufbx 今天在內、DET-3 順序）。推薦：E0 → E2＋E5 → E1（＋E3 若 DET-1＝(c)）→ E4。

## 守衛擴張計畫

代號＝函數名，說明＝註釋。全部上段（觸及 Design Invariants 與數值契約）。工時是量級，未經實作驗證。

```text
v8-arch-determinism-levels              「本卡｜契約草稿；停下等 DET-0～3」
├── E0 v8-engine-sweep-ulp-detect       「xhigh｜重現並定位 sweep_fraction 的跨平台 ULP 差；只偵測不修｜1～2 天」
├── E2 v8-engine-determinism-guards     「high｜旗標順序、configure 檢查、intrinsic 掃描、arm64 金絲雀、VoxelMesh 小數 golden｜0.5～1 天」
├── E5 v8-product-anim-digest-golden    「high｜產品動畫內容 digest 的字面 golden，三平台 L1｜0.5 天」
├── E1 v8-engine-math-trig              「xhigh（函數本體）／high｜Math 自己的 Sin／Cos／Acos／Atan2；全部呼叫點；5 個情境改 golden；§14.4 揭露｜4～6 天」
├── E3 v8-product-digest-pose           「high｜只在 DET-1＝(c)：合成兩鍵 clip 走 SamplePose 的 digest 情境｜2～3 天，E1 之後」
└── E4 v8-arch-kernel-k5-wording        「medium｜PR #175 的 DI-K5 改寫；animation-events.md 或 model.md 加一行指向契約｜0.5 天」
```

| 代號 | 做什麼 | 判定（結果出來前宣告） | 模型／檔位 |
|---|---|---|---|
| E0 | 照 §14.3：(1) 從 11/9 證據取出那幾步的 before 狀態、指令與 arena（biome），寫成 `tests/common/collision` 的 corpus 記錄或產品的單步重現；(2) 在 windows-x64（CI）、linux-x64（CI）、macos-arm64（CI）、macos-x64（本機）各跑 `gyo_collision_corpus`（既有兩樹工具，輸入由整數產生）與該重現，逐位元 diff 記錄；(3) 重現出差異後，在 `CapsuleQueries.cpp`／`Collision.cpp` 的 sweep 路徑逐個中間量印位元，二分到第一個不同的運算；(4) 只記錄原因與候選解釋（libm 呼叫、MSVC `/fp:precise` 的中間精度、不同程式路徑、`-0`），不修 | 事前宣告：重現成功＝至少一組 CI 平台對 macos-x64 本機、或 windows-x64 對 linux-x64 的 corpus 記錄有位元差；若四處都相同，則報告「CI 平台間一致、差異只在本機 macos-x64」並停下（這改變 DET-0 的意義：macos-x64 不是發行平台） | 實作 Opus／xhigh（數值定位）；審查 Fable／high（啟動前問） |
| E2 | (1) `GyoBuild.cmake`：把 `-ffp-contract=off` 的 `add_compile_options` 搬到 `add_subdirectory(third_party)` 之前（`/Zc:preprocessor` 留在後面，它的理由不同）；(2) configure-time 檢查 `gyo_expect_no_contraction(<target>)`：讀邊界內目標（含 `gyo_ufbx`）的 `COMPILE_OPTIONS` 與目錄選項，缺旗標即 configure 失敗，像 `gyo_expect_*_links`——這是 x64 的守衛（x64 沒有 `/arch:AVX2`／`-march` 就沒有 FMA 指令，金絲雀在 x64 不可能失敗）；(3) 來源掃描守衛（照 `neutral_names_guard.py`）：`engine/math`、`engine/collision`、`engine/voxel_mesh`、`engine/entity_store`、`engine/model`、`apps/object_fps_pvp/src/Pvp`、`src/Gameplay/Player`、`src/Collision`、`tests/object_fps_pvp/AuthorityDigest.cpp` 不得出現 `_mm_fmadd`、`vfma`、`__FMA__`、`std::fma`、`fast-math`、`long double`；(4) 執行期金絲雀 `tests/common/math`：以執行期資料算 `a*b+c`，選使 fused 與 unfused 結果不同的 a、b、c，斷言 unfused 的位元；label `cpu`；**只在 macos-arm64 有偵測力**，寫明；(5) VoxelMesh 加一個 `cell_size` 0.1 的字面值 golden（`ArenaDigestTests` 已釘 strtod，這只是補 VoxelMesh 自己的入口） | 三平台 L1 通過；突變：拿掉旗標（arm64 金絲雀要失敗；x64 由 configure 檢查失敗）、在邊界目錄放一個 `std::fma`（掃描要失敗）、把 configure 檢查的目標清單清空（要失敗） | 實作 Opus／high；審查 Opus／high |
| E5 | `tests/object_fps_pvp` 加「產品動畫內容 digest 等於字面值」：對 `assets/object_fps_pvp` 的角色 FBX 與 `presentation.json` 算 `AnimationContentDigest`，與獨立算出的字面值比（同 `ArenaDigestTests` 的做法）；label `cpu`。把 H6 的人工比對變成會失敗的測試。長期：姿勢資料由工具烘焙、以資料契約交給執行期（Engine 不承擔工具職能），那時這個測試改比烘焙檔 | 三平台 L1 相同；改一個 clip 的鍵值要失敗（突變） | 實作 Opus／high；審查 Opus／high |
| E1 | `GYO::Math` 加 `Sin`、`Cos`、`Acos`、`Atan2`（固定多項式，例如 musl 的 sinf／cosf／acosf／atan2f 以 GYO 的旗標重寫，MIT；或 Cephes），結果在三平台 L1 與 macos-x64 本機逐位元相同；特性測試量它和 libm 的最大差（預期 ≤1 ULP）並凍結；**全部**呼叫點換過去：`Quaternion.hpp` 的 `FromAxisAngle` 與 `Slerp`、`Matrix4.hpp` 的旋轉、`PlanarMovement.cpp`、`ShotQuery.cpp`、`PlayerAnimation.cpp:172`、`PlayerPose.cpp:363`、digest 驅動的 `Aim`（`atan2`／`hypot` → `Math::Atan2` 與 `sqrt`）；呈現層一併換（hitbox 必須和 renderer 畫身體的方式一致，`PlayerHitbox.hpp:59`）；`move-turning/*`、`combat-kill-respawn-aimed/*`、`combat-graze` 改為 golden；golden 檔重生一次。**§14.4 揭露**（有緩的面）：`MathCharacterizationTests` 的「Animation Slerp equals Math Slerp bit for bit」與 Matrix4 旋轉的凍結 legacy 副本（`:131-133`、`:309-331`）用 `std::acos`／`std::sin`，換函數後會失敗；守的性質（新舊實作同位元）要改成「對凍結的 legacy 差 ≤ 1 ULP，且四處同位元」，緩的面＝允許 ≤1 ULP 的差，締的面＝5 個情境加入跨平台比對、legacy 比對改為跨平台；要 Fable 檢證者 | 三平台 L1：42／42 golden 相同、-O0 與最佳化相同；macos-x64 本機相同；兩樹 compare：換函數前後**不是** 0 差分（預期），差分只出現在用到角度的情境，數量與第一個差異的 record 寫進事前宣告；E0 的結論要先在手上 | 實作 Opus／high，函數本體 xhigh；審查 Opus／high；§14.4 檢證者 Fable（啟動前問） |
| E3 | 只在 DET-1 選 (c)：digest 加情境「合成兩鍵 clip 的目標被射中」，走 `Model::SamplePose`（E1 後 Slerp 無 libm）；FBX 載入不進 digest（E5 另守） | 新情境三平台相同；既有 golden 不變（只在出現時編碼，E-9 的先例） | 實作 Opus／high；審查 Opus／high |
| E4 | PR #175 的 DI-K5 改成契約 §8 的文字；`animation-events.md`（或新 `model.md`）加一行「取樣是 (b)，見 determinism.md §4.2」 | 文件 | Opus／medium |

### `-ffp-contract=off` 順序的修正：兩個方案

| 方案 | 做法 | 優點 | 缺點 |
|---|---|---|---|
| **A（建議）** | 把那兩行 `add_compile_options` 搬到 `add_subdirectory(third_party)` 之前 | 一行搬動；註解「applies to the directories added below」變成全部為真；`gyo_ufbx`（今天就在邊界內，§4.9）、SDL、imgui 一併不收縮，arm64 上的 Clang 不再融合 | 全部第三方重建一次；第三方效能的微小損失（未量）；不等於「第三方進入邊界」——DET-2 仍要求 golden 才算 |
| B | 旗標位置不動，對邊界會用到的第三方目標逐一 `target_compile_options(<t> PRIVATE -ffp-contract=off)` | 只動需要的 | 今天就有一個（`gyo_ufbx`）；每加一個候選都要記得加，漏了只有 configure 檢查能抓 |

兩案都要配 E2 的 configure-time 檢查與 intrinsic 掃描；旗標管不到 intrinsic（ozz 的 `_mm_fmadd_ps`、Jolt 的 `USE_FMADD`），這是 PR #176 §2.5 第 2 條。
旗標也管不到 ufbx 內部呼叫的平台 `sin`／`cos`（`ufbx.c:259-263`）：那一部分只有 E5 的 golden 能守，或把轉換搬到烘焙時。

## 不做的事

- 不改 `authority_golden.txt`、不改 `DigestVersion`、不改任何 Design Invariants 節（契約的 DI 是候選）。
- 不替第三方評估（PR #176）的 D1、D2 下結論；那張卡在 DET-1 定案後重看。
- 不替 Kernel 設計圖（PR #175）改字；只提供文字（契約 §8）。
- 不去找 ULP 差的原因（那是 E0 的工作；本卡只記錄現象與證據）。

## 成果物與證據

- 契約：[docs/architecture/determinism.md](../../determinism.md)。
- 本資料夾：README、[HANDOFF](HANDOFF.md)。
- dev_log：[2026_11_10_arch_v8_determinism_levels](../../../dev_logs/2026_11_10_arch_v8_determinism_levels.zh-Hant.md)。
- 證據（不進 repo）：主 checkout `build/evidence/determinism-levels/`（11/9 場次 Match 對三台 Client 的移動步驟比對腳本與結果、README）。
