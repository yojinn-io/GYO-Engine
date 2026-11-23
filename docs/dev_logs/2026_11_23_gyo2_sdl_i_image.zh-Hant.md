# 2026-11-23 GYO2：SDL 線子批 I（image Module＋stb_image adapter）

批次代號：`v8-arch-gyo2-sdl-i`。分支 `claude/gyo2-sdl-i-image-c86adf`（從 `gyo2` dd09651a）。任務卡 `gyo2-sdl-i-image`，主會話「[GYO2][不良少年]新架構重構」開的卡（使用者「開 P、I」）。根據：[SDL 總宣告](../architecture/target/v2/sdl-declaration.zh-Hant.md)修訂 21 第 10 節的 I（使用者 2026-11-23「SDL 照建議核准」，`v8-arch-gyo2-sdl-declaration-approved`）、[asset 宣告](../architecture/target/v2/asset-declaration.zh-Hant.md) 5.4、5.11、第 13 節；`v8-arch-gyo2-sdl-q2-ufbx-stb-module`。新 AGENTS（#296）下不另寫事前宣告，卡片的取捨寫在頁的第 5 節。頁：[image.zh-Hant.md](../architecture/target/v2/image.zh-Hant.md)。

## 下載的同意

stb_image 的版本、來源、hash 經主會話交使用者，使用者 2026-11-23 原話「兩個都同意」。v2.34，commit `1da9ade1e66195252114f5da497f244704a6c58d`，只抓 `stb_image.h`（278,442 bytes）。下載後 `git hash-object` 等於 GitHub 的 blob `9e65b834cfd7e34d9f8f8bd81d1c5e6eae128fc7`；SHA-256 `dfc2aadda6295f67f677587773f25e5bf8c8ad427ed0db7c518525c91e72e020` 寫進 `URL_HASH`。授權照檔尾原文：MIT 或 Public Domain（Unlicense）二選一。

## 做了什麼

- `engine/modules/image`（新）：`ImageData`（RGBA8、straight alpha、第 0 列在最上面、不翻轉、`RowPitch()`＝寬×4、`ColorSpace` 只是標籤；`AssetTypeName` 是 `image`；長度不對是 Programmer Error）、`PngLoader`（`image`／`png` 第 1 版，規則 NotPng → BadHeader → TooLarge → Undecodable，settings 只有 `colorSpace`）、`src/adapters/stb_image/StbPngDecode`（我們的話：`DecodePngToRgba8`；`unique_ptr` 配 `stbi_image_free`，複製一次進 `vector`）。
- `third_party/stb_image/`（新）：FetchContent 的 URL＋`URL_HASH`、`DOWNLOAD_NO_EXTRACT`；`gyo_stb_image` 只含 `stb_image_implementation.c`；`STBI_*` 設定放 PUBLIC 定義（asset 宣告 5.11 的五個＋`STBI_MAX_DIMENSIONS=16384`）。
- `config/modules.csv`：`image` 列（`library_only`、`depends=asset`、`private_depends=json`），放在 audio 之後。`third_party/CMakeLists.txt` 加 `stb_image`；`tests/common/CMakeLists.txt` 加 `image`。
- 下載後先核對 stb 的實際行為與 Data Contract 寫的一致：16 位元取高位元組（`stbi__convert_16_to_8`，`stb_image.h:1265-1266`）、灰階 1／2／4 位元的倍率表（`:4751`）、0 寬或 0 高是 Corrupt（`:5201`）、CRC 不查（`:4679`、`:5322`）。

## 寫的時候自己改掉的

