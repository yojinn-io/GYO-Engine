# 架構 v8：決定性的等級與邊界（`v8-arch-determinism-levels`）

日期：2026-11-10～11。分支 `claude/arch-determinism-levels`（切自 origin/master `a75152d8`）。docs-only；不改程式、旗標、golden。
- 執行：架構師開出的任務卡會話（主會話 Fable 5.1／high；卡片指定 Opus／high，主會話模型由使用者設定）。
- 對抗檢查：只讀子代理 general-purpose、Opus／high（和主會話不同模型，照卡片）。6 blocker／8 should／7 nit，全部核對並處置；結果與回應在契約 §9。
- 文件 owner：Engine（架構）。

**狀態：** 契約草稿完成並經對抗檢查修正，停下等使用者決定 DET-0～DET-3（[determinism.md §0](../architecture/determinism.md)）。

件數の突き合わせ：一致（11/9 場次的比對：Match `step` 262,749 筆；win1 `step` 80,866、連接 80,788、相等 80,565、不同 223、只在 Client 的 78；win2 80,301／80,197／79,985／212／104；win3 101,768／101,716／101,578／138／52；每台連接數＝相等＋不同；不同的 223 筆＝before 已不同 170＋before 相同而位置不同 51＋位置相同而中間量不同 2）。

相關文件：
- [determinism.md](../architecture/determinism.md)（本批的產出，英文契約）
- [plans/determinism/README.zh-Hant.md](../architecture/plans/determinism/README.zh-Hant.md)（守衛擴張計畫 E1～E4、旗標修正方案）
- [plans/determinism/HANDOFF.md](../architecture/plans/determinism/HANDOFF.md)

## 做了什麼

- 寫 `docs/architecture/determinism.md`：三級的定義；決定性邊界（三條「在內」的規則：進 digest、被 Client 預測、工具↔執行期契約）分四區
  （預測核心 (c)、中間地帶 DET-1、契約資料 (c)、邊界外 (b)）；每條規則現在寫在哪、守衛是什麼、缺口是什麼；八個模組逐一；證據；第三方規則；
  缺口總表；Kernel DI-K5 的改寫文字；候選 Design Invariants 6 條與 Replacement Criteria 4 條。
- 寫計畫 README：E2（旗標順序、FMA 金絲雀、intrinsic 掃描、VoxelMesh 小數 golden）、E1（Math 自己的 Sin／Cos／Acos、5 個情境改 golden）、
  E3（合成 clip 的命中進 digest，只在 DET-1＝(c)）、E4（DI-K5 文字）；旗標修正 A／B 兩案（建議 A）。
- `docs/architecture.md` 的 Runtime boundaries 前加一行指向契約。
- 證據：主 checkout `build/evidence/determinism-levels/`——三支 Python 腳本與結果，把 11/9 lag-l2 場次的 Match（macOS x64）與 Client win1（Windows x64）
  移動診斷以 (player_id, life_generation, epoch, sequence) 連接、逐欄比對、分類。

## 結論（返回給架構師與使用者）

1. **邊界成立，不是全部一級。** 預測核心（Movement、Collision、VoxelMesh 展開、EntityStore 順序、digest 編碼，含 digest 收錄的 Client 呈現欄位）、host 判斷、契約資料、Client↔Match 在 join 時比對的內容身分（動畫與 arena 的 content digest）要 (c)；呈現層 (b)。
0. **預測核心今天也不是 (c)（對抗檢查 F2 的發現）**：11/9 場次 win1 有 2 步、win2 3 步、win3 1 步，輸入、before、after 都相同而 `sweep_fraction`（win3 另含 `sweep_normal.z`）差 1～4 ULP；windows-x64（MSVC）對 macos-x64（AppleClang），路徑上沒有 libm、x64 沒有 FMA 指令。原因未查明，照 §14.3 不猜、不修，先建偵測（E0、DET-0）。這改變了「(c) 只差三角函數」的前提。
2. **中間地帶今天實際上是 (b)**：有動畫的命中走 `Model::SamplePose` → `Math::Slerp` 的 `acos`／`sin`（B3 量到三組平台不同）；Client 不預測命中，
   所以它正好落在架構師說的「不被預測」一區。要不要升到 (c) 由使用者決定（DET-1）。推薦以 (c) 為目標，由 E1 一次達成，排在第一個 AI 批之前。
