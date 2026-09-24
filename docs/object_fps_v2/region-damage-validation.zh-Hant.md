# 2026-09-23：骨骼部位傷害資料化驗證

男女敵人的 11 個受擊區均由 `damage_multiplier` 配置：頭部 2.0、軀幹／骨盆 1.0、
四肢 0.75。結算使用槍口射線最後命中的部位，先乘倍率再扣敵人防禦，最低傷害 1。
完整契約與 Architecture Delta 見[敵人文件](enemies.zh-Hant.md#部位傷害)。

## 實際執行結果

環境為 Windows x64、MSVC、Ninja、RelWithDebInfo，沿用隔離 registry 的雙產品
build tree `build/target/_build/enemy3d`。證據保存在忽略的
`build/target/validation/region-damage/`。

| 驗證 | 結果 | 證據 |
| --- | --- | --- |
| 雙產品增量建置及全部 CPU 測試 | 29/29 通過，包含 Engine、v1、v2、組裝及產品契約 | `regression-final.log` |
| v2 CPU 測試內容 | 42 個案例、225840 個斷言通過 | build tree 的 `Testing/Temporary/LastTest.log` |
| v2 D3D12／DXIL | 27 張姿勢／碰撞／握槍圖及整波清場通過 | `d3d12.log`、`d3d12/` |
| v2 Vulkan／SPIR-V | 同上 | `vulkan.log`、`vulkan/` |
| 畫面回歸 | 與 `enemy-hair` 基線比較，兩個後端各 28/28 張 BMP 的 SHA-256 相同 | `image-comparison.json` |

每個後端的第 28 張圖為整波結束畫面。兩個後端均在 378 ticks／6.3 秒模擬時間
清完 4/4 配額，10 發、玩家生命 100、剩餘敵人實例 0。程序從 repository 外執行，
使用已組裝的 executable-relative 資產。畫面比對涵蓋頭髮、骨架動畫及碰撞開關；
暫停 F3 的輸入與姿勢不變性由既有 Runtime CPU 測試驗證。

第一輪平行編譯數 6 遇到 MSVC C1060 記憶體不足，降至 2 後完成建置。
測試診斷輸出所需的 `<ostream>` 標頭已補齊；最終無編譯錯誤，仍有既有
doctest 第三方標頭的 C5285 警告。

## 新增覆蓋

- 正式男女資料的全部部位與倍率；舊資料省略欄位使用 1.0。隔離記憶體資產來源
  驗證零、負值、非數字、超出 float 範圍與下溢，以及重複 ID 的拒絕與診斷。
- 傷害 25、防禦 5 的頭部 45／身體 20／四肢 13.75；直接傷害、最低傷害、
  過量傷害、極大有限數值，以及中間乘法不能因 float 溢位而誤殺的案例。
- 未知部位、無效傷害與死亡後命中不改變生命、姿勢、閃光或待處理事件；
  致死部位傷害取消近戰／遠程攻擊。
- 正常 GameSession 輸入流程驗證爆頭 45、準星指頭部但槍口命中重疊軀幹／骨盆時
  僅扣 20，以及準星看見頭部但槍口被側牆遮住時不扣血。測試使用專屬房間與
  武器掛點放大視差，不需要正式遊戲的測試接口。

本次只擴充 v2 資料及 domain 介面，未修改 Engine 或 v1 實作。
未重跑獨立 Engine-only／v1-only／v2-only build tree 或 v1 GPU 驗收；
上述 v1 結果指此次雙產品建置內的 CPU 回歸。
