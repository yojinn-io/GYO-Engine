# 姿態（`engine/modules/transform`）

更新：2026-11-23（新頁，`v8-arch-gyo2-sdl-transform-camera`）。由來：[事前宣告](transform-camera-declaration.zh-Hant.md)（SDL 線子批 X1，2026-11-23 動工前核准）第 4.1、8 節。**Design Invariants 的最終文字：使用者 2026-11-23 合併前核准**（[合併前核准頁](transform-camera-merge-approval.zh-Hant.md)）。

Owner：架構。程式：`engine/modules/transform/`（`GYO::Engine::Transform`，target `GYO::Transform`，登記單位 `TransformModule`）。行為測試：`tests/common/transform/`。

## 一個例子

2D 遊戲的節點是一個帶 `Transform` 的實體：`translation` 的 x、y 是位置，z 決定前後（2D 相機的預設是 z 大的在前面，見 [camera](camera.zh-Hant.md)）。生成的那個同步點，`GlobalTransform` 就已經等於 `Transform` 的矩陣。之後改 `Transform`，`GlobalTransform` 在同一幀的 `PostUpdate` 跟上；`PostUpdate` 之前讀到的是上一次的值。

FPS 的角色把 `Transform` 登記成可回滾（`rollback.Component<Transform>()`，不必給 `DataOptions::equal`）。回滾讓預測中被打死的角色復活時，`GlobalTransform` 在那次放回裡就補上了。判定只讀 `Transform`，不讀 `GlobalTransform`（[CCR-12](../../../cross-component-rules.md)）。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Transform
```

- 登記檔一列：`library_only=no`、`default=yes`、`depends` 空。
- 標頭：`Transform.hpp`（`Transform`、`GlobalTransform`、`ToMatrix`、逐位元的 `==`、雜湊）、`TransformModule.hpp`（登記單位、`TransformOptions`、公開的 set 名 `kPropagateSet`）。
- namespace 與型別同名（`GYO::Engine::Transform::Transform`）：在別的單位裡寫 `Transform::Transform`（N13 的例外，同 `IO::Path`）。

## 2. 用法與後果

- **`Transform`**：位置 `Vec3`、旋轉 `Quaternion`、縮放 `Vec3`（照 Bevy）。相對父的局部姿態；沒有父（X2 之前一律沒有）時等於全域。
- **`==` 是逐位元的**：每個 float 比 bit，−0 與 +0 不同、同樣 bit 的 NaN 相等，和雜湊一致。用 float 的比較，`Restore` 會把 −0 與 +0 當成相同而不寫，摘要就和直跑不同（回滾 R-2；規則本身是 [CCR-13](../../../cross-component-rules.md)，owner 回滾）。
- **`GlobalTransform`**：全域矩陣（`Math::Matrix4`；Bevy 是 `Affine3A`）。只由這個 Module 寫：
  - 初始值：`Transform` 出現（生成的 bundle、往沒有它的實體插入）的那個同步點，Kernel 放 `ToMatrix(transform)`。
  - OnAdd hook：`Restore` 不補初始值（Kernel DI-KS 12），hook 在放回時補上；一般的生成時初始值已經放了，hook 不做事。
  - `SyncTransforms`：`Transform` 被寫過（`Changed<Transform>`）就重算。預設掛 `Main`／`PostUpdate`，在 set `TransformPropagate` 裡；同一個 stage 要讀 `GlobalTransform` 的 System 排在它之後。
- **`Changed<GlobalTransform>`** 的意思是「`Transform` 被寫過」（值相同也算，同 Bevy；[CCR-8](../../../cross-component-rules.md)），不是「真的變了」。
- **兩條路都只補「沒有」的情形**：實體已經留著舊的 `GlobalTransform`（例：拿掉 `Transform`、留下 `GlobalTransform`，之後又放回 `Transform`）時，放回那一刻是舊值，到下一次傳播才對。
- **準備函式的 `World::Insert`** 不跑 hook、不補初始值：那樣放的實體**一直**沒有 `GlobalTransform`（`SyncTransforms` 只寫已有的），要自己放（[System 契約說明頁](system-contract-page.zh-Hant.md)第 17 節）。
- **骨架關掉的 World**：用 `TransformOptions{schedule, stage}` 指名；沒有那個 stage 是 Kernel 的 D-E1。
- **非等比縮放**：X2 加上父子之後，父的非等比縮放配上子的旋轉會出現剪切（矩陣照樣相乘，同 Bevy 的 `GlobalTransform`）。

## Design Invariants

守衛欄：行為測試（ctest `transform.module`）與突變（`docs/architecture/target/v2/scripts/transform_mutations.json`，`tf-`）。綁實作方式的守衛（`gyo_expect_links`）照 `entity_ids` 的先例立：它是建置圖的事實。

<a id="tf-1"></a>
**TF-1 職責。** 只有 `Transform`、`GlobalTransform`、補 `GlobalTransform` 的初始值與 hook、同步（X2 起加傳播）的 System 與公開的 set（預設在 `Main`／`PostUpdate`）。不做：父子關係（Kernel，子批 KR）、回滾／複製／權威的登記（作者與網路那一側）、內插、畫面。守衛：T-9、「a System ordered after TransformPropagate…」；突變 `tf-set-membership-missing`、`tf-default-stage-last`；審查。

<a id="tf-2"></a>
**TF-2 `GlobalTransform` 是推導值，`Transform` 是作者的值。** `GlobalTransform` 只由這個 Module 寫（初始值、hook、同步／傳播的 System）；雜湊不寫內容；這個 Module 不把它登記回滾、權威、複製；判定不讀它（CCR-12）。`Transform` 的雜湊逐欄、`==` 逐位元，兩者一致。守衛：T-7（回滾官方策略的等價案）、T-8、T-9、`==` 的位元案；突變 `tf-hash-global-nonempty`、`tf-hash-transform-drops-scale`、`tf-eq-float-compare`。

<a id="tf-3"></a>
**TF-3 沒有父時等於本身。** 實體原本沒有 `GlobalTransform` 時，`Transform` 出現的那個同步點、`Restore` 放回之後，`GlobalTransform` 就在、而且 `matrix == ComposeTRS(transform)`；每次 `TransformPropagate` 跑完，沒有父、帶兩者的實體都相等。守衛：T-1、T-2、T-3、T-5、T-6、T-7；突變 `tf-initial-identity`、`tf-sync-not-mounted`、`tf-sync-ignores-changed`、`tf-restore-hook-missing`、`tf-hook-identity`。

<a id="tf-4"></a>
**TF-4 依賴。** 只依賴 base、math、kernel、orchestration。守衛：`gyo_expect_links(gyo_transform GYO::Base GYO::Kernel GYO::Math GYO::Orchestration)`（`tests/common/transform/CMakeLists.txt`，configure 時檢查）；突變 `tf-links-more`（陽性對照）。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」。

- **RC-TF1**：傳播超過 R2b 宣告的幀時間預算（計測）。
- **RC-TF2**：要第三條路才保得住「有 `Transform` 就有 `GlobalTransform`」（初始值、hook 之外），或第二個 Module 也要寫「`Restore` 時補推導元件」的 hook（同一件事散到各 Module）。候選：Kernel 加照 Bevy 的必要元件，或由 `Restore` 的呼叫者一處決定推導（DI-KS 12）。
- **RC-TF3**：判定需要讀 `GlobalTransform`（它不再是推導值）。