3. **(c) 今天的守衛只有零角度子集**：37／42 個 digest 情境、三平台 L1（linux-x64、windows-x64、macos-arm64；macos-x64 自 2026-10-31 `5ea8fcc0` 只做本機建置，沒有 L1 列——本卡初稿寫「四平台」，對抗檢查 F1 指出）。非零角度（5 個情境）只「列為資訊」；原因是 `sin`／`cos` 走平台 libm。
   不是「邊界內全部 (c)」——`math.md:78` 明寫三角函數不跨平台一致。
4. **第三方一律邊界外，除非旗標與 intrinsic 受控且有跨平台 golden**（DET-2）。初稿寫「今天邊界內沒有第三方算術」，對抗檢查 F4 推翻：`AnimationContentDigest` 在 Match 與 Client 各自以 ufbx 執行期轉換 FBX 後算 SHA-256、join 時逐位元比對（protocol-v9 §4.6），ufbx 在旗標之前建置且內部呼叫平台 `sin`／`cos`——今天就是跨平台的位元關卡，只靠 B3 的實測與 H6 的人工比對。補 E5（字面 golden）；長期由工具烘焙姿勢資料。nlohmann 是 header-only（拿 GYO 的旗標）；strtod 已由 `ArenaDigestTests` 用非二進位小數釘住。
5. **對架構師前提的修正**：(i) 11/9 的 Match 跑在 macOS x64（Intel Mac 本機建置），不是 Linux；(ii)「兩端一致」＝win1 連接的 80,788 步中 80,565 步
   逐位元相同（99.7 %；win2 99.7 %、win3 99.9 %），航向任意（記錄沒有 yaw 欄，由位移方向推得）；其餘 223 步中 170 步在該步之前就已不同、
   51 步 before 相同但位置不同（對抗檢查以 `match-commands.jsonl` 連接：37 筆是替代的 `held` 指令、14 筆是生命切換時 Match 靜止的 `actual` 指令；
   方向差是 0.143° 的倍數，是輸入不同），**另 2 步是真正的 ULP 差**（上面第 0 點）；(iii) 離線分析器（lag_l2_summary、lan15_judge、shot／ingress）
   只讀 log 不重算模擬，在 Mac 判 Linux 紀錄不需要 (c)，不能當 (c) 的理由。(c) 真正的理由：三平台共用一份 golden、在開發機重現 Linux Match 的事故、
   兩端診斷不帶容差、join 時的內容身分比對。
6. **Audio 的整數混音與 VoxelMesh 的整數單位保留**：成本為零、三平台 L1 已是 (c) 守衛；D0 表「改 float 混音」留給 Audio 的 Replacement Criteria 判，不由 DET-1 決定。

## 經過

1. 讀 AGENTS.md、`agents/effort-gearbox.md`、`agents/navigator.md`。
2. 讀 PR #176 的 evaluation §2.4／§2.5／§4 D0 表；math.md 數值政策；collision、audio、voxel-mesh、entity-store 的 Design Invariants；
   entity-model 的 digest 條文（E-5、E-6、E-8、E-9、E-10、§5.4）；hitbox-plan R5／B3 與 golden policy；anim-bottlenecks dev_log 的 B3 結果；
   entity-requirements 4-6；Kernel 設計圖（PR #175）DI-K5；2026-11-04 架構健檔對 `engine/model` 無決定性承諾的記錄。
3. 讀程式：`GyoBuild.cmake`（旗標在 `add_subdirectory(third_party)` 之後）、`third_party/CMakeLists.txt`；`AuthorityDigest.cpp`（golden 子集的定義與 `Angle` 檢查、42 情境中 37 golden）、
   `authority_digest_check.cmake`（-O0 比對只在 golden 子集；其他「資訊」）、`tests/object_fps_pvp/CMakeLists.txt`（label `cpu;gameplay;pvp`）；
   `Movement.cpp`、`PlanarMovement.cpp`（sin／cos）、`ShotQuery.cpp`、`PlayerAnimation.cpp`、`PlayerPose.cpp`（SamplePose）、`Quaternion.hpp`（Slerp 的 acos／sin）、
   `Angle.hpp`（remainder）、`Collision.hpp`／`StaticAabbGrid.cpp`（double 單次捨入、floor）、`Mixer.cpp`／audio 特性測試（FNV 雜湊、label cpu）、
   voxel_mesh 測試 label；CI 矩陣（四平台）；離線分析器是否重算（grep：不重算）。
