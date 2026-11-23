# 合併前核准：`transform` 與 `camera` Module（SDL 線子批 X1，`v8-arch-gyo2-sdl-transform-camera`）

更新：2026-11-23。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等使用者合併前核准。** 宣告：[transform-camera-declaration.zh-Hant.md](transform-camera-declaration.zh-Hant.md)（修訂 1，2026-11-23 動工前核准）。紀錄：[dev_log](../../../dev_logs/2026_11_23_gyo2_sdl_transform_camera.zh-Hant.md)。

## 1. 判定（宣告第 11 節，先宣告、不放寬）

| 判定 | 結果 |
|---|---|
| 第 6.1 節的測試在 L1 三平台通過、既有測試不退步 | 本機（macOS x64）`ctest -L cpu` 198/198 通過兩次（補測試之前與之後）。L1 三平台：draft PR 手動 dispatch 後補記 |
| 突變全部 killed（只算預期的失敗） | 最後 45/45（transform 11、camera＋math 34）。第一輪 25 個中 `tf-initial-identity` 存活（T-1 沒有隔離），把 T-1 的同步掛到 `First` 後 killed；之後依敵對式檢查加 20 個，重跑時三個 `wrong_failure`（拿掉 set 成員時 Kernel 在啟動報 D-E4，案例的標籤沒印出來），讓那兩個案例在建置失敗時也帶標籤後 killed。都是測試的寫法，判定沒有改 |
| 效能 | 不宣告（沒有出處的預算） |

## 2. 和宣告不同的地方（請一併確認）

1. **Math 多一個函式 `Multiply(Matrix4, Vec4)`**（完整的齊次乘法，不除 w）。宣告只列了 `MakeOrthographic`（第 2.1 節 X1-3、第 7 節第 3 項 (b)）。理由同第 4.4 節：座標換算要齊次乘法，Math 已有 `Vec4`、`MakePoint4` 卻沒有這個運算，寫在 `camera` 的私有程式裡，R2b 一定再寫一份（「一個運算一個實作」）。是加法、不改既有函式；math.md Conventions 的 Matrices 一行寫明。除以 w 的 `Project` 仍在 `camera` 私有（只有一個使用者），R2b 要用時再判斷搬不搬。
2. **錯誤型別**：宣告寫的是純 enum `ViewportConversionFailure`；`Base::Result` 要求錯誤型別滿足 `CodedError`，所以實作是 `Base::Error<ViewportConversionFailureCode>`，訊息留空（每幀換算失敗不配置記憶體）。
3. **`Compute` 是公開函式**（宣告沒列）：`UpdateCameras` 用它算 `ComputedCamera`；只回傳值、不寫任何東西，給不在 World 裡的相機與測試用。camera 頁寫明。
4. **突變的數目 45**：等於「變的 DI 條文 9 條 × 5」的上限（`tests/common/mutation/README.md` 第 8 條），沒有超過。

## 3. 要使用者定的（設計審查提出；審查者 Opus／high，和作者同模型——Fable 用量不足）

### 第 1 題：「回滾用的相等必須逐位元」由誰當唯一出口

**例子**：作者把自己的 `Velocity{float x, y}` 登記成可回滾，用預設的 `==`。某一步算出 −0、放回時是 +0：`Restore` 判成相同而不寫，摘要卻逐位元比，直跑與重跑的摘要不同（R-2 破）。

**現況**：X1 在 `Transform` 上自己給逐位元的 `==`；這條規則本身來自回滾的契約，X1 已照 AGENTS §1 登記成 [CCR-13](../../../cross-component-rules.md)（owner 回滾），每個型別的作者照它寫。

| 做法 | 內容 | 代價 |
|---|---|---|
| **(a) 照現況：CCR-13 登記，各型別自己寫逐位元的 `==`** ←建議 | 不改回滾 | 每個含 float 的作者型別都要記得；寫錯時沒有守衛（審查看） |
| (b) 回滾預設改用逐位元比較（或用 `HashValue` 的位元比），成唯一出口 | 回滾那一批改 `Registrations.hpp:234` 的預設比較 | 改回滾 Module 的已核准契約（R-2 的比較），另開一批；`Transform` 的逐位元 `==` 之後可以拿掉 |

**建議 (a)**：本批不碰回滾；(b) 是回滾那一側的機制改動，若要做交主會話另排一批。

### 第 2 題：`Restore` 不補初始值的洞，由各 Module 自己補

`transform` 用 OnAdd hook 補（回滾讓角色復活時 `GlobalTransform` 不會消失）；`camera` 的初始值鏈有同一個洞但不補，camera 頁寫明「相機不支援被 `Restore` 復活，要回滾就把 `CameraTarget`、`ComputedCamera` 也登記」。RC-TF2 改成「第二個 Module 也要寫同樣的 hook 時停下來交使用者」（候選：Kernel 加必要元件，或由 `Restore` 的呼叫者一處決定推導，DI-KS 12）。**請確認這兩段文字**（屬 DI／RC）。

## 4. Design Invariants 的最終文字

在 [transform.zh-Hant.md](transform.zh-Hant.md)（TF-1～TF-4、RC-TF1～3）與 [camera.zh-Hant.md](camera.zh-Hant.md)（CM-1～CM-5、RC-CM1～2）。和宣告第 8 節草案的差：TF-1 加「預設在 `Main`／`PostUpdate`」、TF-2 引 CCR-13、RC-TF2 改寫（第 3 節第 2 題）、CM-3 加「每個相機讀自己目標的大小」。

## 5. 檢查與審查

- 宣告的敵對式檢查（Opus／high，同模型）：B1 S9 N12，全部採用（宣告第 14 節）。
- 突變清單的敵對式檢查（§14.2；Opus／high，同模型）：找到 27 個存活候選（實際跑之前推論的），分類後補了測試與 20 個突變（契約要補的全部、逐欄細項只補 translation 與各型別的雜湊 assert、不加突變）；不補的：Prepare 不蓋帶過來的 Resource（要 `CarryOver` 的測試環境，影響是空的 Resource，記在 dev_log）。
- 設計審查（Opus／high，同模型）：要使用者定的 3 件（第 2 節第 1 項、第 3 節兩題）；卡片可改的 4 件已改（錯誤訊息留空、`SameComputed` 移進 .cpp、`Compute` 的用途寫明、宣告的偏離表另補三列在本頁第 2 節與 dev_log）。
- 審查報告未另存檔（重點寫在本頁與 dev_log）。
