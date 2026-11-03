# Engine v8：刪除 assets/common（v8-engine-assets-common-retire）

日期：2026-10-31。分支 `claude/v8-asset-layout`（PR [#125](https://github.com/yojinn-io/GYO-Engine/pull/125)）。計畫：[asset-layout](../object_fps_pvp/plans/v8/asset-layout.md)。

**狀態：完成。**

## 為什麼

- 支撐的 Feature：沒有。使用者同意的清理（`assets-common-retire`）。
- `assets/common`（`asset_catalog.json` 與 `white1x1.png`）沒有 `content.json`，沒有 owner；build、程式、測試都沒有引用它。`common` 又是 registry 的保留名（`build/cmake/GyoAppRegistry.cmake`），不會有 product 叫這個名字。
- 它是 2026-09-16 的 commit `545a86a7`（修 CLion 建置）留下的。`white1x1.png` 的同一個 blob（`818c71d0`）已經是 `object_fps`、`object_fps_v2` 各自 catalog 裡的 `textures/common/white1x1.png`，id `common.texture.white` 由它們自己的 catalog 提供。

## 做了什麼

- 刪除 `assets/common/asset_catalog.json`、`assets/common/white1x1.png`。

## 驗證

- 刪除前 `git grep "assets/common"` 在 `docs/` 以外為 0 筆。
- `docs/` 裡有 4 處寫「遊戲不掛載 `assets/common`」（`docs/architecture/3d-assets.md`、`docs/creating_apps.md`、`docs/rendering_architecture.{ja,zh-Hant}.md`）。它們講的是「不依賴共用素材目錄」的規則，刪除後仍然成立，這次不改；要不要改寫由文件的 owner 決定。
- 驗收：合併時的 CI 全量。

## 範圍的限制

- 停用的 `object_fps`、`object_fps_v2` 裡的 `textures/common/white1x1.png` 不動（凍結）。
