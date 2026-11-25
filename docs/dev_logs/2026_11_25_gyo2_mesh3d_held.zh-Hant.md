# 2026-11-25 Mesh3d 接作者持有的模型（`v8-arch-gyo2-mesh3d-held`）

Owner：大地圖線的任務卡（大地圖 S1／S2 設計審查 S-6，主會話轉達使用者「S4 同步做」，`v8-arch-gyo2-big-map-s4-parallel`）。分支 `claude/gyo2-mesh3d-held`（開向 `gyo2`）。作者 Opus 5.5／high。

## 前提

Engine 要解決：`Mesh3d` 只吃 `Handle<ModelData>`，gyo2 的 asset 沒有執行期加入（Bevy 的 `Assets<Mesh>::add`），所以遊戲在執行期產生的幾何（大地圖每個 region 一個網格）交不給它。作者自己繞不過去：遊戲端的 loader 拿不到索引，算不出 region 的世界座標（大地圖宣告 12.2）。

## 做了什麼

- `Render::ModelRef`＝`variant<Handle<ModelData>, shared_ptr<const ModelData>>`，`Mesh3d::model` 改用它。照 `ImageRef` 的先例：持有的那一份新的 `ModelData` 就是新模型。
- `ModelMeshKey` 多 `held`（位址）；`RenderModelMeshes::Use` 多 `holder`（保留期間握著它），新增 `EndFrame`：沒有任何 `Mesh3d` 畫的持有模型，在那一幀之後放掉網格（同 `RenderImages::EndFrame`）。資產的新 life 判斷只看資產的鍵。
- `mesh_render`：抽取時解析 `ModelRef`。持有的不需要來源的 `AssetView`。新增 System `EndModelMeshesFrame`，掛在 render_world 的 Cleanup、`ClearRenderFrame` 之後。
- render 頁：RD-9 與 `Mesh3d` 的說明。

## 驗證

- 本機（Intel Mac，preset `core`）：`gyo_render_tests` 42/42。新案例「a Mesh3d of a model the author holds…」：不用 asset 就畫得出來；畫著的時候網格只建一次、會保留；換成新的 `ModelData` 時，舊網格在那一幀之後放掉；拿掉 `Mesh3d` 後也放掉。
- 突變：新增 `mh-held-never-let-go`、`mh-end-frame-unmounted`、`mh-held-needs-view`。既有的 `p3-mesh-old-life-kept` 的 find 照新的判斷更新，守的性質不變。和既有的 `p3-mesh-twice`、`re-held-not-ended` 一起跑，6/6 killed，件數一致。
- 沒驗：
  - `test` preset 沒在本機建置（PvP 的 `mesh.model = handle` 靠 variant 的隱式轉換，交給 L1）。
  - GPU 上的實際畫面。

## 設計審查與修正（同日）

- 審查者 Opus／high（唯讀），報告在主 checkout 的 `build/evidence/gyo2-mesh3d-held/design-review.md`：B 0、S 3、N 6。審查確認：位址當鍵是安全的，因為 entry 還在時 holder 握著物件，同一個位址不會被別的 `ModelData` 拿走。
- S-1：`EndFrame` 改在 `ExtractMeshes3d` 的結尾呼叫，`RenderModelMeshes` 只有這一個使用者。刪掉 `EndModelMeshesFrame`，連同它讀 `RenderWorldOptions`、用字串指名 `ClearRenderFrame` 的耦合一起拿掉。
- S-2：新增 `Render::ResolveModel`，鍵與 holder 由同一處產生，同 `ResolveImage`。
- S-3：測試記錄每一幀送給後端的 mesh asks，驗證換模型那一幀是「建一個、放一個」，拿掉 `Mesh3d` 那一幀是「放一個」（`[mh-held-released]`）。
- N-1：和 Bevy 的差別寫進 RD-9。N-2：render 頁第 8.3 節「實體對應」。N-3／N-4：`MeshRenderModule.hpp` 的說明。N-5、N-6：照 `ImageKey` 的先例，不改。
- 突變：退役 `mh-end-frame-unmounted`（那個 System 已拿掉），新增 `mh-end-frame-skipped`。`mh-held-needs-view` 改指 `ResolveModel`。
- 重跑（修正後，只跑相關的）：`p3-mesh-old-life-kept`、`p3-mesh-twice`、`re-held-not-ended`、`mh-held-never-let-go`、`mh-held-needs-view`、`mh-end-frame-skipped` 6/6 killed，件數一致。`gyo_render_tests` 42/42。