- loader 原本在 `DecodePngToRgba8` 之後多比了一次「解出的寬高等於 IHDR 的」：stb 回的就是 IHDR 的寬高，這個分支到不了，拿掉（不是守衛，是多餘的結構）；長度不一致仍由 `ImageData` 的斷言接住。
- 「沒有別的 target 外露 stb」原本只寫「靠審查」：照 json 的 J-1、J-6 在 `gyo_expect_image_links` 加「`gyo_stb_image` 不在 INTERFACE 連結裡」，突變 `ib-stb-public` 守它。
- 測試的 `Expect` 原本比 enum，失敗輸出只有 `loaded.error().code == code`，突變執行器的 expect 對不上（第一輪 8 條 `wrong_failure`）：改成比碼名字串，失敗時印出「實際的碼 == 預期的碼」。

## Object_Connect

`~/Code/GitHome/Object_Connect`（ca4482c）的 `NoviceResources` 有 11 張 PNG：mouse 三張 32×32、node 七張 48×48，都是 8 位元 RGBA（色彩型別 6）、不交錯、帶 `sRGB`／`gAMA`／`pHYs`；`white1x1.png` 是 8 位元 RGB（色彩型別 2）。`axis/axis.jpg` 是 KamataEngine 附的，遊戲的原始碼不讀它。

本機用 `PngLoader` 載了 11 張（scratch 程式，不進 repo），全部成功；另用一支獨立的 Python 解碼器（zlib＋反濾波，只支援 8 位元、不交錯的 RGB／RGBA）算像素總和，11 張都和 `PngLoader` 的一致（例：heart.png 48×48，總和 596314）。圖沒有寫授權，不放進 repo；測試用同一種格式（含那三個 chunk）在測試裡逐位元組產生。

## 檢證

本機 macOS Intel，preset `test`。

- `gyo_image_tests`：16 案、700 個 assertion 全過。
- 全量 `ctest -L "cpu|shader" -j6`：198/198 通過（建置在本批的兩處小改之前；之後 `gyo_image_tests` 重建重跑通過）。
- 突變 `tests/common/image/mutations.json`（19 條）：第一輪 11/19 killed，其餘 8 條是 `wrong_failure`（測試失敗了、expect 沒對上，原因見上）；改 `Expect` 與 `ib-extra-link` 的 expect（CMake 的訊息會換行，改成跨行比對）後 8/8 killed。完整重跑 19/19 killed。之後 `ctest -R "gyo_image|naming|cmake"` 13/13 通過。
- 突變的數與理由（tests/common/mutation/README.md 第 8 條）：本批新的 DI 4 條，19 條 ≤ 4×5。每條守一個獨立會錯的決定（每條規則的每個條件、上限的等號與常數、stb 的上限、settings 的四種、色空間傳到圖、不翻轉、長度斷言、連結的兩件）。沒加的與理由寫在清單的 `description`（讀過頭的未定義行為三種、規則 3／4 的先後是結構、stb 內部的轉換不突變第三方檔）。
- merge `origin/gyo2`（c916f31a，X1 #298 合併後；`tests/common/CMakeLists.txt` 的衝突是兩邊各加一行，保留兩邊）之後重建，全量 `ctest -L "cpu|shader" -j6`：198/200；`gyo_os_tests`、`os.wake_fallback` 逾時（60 s）。單獨重跑也逾時；不設 CTest 的時限直接跑 `gyo_os_tests`：80/80 通過，其中「OS wake signal loses no signal sent while the waiter checks its flag」花了 56.4 s。當時同一台 Mac 上別的會話在編譯（load average 32）。這一案在這台 Mac 上拖延是已知的（[2026_11_23_gyo2_os_wake_test_backoff](2026_11_23_gyo2_os_wake_test_backoff.zh-Hant.md)：CI 與 Linux 上沒有，使用者決定不改）；本批不碰 os。判讀交 L1。
- L1 三平台：（PR 上手動 dispatch 後填）。

## 給主會話（記 HANDOFF 用）

本批沒有使用者的新決定；卡片照 L17 判的六件在頁的第 5 節（I-1～I-6），合併前核准時交使用者。和 T（#297）都改 `config/modules.csv`、`third_party/CMakeLists.txt`，後合併的一方 merge gyo2 跟上。
