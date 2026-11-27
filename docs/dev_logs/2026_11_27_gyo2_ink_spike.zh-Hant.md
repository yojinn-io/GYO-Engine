# 2026-11-27 Ink spike：I1 接線與台詞、I2 狀態與場景（`v8-arch-gyo2-ink-spike`）

Owner：Ink spike 任務卡。分支 `claude/gyo2-ink-spike-9fa94f`（PR #366），開向 `gyo2`，途中 merge 了 gyo2 `3825f51b`。作者 Opus 5.5／high。宣告：[ink-spike-declaration.zh-Hant.md](../architecture/target/v2/ink-spike-declaration.zh-Hant.md)；owner 頁：[ink.zh-Hant.md](../architecture/target/v2/ink.zh-Hant.md)。

## 前提（使用者的決定，經主會話轉達）

1. 「全部照推薦，空殼刪掉」：U1～U4 照推薦，卡片判的項目沒有推翻（HANDOFF `v8-arch-gyo2-ink-spike-u1-u4`）。
2. 「開始」：#366 的宣告算動工前核准（HANDOFF `v8-arch-gyo2-ink-spike-start-impl`）。下載只限兩樣：CI clone inkcpp d38608d，本機下載 `inklecate_mac.zip`；要 .NET 就先停。

## I1 接線與台詞

- 下載：`inklecate_mac.zip` 29,173,014 bytes，SHA256 `c516402b…`，和宣告一致。放主 checkout 的 `build/tools/inklecate-1.1.1/`。是 self-contained 的 x86_64 程式，不需要 .NET（這台 Mac 沒有 `dotnet`）。沒有其他下載：inkcpp 用本機的 clone；yyjson、freetype 等用 `gyo2-big-map-declaration-65f33f` 已經抓好的來源，逐一核對過版本與 SHA256。
- `config/plugins.csv` 加 `ink` 列；`plugins/ink/CMakeLists.txt` 用 FetchContent 取 inkcpp、自己列 source，建 `gyo_inkcpp`、`gyo_inkcpp_compiler`（只給測試）、`GYO::Ink`。
- adapter `src/adapters/inkcpp/Dialogue.cpp`：`LoadStory`、`Start`、`Advance`、`Choose`，tag 的拆法，指令的綁定，錯誤。
- 途中發現的事：
  - 選項的 tag 寫在 `]` 之後，照 ink 的語法是選了之後那段輸出的 tag。fixture 改成寫在 `[...]` 裡。
  - inkcpp 在選項前會多給一個空行：沒有文字也沒有 tag 的行，adapter 不交出去。
  - **D1**：已經選過的一次性選項，它的 tag 給了下一個選項（`runner_impl.cpp:1554-1573`）。照宣告不打補丁，加一個釘住現狀的測試。
  - `gyo_product_units` 不收 `library_only` 的 Plugin，所以測試自己連 `GYO::Ink`（宣告 12.3 U6）。
- 突變第一輪 6/8 killed：
  - `ink-lookahead-safe` 存活：台本裡沒有「台詞後面緊接指令」的地方。fixture 在「Here you go.」之後加 `~ play_sound(3)`，就殺掉了。
  - magic 的檢查存活：格式版本與 inkcpp 的 header 已經擋掉同樣的位元組，所以 magic 是多的。拿掉 magic，存檔開頭只留格式版本＋台本 hash。

## I2 狀態與場景

- 種子：`Start` 建 runner 後立刻 `set_rng_seed`。`Restore` 不設種子，用存檔裡的亂數狀態。
- 存讀：存檔開頭 8 bytes，後面是 inkcpp 的 snapshot。太短的位元組先複製進 256 bytes、其餘填 0 的緩衝區，因為 inkcpp 先讀 header 才檢查長度。
- 版本換過的存檔第一輪 survived：測試是在選項處存的，D2 讓正確的版本也讀不回來，所以拿不拿掉版本檢查都失敗。改成選完之後存，就殺掉了。
- **D2**（最重的）：在選項處存的檔，只要台本有 sequence，連讀回同一個台本都失敗（`globals_impl.cpp:294-297`）。用小台本逐一試：
  - 沒事：plain、`RANDOM`、一次性選項、tunnel、EXTERNAL。
  - 失敗：四種 sequence 都會，sequence 放在沒走到的 knot 也一樣。
