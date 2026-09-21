# 為何以 Object_FPS 建立 GYO Text 與可見 UI 閉環

> Historical record: this document preserves the implementation, commands and results from its original date. Paths and ownership may predate the current architecture; use [the current architecture](../architecture.md) and [build guide](../creating_apps.md) for present instructions. These results do not validate later revisions.

## 這次修改要回答的問題

Object_FPS 已經能進入遊戲並呈現世界，但普通啟動時缺少可見選單。這暴露的不是「少幾個畫面」而已，而是一個責任問題：文字究竟應由 GYO、SDL_ttf、Render backend，還是遊戲自己負責？

本輪的目標因此不是建立通用 UI framework，而是找出能讓目前遊戲形成閉環、又不會提前鎖死未來架構的最小責任切法。

## 為何 FontAsset 只保存 encoded bytes

字型需要 Asset ID、catalog、cache 與 lifetime，這些是 GYO 已經提供的 runtime asset mechanism，所以字型檔應該進入普通 Asset 流程。

但 `TTF_Font` 不能放進 `FontAsset`：

- 它會讓 Asset cache 依賴 SDL_ttf。
- native font 的建立與銷毀受具體 library lifecycle 約束。
- 未來替換 FreeType、DirectWrite 或其他 rasterizer 時會被既有 asset representation 綁住。
- AssetManager 會同時背負 identity、native decoding context 與 renderer 使用方式。

因此 `FontAsset` 只持有 engine-ready encoded TTF/OTF bytes。Asset 系統負責「這是哪個字型、何時載入與釋放」，rasterizer 才負責「如何把它變成像素」。

## 為何新增 ITextRasterizer，而不是 TextManager

目前真實需求只有：給定字型 bytes、UTF-8 文字與尺寸，取得可上傳的 RGBA bitmap。這已足以支援選單、HUD 與 Results，沒有證據需要一個掌管所有字型、layout、atlas、cache 與 rendering 的全域 manager。

所以本輪只建立 `ITextRasterizer` 與 owning `TextBitmap`：

```text
encoded font bytes + UTF-8 run + point size
                    |
                    v
             owning RGBA8 bitmap
```

選擇同步、whole-run rasterization，是因為它最直接地完成當前閉環，也能讓錯誤在 presentation 階段明確回報。它不是最終高效文字系統；當 profiling 證明 whole-run texture 數量或更新成本成為問題時，才有足夠資訊決定 glyph atlas、incremental upload 或 batching 應由誰擁有。

## 為何 SDL_ttf 是可選 adapter

SDL_ttf 已經能完成 font decode 與 glyph rasterization，GYO 沒有理由重新實作。GYO 真正需要的是穩定的輸入／輸出契約，而不是 SDL_ttf object model。

因此 SDL_ttf implementation 被放在可選 adapter 中，並在 `.cpp` 內處理 `TTF_Font`、`SDL_Surface` 與 SDL IO lifetime。公開 header 只看到 GYO 的 request、result 與 bitmap。

這個切法讓：

- GYO Core 和 neutral Text contract 可以在沒有 SDL_ttf 時配置與編譯。
- Object_FPS composition root 可以選用 SDL_ttf。
- 未來其他 rasterizer 可以新增 adapter，而不必修改 Asset、Render 或 Object_FPS UI policy。

## 為何不新增 Render::TextSubmission

現有 Render API 已經能接收 neutral `ImageView`、建立 opaque `TextureHandle`，並用 `SpriteSubmission` 畫出帶 tint 的矩形。對目前需求而言，文字 rasterization 結果就是一張普通 RGBA texture。

若現在新增 `TextSubmission`，Render layer 就必須過早決定 font identity、layout、glyph cache、fallback 與 batching 的 ownership。這些責任尚未被真實需求釐清。

所以本輪沿用既有 texture/sprite path：

```text
FontAsset
  -> ITextRasterizer
  -> TextBitmap
  -> IRenderDevice::CreateTexture
  -> TextureHandle
  -> SpriteSubmission
```

代價是 Object_FPS presentation 暫時持有 `{UTF-8, pointSize}` whole-run texture cache。這是刻意接受的局部成本，因為 MVP 的字串集合有限，而且 cache policy 可以在未來有數據後被替換。

## 為何選單與 HUD 定義留在 Object_FPS

