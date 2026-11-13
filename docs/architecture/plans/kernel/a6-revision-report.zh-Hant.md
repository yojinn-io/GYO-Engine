# 報告：`v8-arch-a6-revision`（spec A.6 修訂＋legacy 處置＋Kernel 的 DI 落 target）

件数の突き合わせ：**不一致（兩處，都是宣告的口徑與實際不同，下面說明）**——24 列落字 24／24：一致；36 條重標 17＋19＝36：一致；legacy 行號引用 事前 **14** 命中→0（宣告寫基準 13：legacy-impact 把 spec 同一行的兩個行號算一處，掃描器每個行號算一次，是同一組引用）：口徑不一致；目標 DI 清單 34＝kernel 頁 15＋各部件頁特有 19（宣告原寫「各檔 DI 節條數之和」，經使用者的決定改寫，見〈停止條件 3〉）：依修正後的判定一致；docs 以外的 diff 0：一致。

更新：2026-11-13。Owner：架構（架構師會話）。事前宣告：[a6-revision-declaration.zh-Hant.md](a6-revision-declaration.zh-Hant.md)（使用者核准；文末有〈停止條件 3 的處置〉）。本批只動 `docs/`。

## 先讀：停過一次，與三個要使用者定的事

**停止條件 3（目標 DI 重複）在開工後觸發，已由使用者決定後續做法。** 經過與決定記在宣告文末。結果：Kernel 的條文全文集中在 `target/kernel.zh-Hant.md`；部件頁保留條款標題與條號，內容連到 kernel 頁的單一條款。

還要使用者定的：

1. **目標 DI 清單的一次核准**（遷移計畫 §7 U13）：清單在 `docs/architecture/target/README.md`〈目標 Design Invariants 清單〉，兩層共 34 條。
2. **`neutral_names_guard` 要不要停止掃兩份契約文件**（Q10）：§14.4 的 1～5 在本報告〈Q10〉。
3. **36 條裡有 23 條沒有在所在的檔加標註**：宣告的逐檔清單與這 36 條實際所在的檔不對應。要不要由本批補標，見〈36 條重標〉末段。

另有一點請在決定採用頁保護時（U1）一起看：spec A.6 第三層第 6 點——同一種誤用（System 以外寫到欄）在第三層關著時是 Err，開著時是終結。

## 判定

| # | 判定（宣告） | 結果 |
|---|---|---|
| 1 | 對抗檢查 0 blocker | **第一次不成立**（blocker 1），修正後成立；見〈對抗檢查〉 |
| 2 | 24 列落字 | 成立（下表） |
| 3 | 36 條重標，n＋m＝36，沒有「兩邊都說不通」的條 | 成立：被取代 19＋相容（措辭更新）17 |
| 4 | legacy 行號引用歸零 | 成立：14 命中→0；掃描器以舊版 spec 為正例命中 13、自測 7 個正例與 5 個反例通過、stderr 0 位元組、結束碼區分 0／1／2 |
| 5 | 目標 DI 清單兩層、件數一致 | 依修正後的判定成立：34＝15＋19；整條是連結的 15 條不計 |
| 6 | 區判定四項 | 成立：spec L.3.1，每項有判區、理由、match-sim 的動作、與三份清單的差異 |
| 7 | 零程式變更 | 成立：變更檔全在 `docs/`；`plans/kernel/scripts/` 未動 |
| 8 | Q10 的 §14.4 1～5 | 寫在本報告，標待使用者 |

遷移計畫 §3.1 多列的兩項：target/world 沒有產品欄位名（`bodies_`、`PlayerState`、`combats_`、`animations_`、`connections_`、`actionLedgers_` 掃描 0 命中；正例在產品 target 檔命中）；target/determinism 沒有對應 legacy §7 的節、沒有搬入 §7 的條目。

## 24 列落在哪裡