4. 比對 11/9 場次：先數 kind 與 player（Match 262,749 step；Client 80,866 step，player 5、6、7），再連接比對、分類、算位移方向與 ULP；
   讀 `match.log` 的 ingress statistics 加總替代與遲到的指令數。腳本與結果存主 checkout `build/evidence/determinism-levels/`。
5. 起草契約、計畫、HANDOFF；開對抗檢查子代理（Opus／high，只讀；資料：契約、計畫、各模組契約、digest 程式與 CMake、GyoBuild.cmake、hitbox-plan、證據夾與腳本）。
6. 逐條核對對抗檢查的發現（讀 `package_contract.py`、`cross-platform.yml`、`DigestFields`、`PlayerAnimationDigest.cpp`、`ClientConnection.hpp`、protocol-v9 §4.6、fetch 下來的 `ufbx.c`、`ArenaDigestTests.cpp`、`UiLayoutGolden.hpp`、`MatchRuntimeHost.cpp:603`），寫新腳本 `intermediate_only_diffs.py` 對三台 Client 抽出「位置相同、中間量不同」的步驟並算 ULP，對 win2、win3 重跑比對；重寫契約 §0～§9、計畫（加 E0、E5）、HANDOFF、本 dev_log。

## 對抗檢查

- 設定：general-purpose、Opus／high、只讀；約 17 分鐘、104 次工具呼叫。
- 發現：6 blocker（F1 三平台、F2 ULP 差、F3 預測欄位與 host 判斷在 digest、F4 ufbx 的內容 digest、F5 digest 驅動的 atan2、F6 E1 漏呼叫點）、8 should、7 nit。
- 處置：全部接受並改文件；S1 的指令連接（37 held／14 actual）未由本會話重跑，契約以「對抗檢查的結果」標示；S7 部分駁回（entity-model §5.4 第 251 行確有 DigestVersion 5 的規劃），但其結論（E1 是唯一一次 golden 重生）接受。逐條在契約 §9。
- 文件因此改了什麼：§1 平台；§2 加規則 4（內容身分）與兩區（host 判斷、內容身分）、預測核心納入 digest 的呈現欄位；§4.9 新節；§5 重寫；§0 加 DET-0；DI 六條全部改寫、RC 加一條；計畫加 E0、E5，E1 範圍擴大並加 §14.4 揭露，E2 的金絲雀改為 arm64 守衛。

## 查證過的事實