- **D3**：第一次 `Advance` 前存的檔，讀回來已經結束。
- 一度以為有 D4（tunnel 裡選完存的檔讀回會跳過 tunnel），是我的探測看錯了輸出。完整比對後，讀回同一個台本是逐字接上的，改成正面的測試。
- 場景（U1 的 C）：Product＋一個 World，沒有視窗。`Conversation` Resource 放 `Story` 與 `Dialogue`（move-only 也能當 Resource）。作者的 System 每幀 `Advance` 一次：`give_item` 經 `MessageWriter` 交給另一個 System 數，`play_sound` 經 `Commands::Trigger` 交給 observer。
- thread：兩個各自讀進來的 Story 在兩條 thread 同時跑 50 輪，結果和單條 thread 相同；同一個 Story 的對話每一步換一條新 thread（`join` 隔開），結果和單條 thread 相同。
- owner 頁 `ink.zh-Hant.md`：INK-1～INK-6、RC-INK1～4、第 8 節的缺陷與能存的時間點。

## 守衛

- 第一次跑時兩個守衛失敗，都已修正：
  - `os.boundary_guard` R4：測試用了 `std::ifstream`。改用 `OS::Files::FileReader`。
  - `naming.guard` N1：測試的 namespace 改放在 `GYO::Plugins::Ink` 底下（N17）。
- `cmake.PluginSelection` 本機失敗：我在本機設了 `FETCHCONTENT_FULLY_DISCONNECTED=ON` 避免下載，它的子 configure 也繼承了這個設定，結果找不到 `_deps/yyjson-src`。和 `ink` 無關，在 CI 看。

## 驗證

- 本機（Intel Mac，`core` preset＋`GYO_PLUGINS=ink`，RelWithDebInfo）：`ink.dialogue` 23 案通過；`orchestration.guard`、`os.boundary_guard`、`naming.guard`、`kernel.guard`、`logging.guard`、`trace.guard`、`json`／`text` 的 `adapter_guard`、`entity_store.word_guard`、`cmake.UnitRegistry` 等 `cmake.*`、`build.ci` 通過。
  - `authority.authority_sources`、`authority.sources_scan` 是 Not Run：這個建置目錄只建了 `gyo_ink_tests`，沒有建它們的程式。
- ASan＋UBSan（Debug）：23 案通過，沒有記憶體錯誤。UBSan 有 6 種報告，全部在 inkcpp 裡，adapter 一件也沒有（owner 頁「範囲の限界」）。
- 突變：`ink_mutations.json` 13 個，最後的原始碼上全部重跑 13/13 killed。
- 沒跑：Windows、Linux（交給 L1）；inklecate 的 linux、windows 版。

## I3 審查與收口

- 審查（只讀 `origin/gyo2...11ef39c4`）：
  - 設計審查：general-purpose，Opus／high，給宣告、owner 頁、批次表的審查項目與 git 範圍。
  - 純淨 Code Review：pure-code-reviewer，Fable／high，只給範圍。
  - 重點與處置記在主 checkout 的 `build/evidence/v8-arch-gyo2-ink-spike/reviews-11ef39c4.md`。
- **B1（兩份都指出）**：讀進新版台本時，inkcpp 可能回空的 runner。
  1. 重現：第四版台本 `merchant_v4.ink`（拿掉存檔所在的選項）在 0 個指令時讓 `Restore` 回成功；第三版 `merchant_v3.ink`（hub 改名）則是 inkcpp 丟例外，回的是 `SnapshotUnreadable`。
  2. 寫成失敗的測試 `[ink-place-gone]`。
  3. 修正：加空 runner 的檢查；台本換過時，讀不了一律 `SnapshotNotMigratable`。
- 其他修正：
  - D1 只釘在 `once.ink`：主台本的「買劍」改成 `+`，golden 重新釘。
  - 補零改成長度檢查，加 16 bytes 的截斷台本案。
  - hash 只在 `LoadStory` 時算一次。
  - D2 的測試只比 code。
  - `ScriptFault` 的「之後的狀態」照進入點寫。
  - `Render.hpp` 補 include。
- 交使用者：宣告 12.5 的 U7（`Save` 要不要給「能不能讀回／遷移」的資訊、seed 的位置、tag 的拆法、不支援的參數）。偏離與觸發條件寫進 owner 頁的 RC-INK2、RC-INK3、RC-INK5 與「範囲の限界」。
- 驗證（最後的原始碼）：
  - `ink.dialogue` 24 案通過；`orchestration.guard`、`os.boundary_guard`、`naming.guard` 通過。
  - ASan＋UBSan 24 案通過，沒有記憶體錯誤，UBSan 和之前同樣 6 處，都在 inkcpp 裡。
  - 突變 15/15 killed。

## 交使用者決定的事

宣告 12.3：U5（D2：不處理、打補丁、或先回報 upstream；卡片建議先回報、等遊戲要在選項處存檔時再決定）、U6（產品怎麼選 `library_only` 的 Plugin；卡片建議等第一個用 ink 的遊戲開卡時再定）；12.5：U7。