`START GAME`、`CONTROLS`、HP 顯示方式、按鈕位置、選中顏色與點擊後的行為都屬於這個遊戲。把它們放進 GYO 會讓 Engine 開始知道某個遊戲有哪些 screen 與 menu item，並自然膨脹成通用 UI framework。

因此 `ObjectFpsUi` 是 game-owned presentation policy。它把 immutable game snapshot 轉成 quad/text commands；GYO 只提供輸入、文字 rasterization、resource lifetime 與 render submission mechanism。

這也解釋了為何沒有建立 widget tree、focus manager 或 generic menu class：目前只有一個 concrete game consumer，抽象化它們只會隱藏 Object_FPS policy，而不會帶來已證明的重用。

## 為何 hit-test 與畫面共用矩形來源

若 rendering 和 hit-test 各自維護按鈕位置，改版時很容易出現「看得到但點不到」或「空白處可以點擊」。因此 menu rectangle 由同一個 Object_FPS layout function 產生，Build 與 HitTest 共用。

滑鼠只有在絕對座標模式下移動或按下時才更新 hover。這是為了讓使用者用鍵盤改變 selection 後，不會被停在舊按鈕上的靜止游標於下一幀立即覆蓋。

## 為何 Pause 需要 release latch 與共同 draw order

Pause 經 Esc、Enter 或滑鼠 Resume 回到 Playing 時，觸發 Resume 的按鍵或滑鼠可能仍處於 held 狀態。如果直接交給 gameplay，玩家會在恢復後立即移動，automatic weapon 也可能立即開火。

因此本地 UI resume 與外部 `ResumeCommand` 使用相同的 movement/fire release rule：必須先看到釋放，Gameplay 才重新接受該輸入。

UI commands 原先分成「所有 quads」與「所有 texts」兩批提交，這會破壞建立時的交錯順序。在 Pause 畫面中，HUD text 因而可能在暗幕之後重新畫亮。保留共同 `drawOrder` 是針對這個實際問題的最小修復，不代表建立通用 retained UI scene graph。

## 為何只在 UI 將 Stage 編號加一

`ActiveStageSnapshot.ordinal` 同時被 presentation 用來索引 campaign geometry，所以其 runtime 語義是 0-based。直接改成 1-based 會污染內部索引並增加 off-by-one 風險。

玩家看到的關卡編號則應從 1 開始，因此轉換只發生在 Object_FPS HUD formatting：runtime 保留 index 語義，UI 負責 player-facing representation。

## 為何新增普通啟動的 menu smoke

原有 smoke test 會直接要求進入 Playing。即使 MainMenu 完全空白，只要世界 frame 能 render，測試仍然成功；headless GameFlow 測試也只能證明狀態轉移正確，不能證明 composition root 已把 UI 接到真正 render path。

`object_fps.menu_smoke` 因此保留普通 MainMenu 初始狀態，只增加「呈現第一幀後停止」與「submission 必須非零」的測試控制。它會實際經過 FontAsset、SDL_ttf adapter、texture upload 與 RenderQueue。

這仍不是 pixel-perfect 測試；它保證可見 UI path 已被提交並成功呈現，不能保證每張 GPU／driver 組合上的最終像素完全一致。

## 本次刻意接受的限制

- Whole-run text cache 適合目前有限字串，但不適合無界動態文字。
- 沒有 shaping、wrapping、bidi、fallback font、caret 或 rich text。
- 沒有 glyph atlas、SDF 或 per-glyph batching。
- 沒有通用 UI framework 或 `TextManager`。
- 完整 Object_FPS executable 仍受目前 Windows/MSVC SDL_GPU backend 限制；neutral Text 與 GYO Core 沒有這項限制。

這些不是遺漏的 placeholder，而是等待真實 consumer、資料量與 profiling 結果後再做的決策。

## 決策驗證

- Windows/MSVC Release 完整 build 成功。
- CTest `6/6` 通過，包含 Playing smoke 與 MainMenu first-frame smoke。
- Object_FPS headless `16 suites / 751 assertions` 通過。
- MinGW 可編譯 GYO Text 的 SDL_ttf adapter。
- 關閉 Object_FPS 與所有 SDL text/image/application adapter 後，MinGW core-only `engine` 仍可編譯。
