# 測試 fixture：buns 髮型配件

Owner：`object_fps_pvp`。使用者：`tests/object_fps_pvp/PlayerPresentationTests.cpp`（CTest `object_fps_pvp.presentation_cpu`），用來驗證配件的綁定與擺放。

產品的人物沒有配件，所以這些檔不隨產品出貨（[PvP 素材標準](../../../../docs/object_fps_pvp/assets.zh-Hant.md)第 7 節）。測試把本資料夾的 `asset_catalog.json` 附加到組裝後的產品 catalog 上；id 沿用原本在產品 catalog 裡的 id。

| 檔案 | 來源 | 授權 |
|---|---|---|
| `Hair_Buns.fbx` | Universal Base Characters Standard pack（FBX 內嵌的原檔路徑：`Universal Base Characters\Hairstyles\Hairstyles_Female.blend`） | CC0（pack 授權記在 [assets.zh-Hant.md 附錄 A](../../../../docs/object_fps_pvp/assets.zh-Hant.md#附錄-a素材包清單與授權記錄)） |
| `textures/T_Hair_2_BaseColor.png` | 同上（與已移除的 v1 單機產品 superhero_female 的貼圖是同一個檔；該產品 2026-11-12 移除） | CC0 |
| `character.json` | 在 repo 裡手寫 | — |

2026-11-01 由 `v8-product-asset-buns-fixture` 從 `assets/object_fps_pvp/characters/hairstyles/buns/` 與 `characters/superhero_female/textures/` 搬來，內容不變。