| 計畫 §5 的列 | 落點 |
|---|---|
| 1、30 | spec A.6 第三層第 8 點（平台層屬 `engine/os`、Delta-4、`os.md` RC 9 的出口與採用時的標註文字；`os.md` 未改） |
| 6、7、8、9、12、14 | spec A.6 第三層第 1、2、3、4、6、7 點；target/limits LIM-3；target/system-contract §2b 的報告欄位（9） |
| 19、22 | spec G.4 第 5 條 |
| 20 | spec G.4 第 6 條（新） |
| 23 | spec G.4 第 3 條 |
| 27 | spec §0 用語「上下文鏈」「並列清單」、B.4 第 7 點；target/error-handling 新節；legacy error-handling 的標註 |
| 28 | target/runtime-composition §3b 新條；preview.md 檔頭的限縮註、DI 4 與 RC 4 的標註；model_viewer RC 1 的標註 |
| 29 | target/system-contract §2b「與 field-session DI 3 的關係」 |
| 32 | 本報告〈36 條重標〉＋檔內標註 |
| 33 | target/determinism §1、§2、§4（節號同 legacy）；legacy determinism 檔頭註；HANDOFF〈暫停〉一行；spec §0 用語的註 |
| 34 | spec 13 個、target/README 1 個行號引用改章節名 |
| 35 | 新檔 `docs/object_fps_pvp/target/entity-model.zh-Hant.md` §1；target/world §3 指過去 |
| 36 | target/world §6（新） |
| 39～42 | spec L.3.1（新） |

## 36 條重標

定義（HANDOFF `v8-arch-legacy-impact-dispositions`）：遷移完成後該條對任何程式都不再成立，才算**被取代**；仍約束同一份程式、只是措辭或機制換了，歸**相容（措辭更新）**。本批用的補充規則：owner 的程式整個被刪除的（framework），它的條算被取代，即使性質由別的 owner 的新條文承接。

| 檔 | 條 | 重標 | 理由／承接 | 檔內標註 |
|---|---|---|---|---|
| entity-store.md | DI 1 | 相容（措辭更新） | 「given, never made」仍約束 store；generation 位元在格式裡、store 不解讀 | 已標 |
| | DI 4 | 相容（措辭更新） | store 側的半條不變；延後佇列的權威在 kernel DI-KS 9 | 已標 |
| | RC 2 | 相容（措辭更新） | 「never inside the store」仍約束 store；處置從「Engine 的配發器」改成產品的 Resource（對抗 S-8 改判） | 已標 |
| | RC 1、5、6、8 | 被取代 | 格式已含 generation；查詢是 World 的契約；stamp 已定；Kernel 進 Engine、framework 退役 | 已標 |
| entity-model | E-3、E-7、E-12、RC-2 | 相容（措辭更新） | 目標措辭在產品 target 檔 §2 | 已標 |
| framework/architecture.md | DI 1～6、RC 1～8 | 被取代（14） | 程式隨 framework 刪除。DI 1、DI 3 的性質→plugins 層的 DI；DI 5→runtime-composition §4；DI 6→runtime-composition DI 2 | **未標**（檔頭已寫整節隨元件退役） |
| framework/preview.md | RC 4 | 被取代 | 三個方案都保留讀回 | 已標 |
| model_viewer | RC 1 | 相容（措辭更新） | 前半（viewer 自己畫）仍成立，後半（同步讀回）不再適用 | 已標 |
| model_viewer | DI 1、2、4 | 相容（措辭更新） | 規則留、「經 framework」改成經 plugin | **未標** |
| ui_toolchain（ui_editor） | DI 1、2、3 | 相容（措辭更新） | 同上 | **未標** |
| audio_editor | DI 1 | 相容（措辭更新） | 同上 | **未標** |
| voxel_map_editor | DI 1 | 相容（措辭更新） | 「不連遊戲、不含產品名」留；core／window 的切法由 tool-voxel 重導 | **未標** |
| platform-sdl.md | RC 2 | 相容（措辭更新） | 例外名單的 target 改名為 imgui plugin | **未標** |

合計：相容（措辭更新）17，被取代 19。檔內已標 13 條，未標 23 條。

