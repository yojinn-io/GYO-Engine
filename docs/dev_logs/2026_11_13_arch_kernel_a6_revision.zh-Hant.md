# 2026-11-13 `v8-arch-a6-revision`：spec v0.4、legacy 處置、Kernel 的 DI 落 target

Owner：架構（架構師會話，Fable／high；對抗檢查 Opus／high 只讀）。只動 `docs/`。宣告：`docs/architecture/plans/kernel/a6-revision-declaration.zh-Hant.md`；報告（判定、36 條重標表、Q10、對抗處置）：`docs/architecture/plans/kernel/a6-revision-report.zh-Hant.md`。

## 做了什麼

- **spec v0.4**：A.6 加存取驗證第三層（頁保護）的條件化條文八點；G.4 第 3、5 條補列、新第 6 條；新 L.3.1（-zones 回報四項的區判定與 `kz-*` 突變的預期去處）；§0 加「上下文鏈」「並列清單」；對 legacy 文件的行號引用改章節名；K.1、K.2 凍結為來源紀錄。
- **target**：新 `kernel.zh-Hant.md`（DI-KS 1～15、RC 1～13 全文，每條有錨點）；world、system-contract、scheduler、runtime-composition、determinism、error-handling 的 DI 節改成「保留標題與條號、內容連到 kernel 頁的單一條款」加各自特有的條；determinism 搬入 legacy 的 §1、§2、§4（節號相同，原 §1～§4 改 §10～§13）；world §6、runtime-composition §3b／§4、system-contract §2b、error-handling 新節、limits LIM-3、naming §4／DI 1／DI 4；README 加「先讀 kernel 頁」與兩層 DI 清單（34 條）。
- **產品 target**：新 `docs/object_fps_pvp/target/entity-model.zh-Hant.md`（欄位分類、四條 legacy 條文的目標措辭）。
- **legacy**：只加標註（entity-store 7 條、entity-model 4 條、preview、model_viewer RC 1、error-handling、determinism、architecture.md）。
- **其他**：`third-party-evaluation/evaluation.zh-Hant.md` §2.1 落字 audit §4.4；HANDOFF〈暫停〉加 determinism §7／E0～E5 一行。

## 經緯

1. 開工後、寫 kernel 頁之前，對照發現 DI-KS 15 條裡 11 條與部件頁的 DI 同性質——宣告的停止條件 3。停下列三個選項給使用者；不相依的部分先做、不提交。
2. 使用者選「全文集中 kernel 頁」，並追加三點：部件特有的規則不進 kernel 頁；部件頁要保留條款標題、引用細到單一條款；必讀的強調落在每條的指向句。照此做，判定 5 改寫（宣告文末〈停止條件 3 的處置〉）。部件頁同性質條裡 spec 沒逐字寫到的句子與守衛併進 kernel 頁，標〔併自〕。
3. 36 條重標時發現宣告的逐檔清單與 36 條實際所在的 9 個檔不對應（5 個檔沒列、一個路徑寫錯）。只標清單內的 13 條，其餘列給使用者。
4. 對抗檢查 blocker 1／should 14／nit 10：blocker 是我併句時把「Host 與 ingress 可寫 boundary」寫成也可寫 domain tick；另有產品欄位表三處事實錯、G.4 漏一個入口、兩處與 U7 不一致。全部處置（S-7、S-11 是列給使用者，不是修）。
5. Q10 的 1～5 寫進報告；實際做下來認為不需要放寬，建議與先前處置行相反，交使用者定。

## 檢驗

- 行號引用：`scan_legacy_line_refs.py` 事前 14 命中（宣告基準 13，口徑差一）→ 0；舊版 spec 當正例命中 13；自測通過；stderr 0。
- 36 條：17＋19＝36。24 列：24／24（報告的落點表）。DI 清單：34＝15＋19。
- `git status`：變更全在 `docs/`。變更檔內的相對連結逐一檢查存在（legacy determinism.md 裡一個原本就壞的連結未動）。
- 對抗檢查者另確認：kernel 頁與 spec K.1／K.2 逐字相同；legacy 七檔去掉標註後與原文相同；determinism 搬入的三節與 legacy 相同。

## 證據

主 checkout（Intel Mac）`build/evidence/kernel-a6-revision/`（git 管理外）：掃描腳本與前後輸出、`adversary-report.md`、修改腳本、對抗檢查時的工作樹差異。

用時約 6 h（估 6～12 h）。