| 事實 | 出處 |
|---|---|
| GYO 的 `-ffp-contract=off` 在 `add_subdirectory(third_party)` 之後加 | `build/cmake/GyoBuild.cmake:35`、`:41-42`；加入於 2026-10-03 `8f836766`（B1b） |
| digest 42 情境、37 golden；golden＝所有 yaw／pitch 為 0；非 golden：`move-turning/*`、`combat-kill-respawn-aimed/{product,contract}`、`combat-graze/corridor` | `tests/object_fps_pvp/AuthorityDigest.cpp:6-10`、`:134-137`、`:749-846`；`fixtures/authority_golden.txt` 非註解 37 行 |
| -O0 與最佳化的比對只在 golden 子集強制；原因是 Apple clang 把 sinf／cosf 合成 `__sincosf_stret`，arm64 可差 1 ULP | `tests/object_fps_pvp/authority_digest_check.cmake:3-8` |
| digest 的命中模型是合成的，不讀 clip | `AuthorityDigest.cpp:45-47`；entity-model §7 G13 的註 |
| 移動一步只有 `sin`／`cos`（yaw）走 libm；`WrapRadians` 用 `std::remainder`（精確）；跳躍用 `sqrt`（精確） | `PlanarMovement.cpp:17-18`、`Angle.hpp:21`、`Movement.cpp:84` |
| 射擊方向用 `sin`／`cos`（yaw、pitch） | `ShotQuery.cpp:82-84` |
| 姿勢取樣走 `Model::SamplePose` → `Math::Slerp` 的 `acos`／`sin`；B3：四平台各自可重現、載入四平台相同、取樣三組不同 | `PlayerPose.cpp:510-561`、`Quaternion.hpp:93-96`；`dev_logs/2026_10_31_pvp_v8_anim_bottlenecks.zh-Hant.md:31-35` |
| Collision 以 double 計算、對 float 捨入一次 | `engine/collision/include/engine/collision/Collision.hpp:17-22` |
| Audio 整數混音「identical on every platform」；特性測試以 FNV-1a 雜湊比對；label cpu | `docs/architecture/audio.md:36-37`；`tests/common/audio/CharacterizationTests.cpp:49`；`tests/common/audio/CMakeLists.txt:8` |
| VoxelMesh DI 5 位元 golden、DI 7 鍵序；單位 1/16 cell | `docs/architecture/voxel-mesh.md:249-258`、`:157-160` |
| EntityStore DI 8「Deterministic by construction」 | `docs/architecture/entity-store.md:167-170` |
| AI「只要 Match 自己一致」 | `docs/object_fps_pvp/plans/v8/entity-requirements.md:74`、`:155` |
| 11/9 場次 Match 在 macOS 26.7.1 x64、三台 Client 在 Windows 11 x64（win3 資料夾是跑 win2 kit 的測試者）、同 revision `24943706` | `session/mac/machine.txt`、`session/clients/*/machine.txt`、`MAPPING.txt`、`marks.txt` |
| L1 平台＝windows-x64、linux-x64、macos-arm64；macos-x64 只做本機建置 | `build/acceptance/common/package_contract.py:18`；`.github/workflows/cross-platform.yml:287`；commit `5ea8fcc0`（2026-10-31） |
| digest 收錄 Client 的 `renderPosition`、`correctionOffset`、`interpolationAlpha`、phase 校正，與 host 的 quality／eviction | `AuthorityDigest.cpp:63-66`、`:411-415`、`:621-648`、`:715-721` |
| `AnimationContentDigest` 在 join 時比對；ufbx 以平台 libm 做 Euler→四元數；`gyo_ufbx` 在旗標前建置 | `PlayerAnimationDigest.cpp:54-108`、`ClientConnection.hpp:128-131`、`protocol-v9.zh-Hant.md:99-101`、`_deps/ufbx-src/ufbx.c:259-263`、`third_party/ufbx/CMakeLists.txt:13` |
| digest 驅動的瞄準用 `std::atan2`／`std::hypot` | `AuthorityDigest.cpp:385-391` |
| `ArenaDigestTests` 以非二進位小數釘住 strtod | `tests/object_fps_pvp/ArenaDigestTests.cpp:17-24`、`:47-49` |
| UI layout golden 是字面 binary32、明寫依賴 contraction 關閉 | `tests/common/ui/UiLayoutGolden.hpp:14-18` |
| host 的時鐘是注入的 `now_()` | `MatchRuntimeHost.cpp:603` |
| 離線分析器不重算模擬 | `build/acceptance/object_fps_pvp/*.py` grep `recompute|re-judge|replay` 只在 GUI／soak 證據腳本的字串，判定腳本只讀 log |
| Client 預測以 correction 吸收任何不一致 | `LocalPlayerPrediction.cpp:158-173` |

## 證據

- 主 checkout（使用者的 Intel Mac）`build/evidence/determinism-levels/`：`README.md`、`compare_movement_diag.py`、`classify_movement_diff.py`、
  `displacement_of_diffs.py`、`intermediate_only_diffs.py`、結果 `.json`（win1 三份、win2／win3 摘要、三台的中間量差）。輸入為 `build/evidence/v8-lag-l2-session-20261109/session/`（已存在）。
- 對抗檢查者自己的腳本在會話 scratchpad，未保存（只作出處）。
- 讀程式與文件的 grep 輸出：未保存（只作出處，不用於判定）。

## 時間

時間盒 8～12 h。實際約 5 h：讀 1.5 h、比對 0.5 h、寫 1 h、對抗檢查 0.3 h（子代理）、核對與重寫 1.5 h。
