# 模型檢視工具的試用（`v8-tool-model-viewer-trial`，2026-11-04）

宣告見 [model-viewer.md](model-viewer.md) 的「`-trial`」。L2 的兩項（P1、P2）已經量完；L3 由你在 Mac 上操作。

## L2 的結果（你的 Mac，`build/target/_build/ui`，commit 見證據的 `commit.txt`）

件數の突き合わせ：一致（每個模型 600 筆，P2 的事件檔 1 個）。

- **P1：通過。** 門檻是每個模型的 p95 ≤ 16.7 ms，試 1 次，沒有重試。

  | 模型 | 片段（最長的） | 筆數 | p50 | p95 | p99 | max（ms） |
  |---|---|---|---|---|---|---|
  | `object_fps_pvp.model.animation.ual1_mannequin` | `Armature\|Fixing_Kneeling` | 600 | 4.952 | 5.957 | 7.261 | 33.885 |
  | `object_fps_pvp.model.character.player_v20` | 沒有片段（預設姿勢） | 600 | 4.473 | 4.726 | 4.888 | 5.753 |
  | `object_fps_pvp.model.nailer_v2` | `Armature\|Reload` | 600 | 4.872 | 5.214 | 5.794 | 6.125 |
  | `object_fps_pvp.model.weapon.nailer_v2.world` | 沒有片段（預設姿勢） | 600 | 3.889 | 4.150 | 4.345 | 18.953 |

  - 量的範圍是每一幀從取樣到上傳的全部工作：取樣、`ModelRenderer` 更新、`Renderer::Render`、讀回（含等 GPU）、上傳成 SDL 渲染器的貼圖。不含 ImGui。用的是 `gyo_model_viewer measure-preview`，大小 1280×720，Metal。
  - 和宣告不同的地方：
    - 建置是 RelWithDebInfo（`-O2`），不是宣告寫的 Release。最佳化不會比 Release 多，所以這個結果只會偏慢、不會偏快；門檻也沒有因此放寬。
    - 有兩個模型沒有片段，「最長的片段」只能用預設姿勢代替。
- **P2：通過。** PvP catalog 裡的 `gyo.anim_events` 只有 1 個（`object_fps_pvp.anim_events.nailer_v2.viewmodel`）。`roundtrip` 的結果是讀回相同、逐位元組相同。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/trial/`，有 `p1-frames.csv`（2400 筆）、`p1-summary.txt`、`p2-roundtrip.txt`、`commit.txt`。

## L3：你操作的步驟

### 事前要知道的限制

- **第 5 項的「遊戲裡聽到」做不到。** PvP 現在用自己的 `SoundCues` 播換彈聲，不讀 `gyo.event_sounds`。本工具配的聲音只能在工具的試聽裡聽到。要讓遊戲讀 event_sounds，是 PvP 那一側的工作，記成之後的候選。
- **聲音必須是 48 kHz。** 混音器只收 48 kHz；其他取樣率的聲音可以配上、也可以存檔，但試聽不會出聲，Sounds 面板會標示「not heard」。
- **遊戲讀的是建置輸出裡的那份資產。** 在工具裡改的是原始資產（`assets/object_fps_pvp`），要重新建置，遊戲才看得到。

### 開工具

在這個工作樹的根目錄執行：

```bash
build/target/_tools/model_viewer/bin/gyo_model_viewer window --asset-root assets/object_fps_pvp
```

### 1～4：模型、動作、事件

1. 左邊的 Models 選 `object_fps_pvp.model.nailer_v2`，「Engine's view」的片段裡有 `Armature|Reload`。
2. Clip 選 `Armature|Reload`，按 Play：網格與骨架（黃色細條）跟著動；Speed、Seconds、轉鏡頭都正常；看得出彈匣入槽的時刻。
3. Events 按「Open object_fps_pvp.anim_events.nailer_v2.viewmodel」。把 `magazine_in` 的標記拖到別的時刻，按 Save（或 Ctrl+S），關掉工具再開，時刻是存的值。
   - 要在遊戲裡聽的話，先重新建置，再照你平常的方式開 PvP：

     ```bash
     $HOME/Code/Runtime/Builder/cmake-4.4.3/CMake.app/Contents/bin/cmake --build build/target/_build/ui
     ```

   - 之後要改回或保留，由你決定。
4. 試新增（Add at playhead）、改名（Name）、刪除（Delete）、Undo／Redo（按鈕，或 Ctrl+Z、Ctrl+Shift+Z）。在 Seconds 欄位輸入超過片段長度的值（例如 99）再存檔，會看到 Engine 拒絕的訊息，檔案不會被寫入。

### 5：用 audio_editor 做聲音並配到事件上

audio_editor 在它自己那條線的工作樹裡已經建好（[PR #141](https://github.com/yojinn-io/GYO-Engine/pull/141)，未合併，建置的時間晚於它最後一次程式的變更）：

```bash
/Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/trusting-joliot-f13b78/build/target/_tools/audio_editor/bin/gyo_audio_editor edit
```

1. 在 audio_editor 裡剪好一個聲音，輸出到 PvP 的內容：asset root 是 `assets/object_fps_pvp`，catalog 是 `assets/object_fps_pvp/asset_catalog.json`。不用視窗也可以，在這個工作樹的根目錄執行（`<source.wav>` 換成你的檔；id 與 path 只是例子）：

   ```bash
   /Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/trusting-joliot-f13b78/build/target/_tools/audio_editor/bin/gyo_audio_editor convert <source.wav> --asset-root assets/object_fps_pvp --asset-catalog assets/object_fps_pvp/asset_catalog.json --id object_fps_pvp.audio.trial_magazine --path audio/trial_magazine.wav
   ```
2. 在本工具按「Open content folder...」重新掛載 `assets/object_fps_pvp`（新的聲音才會出現在清單裡），再開 `nailer_v2` 與它的事件檔。
3. 在時間軸上選一個事件，Sounds 裡按「New sounds file」：填 New sounds id（例如 `object_fps_pvp.event_sounds.nailer_v2.viewmodel`）和 path（例如 `weapons/nailer_v2/viewmodel/viewmodel.event_sounds.json`），Sound 選你做的聲音，調 Sync 與 Gain，按「Save sounds as new file」。
   - 事件檔有未存檔的修改時，要先存事件檔。
4. 按 Play，在事件時刻聽到聲音。改 Sync，聽到的時刻跟著變。

### 6：關閉前的詢問

改一個事件不存檔，然後關閉視窗或按 Cmd+Q：會出現「Discard unsaved events?」。按「Keep editing」視窗不會關；按「Discard」才關。

試完後告訴我每一項的結果（通過，或看到什麼）；我會記進這份文件與 dev_log。
