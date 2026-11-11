# HANDOFF：determinism（`v8-arch-determinism-levels`）

## 現況（2026-11-11）

- 分支 `claude/arch-determinism-levels`（切自 origin/master `a75152d8`）。docs-only。
- 契約草稿 [determinism.md](../../determinism.md) 完成；對抗檢查（Opus／high，只讀子代理）6 blocker／8 should／7 nit 全部核對並處置，回應在契約 §9。
- 新發現（對抗檢查 F2）：11/9 場次有輸入與狀態相同而 `sweep_fraction` 差 1～4 ULP 的步驟（windows-x64 對 macos-x64），原因未明 → DET-0、E0。
- **停下等使用者決定**：DET-0（先做 E0 偵測）、DET-1（中間地帶 (b)／(c)）、DET-2（第三方一律邊界外；ufbx 今天在內）、DET-3（E0～E5 的順序）。
- 完成後通知：架構師會話（例行架構掃描）、第三方評估卡會話（D1、D2 在 D0 定案後重看）。

## 決定表

| 日期 | 決定 | 誰 |
|---|---|---|
| 2026-11-10 | 開卡：弄清決定性要哪一級、各模組付了什麼，結論返回 | 使用者（經架構師） |
| （待） | DET-0、DET-1、DET-2、DET-3 | 使用者 |

## 代號對照

| 代號 | 一句話 |
|---|---|
| `v8-arch-determinism-levels` | 本卡：契約草稿與計畫 |
| E0 `v8-engine-sweep-ulp-detect` | 重現並定位 sweep_fraction 的跨平台 ULP 差（只偵測） |
| E2 `v8-engine-determinism-guards` | 旗標順序、configure 檢查、intrinsic 掃描、arm64 金絲雀 |
| E5 `v8-product-anim-digest-golden` | 產品動畫內容 digest 的字面 golden |
| E1 `v8-engine-math-trig` | Math 自己的 Sin／Cos／Acos／Atan2，全部呼叫點，5 個情境改 golden |
| E3 `v8-product-digest-pose` | 合成 clip 的命中進 digest（只在 DET-1＝(c)） |
| E4 `v8-arch-kernel-k5-wording` | DI-K5 改寫、model 的等級一行 |
