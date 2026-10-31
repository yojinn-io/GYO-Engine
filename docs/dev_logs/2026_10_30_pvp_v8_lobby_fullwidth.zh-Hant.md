# PvP v8：v8-product-lobby-fullwidth（大廳位址欄的全形轉半形）

日期：2026-10-30。分支 `claude/v8-tool-ui-ime-observe`（[PR #111](https://github.com/yojinn-io/GYO-Engine/pull/111)）。檔位 medium，引擎 Opus 5.5。估 1～2 h，實際約 0.5 h。由工具線會話做（使用者指定，HANDOFF `v8-tool-ime-plan`）。

**狀態：** 完成（CI 與 Mac 上的單元測試）。Windows 用日文輸入法實際打位址（L3）**還沒做**。

相關文件：[工具線 README](../tools/plans/v8/README.md)「檢測之後：輸入法的規劃」、[輸入法檢測的 dev_log](2026_10_30_tool_v8_ui_ime_observe.zh-Hant.md)。

## 為什麼

Windows 的檢測 log（`docs/tools/plans/v8/evidence/ime-observe/`）顯示：日文輸入法在平假名模式確定的是全形字元（`．` U+FF0E、`。` U+3002、`０` U+FF10、`１` U+FF11、`：` U+FF1A）。大廳的過濾逐 byte 只收 ASCII，這些字被靜靜丟掉，冒號「打不出來」。

（後續，2026-10-31）`docs/tools/plans/v8/evidence/ime-observe/` 已移出 repo（AGENTS §15），現在在主 checkout 的 `build/evidence/tool-v8-ime-observe/report.txt`（使用者的 Intel Mac，git 管理外）；腳本的分類全文抄在 [2026_10_30_tool_v8_ui_ime_observe](2026_10_30_tool_v8_ui_ime_observe.zh-Hant.md)「判定為什麼不成立」一節。

## 做了什麼

- 大廳的過濾從 `PvpApplication.cpp` 的 `AppendAddress` 搬到 `RetroFPS/Pvp/AddressText.hpp` 的 `FilterAddressText`（純函式，可以測試）。`AppendAddress` 改呼叫它，128 字的上限照舊。
- 先把 UTF-8 解成字元；全形英數與標點（U+FF01～U+FF5E）換成對應的 ASCII，`。`（U+3002）換成 `.`，再用原本的集合（a-z A-Z 0-9 . : - [ ]）過濾。其他字元、以及不完整的 UTF-8，只丟掉自己。
- 這是產品的規則（`v8-tool-ui-edit-policy-owner`：過濾由產品負責）；Engine 沒有改。

## 驗證

- Red：先把舊規則原樣搬進 `FilterAddressText`，新測試在舊規則下有 4 個斷言失敗，都是全形相關的，正是 log 裡的現象。
- 經過：第一次跑時，ASCII 的測試也失敗 1 個：我把 `"127 0/0\t1!"` 的期待值寫成 `12701`，正確是 `127001`。是測試數錯，改正期待值。
- Green：新規則下全部通過；產品的 CPU 測試 `gyo_object_fps_pvp_tests` 全部通過。
- 突變 5 個，全部被抓到：
  - 不轉全形
  - 不轉 `。`
  - 拿掉「只收 ASCII」的檢查（`あ` 會被截成 `B`）
  - 全形的位移算錯
  - 不完整的序列連後面的字一起跳過
  - 最後一個第一次存活，補了「壞掉的序列只丟自己」的測試後被抓到。

## 未做

- L3：Windows 上用日文輸入法打「127.0.0.1:8080」，確認位址欄顯示正確。可以和 `v8-engine-input-composition` 的 L3 一起做。
- 組字中的字仍然看不到（要等組字事件與 `text_field`）。
