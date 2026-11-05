# PvP v8：補回「只被測試引用的素材」守衛的檢出力（v8-product-asset-guard-product-use）

日期：2026-11-05。分支 `claude/fix-asset-guard-product-use-eb5795`（從 master `4a88f67f`）。計畫：[asset-layout](../object_fps_pvp/plans/v8/asset-layout.md)。標準：[PvP 素材標準](../object_fps_pvp/assets.zh-Hant.md)。

**狀態：完成。**

## 為什麼

- 突變 `v8-asset-layout-guard-product-use`（`tests/object_fps_pvp/mutations.json`，批次 `v8-asset-layout-guard`）在 master 上一直存活：
  它把 `PRODUCT_ROOTS` 換成 `CODE_ROOTS`，意思是「測試裡的引用也算產品使用，只給測試用的素材因此會出貨」，違反素材標準第 7 節與 DI-1。
  hitbox-acceptance 線的 K3 完整突變（852／862）首先發現它，v8 主會話的 `v8-product-event-sounds-migrate` 因為改到同一個測試檔，依 D184 重跑時再次撞到，事前宣告「範圍全部 killed」因此不成立。使用者決定另開一張卡處理。

## 原因（查明後才改）

三個候選原因逐一核對：

| 候選 | 結果 |
|---|---|
| 守衛檢出力不足 | **成立。** 這條突變原本靠 `LAYOUT_EXCEPTIONS` 裡 buns 的 3 筆 `product-use` 例外被抓到：把 tests 算進產品使用後，這 3 筆變成過期例外，`test_every_exception_is_still_needed` 失敗。`v8-product-asset-buns-fixture`（commit `33ecff1b`）把 buns 搬成 fixture、刪掉這 3 筆例外之後，catalog 裡再也沒有「只被測試引用」的項目：`test_every_id_is_used_by_the_product` 放寬根目錄只會讓發現變少，不可能失敗；既有自測 `test_the_product_use_check_finds_an_id_the_product_does_not_use` 的 orphan id 沒有任何測試引用，兩個根目錄算出來一樣。 |
| 等價突變 | 不成立。repo 裡 `tests/object_fps_pvp/fixtures/hair_buns/` 的 3 個 id 正是「只被測試引用」的素材（`PlayerPresentationTests.cpp`、`PlayerAnimationEquivalenceTests.cpp` 以字串引用，`apps/object_fps_pvp/` 沒有引用），兩種根目錄對它們的判定不同。 |
| 預期訊息指錯測試 | 部分成立：`expect` 指的 `test_every_exception_is_still_needed` 在例外刪掉後已不可能因這個突變失敗，但換成別的既有測試也抓不到，所以單改 `expect` 不夠。 |

結論：守衛本身的判定沒有錯，是「只被測試引用」這個邊界沒有任何資料在守，只能靠例外表順帶證明。

## 做了什麼

- `tests/object_fps_pvp/test_asset_catalog.py`：
  - 新常數 `TEST_ONLY_CATALOG`，指向 `tests/object_fps_pvp/fixtures/hair_buns/asset_catalog.json`（第 7 節規定測試專用素材的位置）。
  - 新自測 `test_the_product_use_check_ignores_a_reference_from_a_test`：讀 fixture catalog 裡的 model 與 texture（text 項目會被 `used_ids` 讀內容，而它的路徑不在 `assets/object_fps_pvp/` 下，所以排除），斷言三件事：
    1. 以全部 code root（apps＋tests＋acceptance）掃，這些 id **有**被引用（證明 fixture 真的是測試在用，不是沒人用的孤兒）；
    2. 只以產品根目錄掃，這些 id **沒有**被引用；
    3. `product_use_findings` 對它們全部回報 `product-use`。
    突變把 `PRODUCT_ROOTS` 換成 `CODE_ROOTS` 時第 2 點必定失敗。資料來自 repo 的實際 fixture，不是 Agent 憑理解寫的合成字串（AGENTS §14.2）；若將來有人讓產品程式引用 fixture 的 id，或測試不再引用它，這個自測也會失敗並指出 fixture 已不是第 7 節的例子。
- `tests/object_fps_pvp/mutations.json`：這條突變的 `expect` 從 `FAIL: test_every_exception_is_still_needed` 改成 `FAIL: test_the_product_use_check_ignores_a_reference_from_a_test`。`find`／`replace`／`what` 不變。

## 判定變更的報告（AGENTS §14.4，事後）

既有測試的判定都沒有改。`test_every_id_is_used_by_the_product`（DI-1 的守衛）、`test_every_exception_is_still_needed` 的程式與判定原樣；新增的自測只締不緩；突變定義只換了預期的測試名稱，指向唯一一個能抓到它的測試，沒有換成泛用訊息。

1. 守的性質：出貨的 id 要被 `apps/object_fps_pvp/` 的程式用到，只被測試引用的不算（DI-1、第 7 節）。
2. 緩和的地方：沒有。
3. 收緊的地方：守衛把測試引用算成產品使用的實作錯誤，從「沒有任何測試會失敗」變成自測失敗。
4. 沒有緩和，不適用。
5. 根據：下節的 Red／Green 與批次重跑。

## 驗證

- Red：改動前在本工作樹跑 `run_mutations.py --only v8-asset-layout-guard-product-use`：survived（和 master 的證據 `v8-hitbox-acceptance-mutations/master-10.out` 一致）。
- Green：加自測並改 `expect` 後同一條：**1／1 killed**。
- D184 範圍：同檔案的整批 `v8-asset-layout-guard` 重跑，**13／13 killed**（含這一條）。沒有新增突變；其他批的 `find` 都不在改動範圍。
- `python3 tests/object_fps_pvp/test_asset_catalog.py`：28 個測試通過（原 27＋新 1）。
- CTest `cpu|shader`（本工作樹的 test preset，重新建置後）：115／115 通過。
- 證據：主 checkout 的 `build/evidence/v8-product-asset-guard-product-use/`（`before-fix.out`、`after-fix.out`、`after-fix-report.json`、`batch-rerun.out`、`batch-rerun-report.json`、`build.log`、`ctest-cpu-shader.log`，git 不追蹤）。

## 範圍的限制

- K3 切分記到的另 5 條過時突變（Go 的 uint32／uint64 4 條、`v71hf-damage-bounds-maximum-unchecked` 1 條）不在這張卡，沒有動。
- v8 主會話的分支也改了 `test_asset_catalog.py` 的 `DEFINITION_SUFFIXES` 一行（加 `.event_sounds.json`）；本批沒有碰那一行，合併時兩邊都保留即可。

## Architecture Delta

沒有。只動產品自己的測試、它的 fixture 引用與突變清單。
