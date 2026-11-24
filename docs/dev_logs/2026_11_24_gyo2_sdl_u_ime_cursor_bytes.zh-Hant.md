# 2026-11-24 輸入法組字游標改成位元組（`v8-arch-gyo2-sdl-u` 的後續小 PR）

Owner：架構。子批 U 的任務卡做。分支 `claude/gyo2-sdl-u-ime-cursor-bytes`（疊在 #322 上，開向 `gyo2`）。作者 Opus 5.5／high。
根據：主會話的判斷（HANDOFF `v8-arch-gyo2-sdl-u-three-judgments` 第 3 件，gyo2 0dadc1df）：U 的設計審查 S2 指出 `input` 的 `TextComposition` 照後端的單位給組字游標，換算落在每個消費者；改成位元組、由 `sdl_window` 的 adapter 換算，照 winit 給 Bevy 的 `Ime::Preedit`（`bevy_window/src/event.rs:276-284`）。要在 PvP 大廳接上之前合併。

## 做了什麼

1. **`input`**：`TextComposition` 與 `TextInput` 的 `cursor`／`selectionLength` 改名 `cursorByte`／`selectionBytes`，意思是組字字串的位元組（code point 的邊界上，-1＝沒給）。改名是為了讓照舊單位寫的程式編不過，不會安靜地沿用。
2. **`sdl_window` 的 adapter**：`EditingUnitOf`（`SDL_GetCurrentVideoDriver()` 是 `"windows"` 時 UTF-16，其他 code point）、`EditingByteOffset`（換成位元組、夾在字串內、-1 照舊）；`TEXT_EDITING` 的起點與長度都換。
3. **`ui`**：`UiTextInput::cursor` 改名 `cursorByte`（位元組）；`UiUtf16Offset` 拿掉。
4. **文件**：input 頁、sdl 頁 SDL-7 的對應表、ui 頁第 3、4.4、7.1、8、10 節。
5. **測試與突變**：`sdl_window` 的 adapter 測試加兩案（換算的單位、經 SDL 事件的整條路）；input、ui 的測試改成位元組；突變 `window_input_mutations.json` 加 4 個 `sw-ime-*`，`ui_mutations.json` 拿掉 `ui-utf16-no-surrogates`、改 `ui-composition-caret-end` 的 find。

## 查到的事實（更正）

W 的 input 頁寫「SDL 在 Windows、macOS 是 UTF-16 的單位」。這個 SDL（3.4 系，本機 `build/local-cache/src/sdl3-src`）的文件寫「UTF-8 字元」（`SDL_events.h:378-380`）；Cocoa 先換成 code point（`SDL_cocoakeyboard.m:145-147`，用 UTF-32 的長度），Wayland 也是（`SDL_waylandevents.c:3076-3077`），X11、IBus、Fcitx 是字元的索引；只有 Windows 直接給 IMM 的 WCHAR 位置（`SDL_windowskeyboard.c:811`、`:924-928`）。所以 adapter 只對 Windows 照 UTF-16 數。

## 驗收

- 本機（macOS arm64）：`ui.*`、`input.module`、`window.module`、`sdl_window.*` 全過；完整 `ctest -L "cpu|gpu"` 與 L1 的結果見 PR。
- 突變：新的 4 個與相關的 3 個（`ui-composition-caret-end`、`ui-key-while-composing`、`ui-text-before-key`）全部 killed。
- Windows 的 UTF-16 那一支：CI 的 Windows 只跑 cpu／shader，沒有真的輸入法；`EditingUnitOf("windows")` 與換算由單元測試驗，實機的組字由 PvP 卡在 Windows 上看。