**未標的 23 條**：宣告的逐檔清單只列了 entity-store、entity-model、preview 與「framework/model-viewer.md」（實際路徑是 `docs/model_viewer/architecture.md`），沒有列 framework/architecture.md、ui_toolchain.md、audio_editor、voxel_map_editor、platform-sdl 這五個檔，所以沒有動它們；model_viewer 只標了宣告點名的 RC 1。其中 9 條「相容（措辭更新）」的新措辭要等 plugin 的 target 名字定了才寫得出來（plugins-foundation 之後），照 target/README 的規則本來就由該部件的遷移批標。請使用者定：(a) 維持現狀，由各遷移批標（建議）；(b) 本批補一個修訂，把 23 條都加上只寫重標結果的標註。

另外不在 36 條裡、但宣告點名的：preview DI 4（衝突 2）標「superseded」；error-handling「no error chains」一句標「對 Kernel 收窄」；architecture.md〈Layers〉加一段註（三層表對新程式由 runtime-composition §4 取代；check 1 的落點是 target/naming DI 4）；determinism.md 檔頭註。network-architecture 沒有任何一條在 36 條裡，未動。

## 區判定（spec L.3.1）

- `ArenaMesh`：呈現，不是模擬。audit P13 把它與 `Arena` 併判是分組造成的。
- `App/` 三個內容定義檔：依內容分成判定用的定義（(c) 內容身分）、呈現專用、載入器的 JSON 輔助三份；拆型別的做法由 match-sim 與 assets-core 的宣告定。
- `Combat.hpp`：(c)，留在 Sim。**與計畫第 41 列「對齊 audit P1 分組」的措辭不同**：區一致（都是 C），初判不同（它是「留」，P1 的「重寫」指 `PvpMatch`）；以 L.3.1 為準。
- Match 的 build 清單裡的兩個 Present 檔、兩個 Present 標頭含入 `PvpMatch.hpp`、`Sim/PlayerPose` 裡只給 Client 用的函式：都是呈現；搬動由 match-sim 做。

## Q10：`neutral_names_guard` 停止掃兩份契約文件（§14.4 的 1～5；待使用者）

1. **守的性質**：使用這支守衛的 Engine 模組（voxel_mesh、map_markers），它的程式、測試與契約文件都不出現七個玩法名詞（spawn、boss、loot、extract、patrol、team、zone）。
2. **緩める面**：改後，`docs/architecture/voxel-mesh.md` 與 `map-markers.md` 裡出現這些詞不再讓測試失敗。例：在 voxel-mesh.md 的範例裡寫「loot zone」，以前失敗、改後通過。
3. **締める面**：無。
4. **性質由誰接**：程式與測試照舊被掃（`engine/voxel_mesh`、`engine/map_markers` 與兩個測試目錄），格式的欄位名、範例資料、錯誤訊息裡的玩法名詞照樣被擋。文件的部分從「機器擋」降為「審查擋」（target/naming DI 4）。
5. **根據與證據**：守衛用不分大小寫的子字串比對、沒有詞界（`tests/common/scripts/neutral_names_guard.py` 的 `WORDS` 與 `re.compile`）；目標架構的用語含 `zone`（決定性區）、`Extract`、`Despawn`，這兩份文件若要自述它的區就寫不進去（legacy-impact §4 第 3 列、複核 #9 的實測）。

**架構師的看法（與先前處置行的方向不同，請使用者定）**：本批實際做下來，這兩份文件沒有需要寫進這些詞的地方——VoxelMesh 的區已寫在 target/determinism §4.6。放寬換到的東西很少。建議**不放寬**，維持現狀；等真的有一條註記寫不進去時再提。若使用者仍要放寬，腳本的修改由 kernel-scaffold 做。

## 宣告之外、照遷移計畫 §3.1 做的

宣告的範圍表漏列、但計畫 §3.1 與 §5.2 指派給本批的：target/naming §4 的例子與 DI 1 的對應寫法（`RetroFPS/Sim` ↔ `fps::pvp::sim`）；target/naming 新增 DI 4「Engine 的名字中性」（計畫 §5.2 第 3 列的 DI 落點，承接 legacy〈Layers〉check 1，條文不變只換落點——它是一條新增的目標 DI，列在清單裡請使用者一併核）；target/runtime-composition §4「plugins 受 OS 邊界守衛掃描」。另：遷移計畫一處「target/determinism §2」因節號重排改成 §11；`third-party-evaluation/evaluation.zh-Hant.md` §2.1 落字 audit §4.4 的四處收窄與五個新列。

