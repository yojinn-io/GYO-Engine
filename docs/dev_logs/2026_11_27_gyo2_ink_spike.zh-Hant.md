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

- U5 已決定（宣告 11.1，HANDOFF `v8-arch-gyo2-ink-spike-u5`）：「照建議，不過讓主會話開一張任務卡來專門處理回報的問題」。D2 由另一張回報卡回報 upstream；存檔時機（選完之後、下一次 `Advance` 之前，選項畫面不存）是遊戲的規則，adapter 不改。要不要在 `Save` 上露出，等 U7 (a)。
- U6 擱置（宣告 11.2，HANDOFF `v8-arch-gyo2-ink-spike-u6-unit-research`）：「先擱置這個問題，開一張研究卡。看下應該如何設計。」unit 的選擇、建置、連結與註冊交研究卡 `v8-arch-gyo2-unit-composition-research`；`ink` 照現在的 `GYO_PLUGINS` 路徑建，spike 不改。
- U7 已決定（宣告 11.3，HANDOFF `v8-arch-gyo2-ink-spike-u7`）：「我同意你的看法。」(a) 現在做（見 I4）；(b)(c)(d) 等第一個用 ink 的遊戲，(d) 定案前至少要報錯。U5～U7 都處理完。

## I4 U7 (a)：`Save` 說讀不讀得回來

- 介面：`Save` 回 `Saved{bytes, restorable, migratable}`（原本只回位元組）。位元組照給，不擋。
- `restorable` 的判法：`Save` 當場在同一個台本上照 `Restore` 的做法重建一次（抽出 `Rebuild`，`Restore` 與 `Save` 共用），失敗是 false；成功但 `can_continue` 和原本不同也是 false（D3）。
  - 為什麼不用規則猜：D2 取決於台本有沒有 sequence。`once.ink` 沒有 sequence，在選項處存讀得回來，所以「在選項處 → false」會錯。
- `migratable`：`restorable` 而且 inkcpp 的 `snapshot::can_be_migrated()`。
  - 我原本以為 tunnel 裡選完之後不能搬，inkcpp 說能，`Restore` 讀進第二版也成功，我就照這個寫了測試。但只核對了 `Restore` 有沒有成功。
  - 設計審查（general-purpose，Opus／high）指出 **D4**：搬過去之後 `Advance`，tunnel 回到別的地方，多出一句台詞和一個玩家沒選的指令。我用測試重現（「No.」之後是「Here you go, friend.」與 `play_sound(3)`），讀碼確認 inkcpp 的 `can_be_migrated` 不看 tunnel 的返回點、`migrate_to` 不清堆疊。
  - 這個 D4 和 I2 那次「一度以為有 D4」的誤報不同：那次是讀回**同一個**台本（確認沒事），這次是讀進**新版**台本。代號沿用 D4，因為誤報沒有佔用代號（owner 頁只記 D1～D3）。
  - 處置照 U5 的先例：不打補丁，記成 D4（owner 頁第 8 節），加一個「inkcpp defect, not patched」的測試釘住現狀；旗標照 inkcpp 給，註解與 owner 頁寫明 D4。adapter 自己判斷 tunnel 要讀 inkcpp 私有的堆疊，沒有做，交使用者（宣告 12.6 U8）。
- 原本也比選項的個數，突變 `ink-save-choices-uncompared` 存活：這一版 inkcpp 沒有會讓它不同的情形，拿掉。
- (d) 照程式核對：list 參數已經是 `ScriptFault`，沒有改。
- 測試：新增 3 案（6 個存檔點的旗標＋用 `Restore` 核對；讀不回的存檔仍給位元組；D4）。
- 審查的其他修正：`migratable` 的說明照 inkcpp 實際的條件改寫（原本寫成「在 tunnel 裡不行」）；`restorable` 的說法收斂成「試讀一次、比能不能往下跑」；`SnapshotNotMigratable` 的註解與一個測試名改成「在等選擇」（真正的原因）；owner 頁「參數個數不限」改成最多 10 個。`Rebuild` 用 `optional<string>` 表示失敗原因（審查建議改 `Result`），沒有改：它只在 adapter 內部用。
- 突變：新增 4 個；`Rebuild` 抽出後 4 個舊突變的 `find` 更新。跑了相關的 12 個（新 5＋`find` 更新的 4＋`find` 在 `Restore` 附近的 `ink-migrated-inverted`、`ink-migration-failure-unreadable`、`ink-save-version-unchecked`）：11 killed，`ink-save-choices-uncompared` 存活（上一條，連同突變拿掉）。拿掉之後受影響的 2 個重跑，2/2 killed。目錄共 19 個。
- 驗證：`ink.dialogue` 27 案通過；ASan＋UBSan 27 案通過，沒有記憶體錯誤，UBSan 同樣 6 處都在 inkcpp。
- 另：主會話發現一個跑了約 3 小時的 probe（PID 2113，查 D2 時留下）。原因是當時的測試在 `-s` 下對成功的 `Result` 呼叫 `.error()`，GYO 的 assert 觸發 abort，doctest 的 signal handler 再去鎖已持有的 mutex，於是死鎖；不是 inkcpp 的行為。已結束它；之後的探測都加逾時。

## I5 上游回報後的登記：D2b、D5 與 issue 的 URL

- 主會話轉達：上游回報卡已在 JBenda/inkcpp 發了五件（#178 D2b、#179 D1、#180 D3、#181 D4、#182 D5）。D2 由上游 PR #176 處理，還是 OPEN、未合併，d38608d 不含。證據在主 checkout 的 `build/evidence/gyo2-inkcpp-upstream/`。
- owner 頁第 8 節：D2b、D5 用正式編號登記，D1～D5 都附上游的 URL，D2 寫明 PR #176 OPEN 未合併；「能存的時間點」改寫成「沒有一個點對所有台本都對」，並寫明 U5 (3) 已交使用者重新決定。
- 測試（照 D1～D4 的做法，各一個「inkcpp defect, not patched」）：
  - D2b：新台本 `sequence.ink`（stopping＋cycle）。選完之後存，原本接著是「second」「two」，讀回是「first」「one」。
  - D5：新台本 `shuffle.ink`（照上游回報，台本自己 `SEED_RANDOM(21)`）。兩個元素拜訪兩次都是「y」。
- `restorable` 對 D2b：**true**（試讀成功、`can_continue` 相同，`Save` 看不出 sequence 回到開頭），D2b 的測試把這一點也釘住。要看出來得比對 inkcpp 不公開的拜訪次數，或每次存檔多跑一段台本，都沒有做，等存檔時機的重新決定一起看（owner 頁第 9 節）。
- 之前 `merchant.ink` 的存讀測試能逐字接上，是因為它只有 `{~…}` 的 shuffle：shuffle 每次重抽（D5），拜訪次數歸零看不出來。
- 驗證：`ink.dialogue` 29 案通過。突變沒有跑：這一批沒有改 adapter 的程式，只加了測試與註解。

## U8 的決定

- 宣告 11.4（HANDOFF `v8-arch-gyo2-ink-spike-u8`）：「先維持現狀吧，等需要用到的時候。」不 patch、不讀 inkcpp 內部的 stack，D4 由 I4 的測試固定。
- 附帶的事已做：`Saved::migratable` 的註解與 owner 頁第 9 節、INK-7 寫明「在 tunnel 中存檔時，`migratable` 不可信」，附 #181。只改了註解與文件，沒有跑突變，也沒有另外 dispatch L1（程式與測試和 `3cd56913` 相同）。
- U5 (3) 的存檔時機還在重議（#178）。
