# 工具 v8：v8-tool-assets-anim-events（動畫事件的通用格式，手工階段）

日期：2026-10-31。分支 `claude/nifty-lovelace-dd931a`（切自 `origin/claude/v8-tool-handoff-cleanup` `3182ac53`，因為規劃文件 `assets.md` 還沒進 master）。檔位 high，引擎 Opus 5.5。估 6～8 h。

**狀態：** 程式、資料、測試與突變完成（macOS）。**使用者實聽未完成**：照任務的完成條件，聽過才算完成。

相關文件：[契約 animation-events.md](../architecture/animation-events.md)、[資產工具鏈規劃](../tools/plans/v8/assets.md)、[音效的 dev_log](2026_10_30_pvp_v8_product_sfx.zh-Hant.md)（`ReloadSoundOffsetSeconds` 的由來）。

## 做了什麼

推導順序（AGENTS §2）：功能＝換彈聲對上畫面；契約＝`gyo.anim_events`（片段上的時刻＋名稱）與 PvP 的「事件 → 音效」對照；架構＝GYO::Model 多一份契約與一條 PRIVATE 依賴。

- **Engine（`GYO::Model`）**：
  - `engine/model/AnimationEvents.hpp`：`ParseAnimationEvents`（不需要模型就能檢查的規則）、`BindAnimationEvents`（clip 存在、時刻不超過 clip 長度）、`BoundAnimationEvents::ForClip`。
  - 錯誤碼加 `InvalidAnimationEvents`，detail 是出錯值的路徑。
  - nlohmann_json 以 PRIVATE 連結；設定階段守衛 `gyo_expect_model_links`（連結只能是 Engine、Math、JSON）。
  - 名稱不解讀；不知道音效。重複的 JSON 鍵用解析回呼抓（DOM 會靜默保留最後一個）。
- **契約文件**：`docs/architecture/animation-events.md`（格式、驗證規則 7 條、Design Invariants 5 條、Replacement Criteria 3 條）；`docs/architecture.md` 的 GYO::Model 一列與邊界段落。
- **PvP 的資料**：
  - `weapons/nailer_v2/viewmodel/viewmodel.anim_events.json`：`Armature|Reload` 的 `magazine_in` 在 2.35 s，登錄為 `object_fps_pvp.anim_events.nailer_v2.viewmodel`（type `text`）。
  - `nailer_v2_viewmodel.json` 加選填的 `animation_events_asset_id`。
  - `audio/sfx.json` 升 v2：新增 `animation_events`（事件名稱 → `cue`＋`sync_seconds`）；`magazine_in` → `reload`，`sync_seconds` 0.33。
- **PvP 的程式**：
  - `WeaponPresentationDefinition` 載入、解析並綁定事件，檢查檔案的 `model_asset_id` 就是武器的模型；`WeaponViewModel::GetActionEvents`。
  - `SoundSettings::eventSounds`、`FindEventSound`、`EventSoundOffsetSeconds`（事件在 clip 中的秒數 ×（動作長度÷clip 長度）−同步點）。
  - `PvpSound` 在載入時檢查同步點不超過該 cue 的片段長度。
  - `PvpApplication`：換彈一出現在快照，就把 Reload clip 的每個事件換成 Engine 時間，交給 `GYO::Audio` 排程（對齊方式 (ii)）。已經過去的時刻會遲到、立即播出。
- **移除**：`ReloadSoundOffsetSeconds`、它的測試案例、突變 `sfx-reload-starts-at-start`、未再使用的 `PvpSound::ClipSeconds`。

## 時刻是怎麼定的

手工階段沒有工具可以看動畫，所以從資料推：

- 模型：`Armature|Reload` 長 3.7333 s，權威換彈 1.5 s（90 Tick），畫面上的 clip 等比拉伸（`WeaponViewModel` 依進度取樣），倍率 0.4018。
- 暫時的探針（測試檔，用完刪除）取樣 `Mag_j`（父節點 `main_j`）與 `side_j` 的局部變換，每 1/60 s：
  - 彈倉 0.57 s 起往下掉出，1.0～1.07 s 換手，1.22～1.32 s 移到彈倉口下方，2.22～2.37 s 推到底（2.3667 s 回到靜止位置）。
  - `side_j`（滑套）3.12～3.17 s 一次跳回。
  - 第一次只看全域座標，誤把 1.27 s（移到彈倉口）當成插入；改看局部座標才分清楚。
- 音檔 `gun_reload.wav`（0.958 s）：兩組聲音，峰值約 0.33 s 與 0.63 s，相隔 0.30 s。
- 拉伸後：插到底 0.944 s、滑套 1.266 s，相隔 0.32 s，和音檔兩組聲音的間隔一致。所以第一組＝彈倉到底、第二組＝滑套。
- 結果：`magazine_in`＝2.35 s、`sync_seconds`＝0.33。聲音在換彈 0.614 s 開始，第一組落在 0.944 s，第二組約 1.244 s（比滑套早約 22 ms）。
- 對照舊規則（聲音結尾對齊換彈結束，從 0.542 s 開始）：第一組約 0.87 s（早 70 ms），第二組約 1.17 s（早約 100 ms），可能就是「仍覺得違和」的原因。這是推論，以實聽為準。

## 驗證

- `gyo_model_tests`：24 案通過（新增 9 案，對應驗證規則與綁定）。
- `gyo_object_fps_pvp_tests` 的音效案例、`presentation_cpu` 新增 2 案（正式資料綁定成功；其他模型、缺 clip、版本 2 都拒絕）。
- 突變：
  - Engine（`docs/tools/plans/v8/scripts/anim_events_mutations.json`，以 time-threads-trace 的 runner `--catalog` 執行）：13／13 killed。
  - PvP（`tests/object_fps_pvp/mutations.json`，batch `v8-tool-assets-anim-events`）：8／8 killed。
  - 經緯：第一次 Engine 8／13、PvP 6／8。
    - 3 個存活：`-tc` 的名稱含逗號，doctest 用逗號分隔篩選條件，結果什麼都沒跑就通過。篩選改成不含逗號的片段，目錄的說明加上這條。
    - 1 個 build_failed：刪掉整行讓 `for` 沒有本體；改成 `static_cast<void>(item);`。
    - 3 個 wrong_failure：PvP 兩個是 doctest 把 `const char*` 的訊息印成指標（改成 `std::string`）；Engine 一個是 CMake 把錯誤訊息折行（期望改成 `(?s)…expected\s+exactly`）。
    - 修正後重跑這 7 個，全部 killed。
- 完整 CTest（macOS，test preset，`GYO_APPS=object_fps_pvp`）：100／100 通過。

## 範圍的限制與沒做的

- `PvpApplication` 的排程沒有單元測試（換算由 `EventSoundOffsetSeconds` 的測試守住，呼叫處靠實聽）。
- 音檔是兩組聲音合在一個片段裡，只能用一個事件對齊；第二組（滑套）跟著第一組走。要各自對齊，需要把音檔切成兩個（音效工具的裁剪，`v8-tool-audio-editor-edit`），再加 `slide_release` 事件。資料已支援，不用改程式。
- 第三人稱（其他玩家）的換彈沒有聲音，和之前一樣。

## Architecture Delta

- GYO::Model 多一份 Data Contract（`gyo.anim_events` v1）與 Design Invariants；新增依賴邊：GYO::Model → nlohmann_json（PRIVATE，和 Render、Ui 相同）。方向不變，GYO::Model 不依賴 Audio。
- PvP 的內容契約：`sfx.json` v1 → v2；武器 presentation 加選填欄位；catalog 多一個 text 條目。