## 對抗檢查（Opus／high，只讀；作者 Fable）

第一次：**blocker 1／should 14／nit 10**。全文在主 checkout `build/evidence/kernel-a6-revision/adversary-report.md`。檢查者確認沒有問題的：kernel 頁與 spec K.1／K.2 逐字相同、legacy 七檔只加標註、determinism 搬入的三節與 legacy 相同、只有 `docs/` 有差異、行號掃描 0。

| # | 發現 | 處置 |
|---|---|---|
| B-1 | kernel DI-KS 8 的併入句與 determinism DI 2 寫成「Host 與 ingress 可寫 domain tick」，與 DI-KS 12、spec A.2 矛盾 | 修：boundary Resource 與 domain tick 分開寫；tick 只由 clock schedule 的 `Run` 第一步推進 |
| S-1 | A.6 第三層第 6 點與第二層對同一種誤用的性質不同；「常開」不成立 | 修文字（拿掉「常開」、寫明與第二層的關係與替代做法）；性質隨開關不同這件事留給使用者在 U1 定 |
| S-2 | 第三層沒定 Ready／Applying 的保護狀態 | 修：寫明只在 Running 的 System 呼叫內有效，其餘由 world-exec／pageguard-os 定 |
| S-3 | 第 3 點不採全開全關，卻指向照那個模型寫的 BU-8 | 修：註明 BU-8 那一格是上限，pageguard-os 重寫 |
| S-4 | G.4 第 3 條漏了 `RecordMovementDiag` | 修 |
| S-5 | G.4 第 5 條與 H.4.1 (b) 寫「Ingress 判」，與 U7 不一致 | 修：兩處加註依 U7 |
| S-6 | 產品 target 的欄位表三處事實錯（`combats_`、`connections_` 也有多個寫者等） | 修 |
| S-7 | 23 條沒有檔內標註 | 不補；列給使用者（上文） |
| S-8 | entity-store RC 2 是半條 | 採：改判相容（措辭更新），17／19 |
| S-9 | J-14 在 kernel 頁失去對應 | 修：併進 DI-KS 12 |
| S-10 | PV2、PV5 寫窄了 | 修：PV2～PV5 |
| S-11 | naming DI 4 不在宣告範圍表 | 不改內容；列在上一節與清單 |
| S-12 | 節號重排後一處引用指錯 | 修（遷移計畫一處、spec G 節一處） |
| S-13 | 基準 13 與實測 14 | 件数行寫成口徑不一致並說明 |
| S-14 | 本報告當時不存在 | 本檔 |
| N-1～N-10 | 空白、出處標記、指向少一跳、README 對照兩處、L.3.1 兩處半句、掃描器兩個形狀、error-handling 多寫的一句、條數的算法 | 全部修；掃描器補形狀後重掃仍為 0 |

修正後沒有再做第二輪對抗（修正都在檢查者給的最小修正範圍內）。

## 範囲の限界

- 只寫文字，沒有驗條文可實作（由 registry-schedule、world-exec 驗）。
- 「被取代」的標註在該部件的遷移批完成時才生效。
- A.6 第三層是條件化條文；採用與否待使用者在閘門 G2a 後定（U1）。
- determinism 的 §7、E0～E5、§4.2a 沒有搬（暫停項）。
- U14：spec H.2b 第 4 點在 v1 只在合成測試成立。
- L.3.1 的事實以 2026-11-13 的工作樹為準；程式行號不寫進條文。

## 證據

主 checkout（Intel Mac）`build/evidence/kernel-a6-revision/`（git 管理外）：`scan_legacy_line_refs.py`、`line-refs-before.txt`、`line-refs-after.txt`、`adversary-report.md`、各 `edit_*.py`（本批的修改腳本）、`a6-worktree.diff`（對抗檢查時的工作樹差異）。
