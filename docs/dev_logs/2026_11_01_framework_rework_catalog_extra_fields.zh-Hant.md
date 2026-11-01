# framework 整改：v8-framework-catalog-extra-fields（更新條目時保留其他欄位，D-6）

日期：2026-11-01。分支 `claude/framework-rework`。檔位 high（主會話，Opus 5.5），沒有子 Agent。估 2～3 h，實際約 1.5 h。

**狀態：** 完成（Mac 上的測試、突變、全量 CTest）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[README](../architecture/plans/framework-rework/README.md) 的 D-6、[PLAN 第 2.5 節](../architecture/plans/framework-rework/PLAN.md)。

## 做了什麼

- **缺陷**：JSON 綁定的 `RegisterAsset` 更新既有條目時，把整個條目文字換成只有 id、type、path 的新物件，其他欄位消失。內容契約保留未知的選用欄位（cases.json `valid-custom-type-and-metadata`）。這段程式是上一批從舊的 `CatalogRegistration` 照搬的。
- **修正**（`framework/bindings/json/src/CatalogText.cpp`）：更新時只改條目最上層 `type` 與 `path` 兩個值的字串；巢狀物件裡同名的成員不算。寫完後的比對改成「原條目只換 type 與 path」。新增條目的寫法不變。
- **不修的**：舊的 `CatalogRegistration` 有同樣的缺陷，但 audio_editor 在 T3b 前還用它，修它會改變工具的行為。它在 v8-framework-invariants 刪除，audio_editor 在 T3b 換到新 API 時得到修正。

## 驗證與經緯

件數の突き合わせ：一致（宣告 3・執行 3・killed 3、stale 0；另依 D184 重跑 fw-cat-edit-other 1・1・1）。

- **§14.3**：E1（巢狀同名成員、要跳脫的字元、整份文字逐位元組比對）、E2（鍵的順序不同、冒號前有空白、政策允許換 type）、E3（契約案例更新後仍保留 `author`）修正前都失敗（Red），E3 直接顯示 `author` 被丟掉；修正後通過（Green）。
- **宣告的判定不成立一次**：修正後上一批的 C3 失敗。C3 照抄舊測試，預期更新後整個條目被改寫成一行；修正後條目保留原本的兩行排版、只換 path 的值。依 §14.1 先回報。使用者核准把 C3 的預期改成「只換值」（§14.4：放寬面＝保留排版只換值的輸出以前算失敗；收緊面＝整個條目被改寫的輸出以前通過；整份文字仍逐位元組比對，額外欄位由 E1～E3 守住）。
- **突變前補測試**：E1 的巢狀成員排在條目自己的成員後面，拿掉深度判斷的突變會先找到最上層的鍵而殺不到，所以補 E4（巢狀成員排在前面）。
- **突變**：更新時又換掉整個條目、成員搜尋不看深度、寫完後的比對預期整個新條目，3/3；fw-cat-edit-other 重跑 1/1。宣告前預檢 find 都恰好一次。
- **全量 CTest** 119/119。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-framework-catalog-extra-fields/`（git 不追蹤，見該資料夾的 README）。
