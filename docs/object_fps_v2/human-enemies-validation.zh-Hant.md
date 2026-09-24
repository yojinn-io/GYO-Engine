# 2026-09-23：人物與手槍敵人驗證

本次以 Superhero Male 取代近戰 mannequin，以 Superhero Female 取代遠程 mannequin。
近戰保留拳擊；遠程改為右手持 Ultimate Pistol 1，以 Pistol_Shoot 的起始姿勢發射。
設計、資料契約與 Architecture Delta 見[敵人文件](enemies.zh-Hant.md)。

## 校正依據

使用與正式遊戲相同的 ufbx 座標轉換檢查原始資產，並以正式內容載入器
組裝後的模型再次執行 `--inspect-rig`。兩個人物與 UAL1 的骨架階層相同，
但 reference 旋轉、骨長及蒙皮綁定不同；測試確認轉移保留目標 reference pose、
幾何與綁定，沒有改寫來源快照。

手槍匯入後槍管朝 +X；槍口與握把位置由模型幾何取樣，掛到右手後以
正面與側面畫面確認方向及握持位置。Pistol_Shoot 的 0 至 1/30 秒已開始後座，
因此 release 與攻擊過渡均為 0 秒。驗收另外擷取 1/30 秒姿勢，避免只檢查
發射與動作結尾而漏掉後座過程。

人物分別以 reference geometry 高度校正至 1.6 公尺，Head 受擊球使用各自的
骨骼局部偏移。F3 顯示的受擊區與模型共用轉移後姿勢；死亡停用的區域消失。

## 本機結果

環境為 Windows x64、MSVC、Ninja、RelWithDebInfo。使用既有隔離 registry
及雙產品 build tree `build/target/_build/enemy3d`，沒有更動預設產品選擇。
以下證據位於忽略的 `build/target/validation/human-enemy-audit/`。

| 驗證 | 結果 | 證據 |
| --- | --- | --- |
| 雙產品增量建置與全部 CPU 測試 | 29/29 通過，包含 Engine、v1、v2、組裝及產品契約 | `regression.log` |
| 最後的載入驗證／驗收鏡頭修改 | 重新建置；Model 與 v2 CPU 4/4 通過 | `final-build.log` |
| v2 D3D12／DXIL | 27 張姿勢／碰撞／握槍側面圖與整波清場通過 | `d3d12.log`、`d3d12/` |
| v2 Vulkan／SPIR-V | 同上 | `vulkan.log`、`vulkan/` |
| v1 D3D12／Vulkan | 每個後端的場景、viewmodel、reload、muzzle 共 8 次通過 | `v1-gpu/` 各 `probe.log` |
| v1 畫面回歸 | 與本次修改前保存的 v1 圖像比較：248/248 SHA-256 相同，無缺檔或多檔 | `v1-image-comparison.json` |

兩個 v2 後端都在 378 ticks／6.3 秒模擬時間清完 4/4 配額，剩餘敵人實例為 0，
10 發、玩家生命 100。GPU 程序從 repository 外執行，使用 executable-relative
已組裝資產，未依賴來源美術資料夾。

合成測試涵蓋不同 reference 座標與比例、不同節點索引、階層與縮放錯誤、
來源／輸出隔離，以及非單位武器 root、geometry、hand、mount、actor 變換下，
實際蒙皮槍口頂點與攻擊原點一致。v2 內容測試另確認男女材質、頭頂受擊、
單次攻擊、死亡取消、暫停與模型／武器資源回收。

畫面人工檢查涵蓋待機、跑動、拳擊、射擊起始與後座、倒地，以及碰撞開關。
Scene capture 不含最終 HUD；F3 與 HUD 由既有 UI／Runtime 測試驗證。
這次沒有重跑獨立 Engine-only／v1-only／v2-only build tree，亦未驗證 Metal。
本次沒有修改 v1 的 source、assets 或既有動畫綁定規則。

## 同日追加：敵人頭髮

男性使用 Hair_SimpleParted 側分短髮，女性使用 Hair_Buns 雙髮髻，沿用資產包的
頭髮貼圖。外觀資料透過 v2 的 `accessories` 配置掛到 Head，以角色的權威姿勢
更新蒙皮；身高校正、受擊區與攻擊判定維持原本的人物資料。此追加只擴充 v2
呈現資料契約，沿用既有 ModelRenderer，沒有新增 Engine 依賴或修改 v1。

以下證據位於忽略的 `build/target/validation/enemy-hair/`，與上述人物／手槍
驗證分開保存。

| 驗證 | 結果 | 證據 |
| --- | --- | --- |
| 最終增量建置與 Model／v2 CPU 測試 | 4/4 通過 | `final-build.log` |
| v2 D3D12／DXIL | 27 張姿勢／碰撞／握槍側面圖與整波清場通過 | `d3d12.log`、`d3d12/` |
| v2 Vulkan／SPIR-V | 同上 | `vulkan.log`、`vulkan/` |

新增測試確認實際頭髮頂點跟隨 Head 平移與旋轉、非單位來源綁定及掛點變換、
來源／角色／前一幀姿勢互不改寫，以及缺失掛點的拒絕行為。既有測試同時確認
人物仍為 1.6 公尺、受擊區不變，以及附屬網格在敵人回收和換關後釋放。

兩個後端均在 378 ticks 清完 4/4 配額，剩餘敵人實例為 0。人工檢視待機、
跑動、倒地與持槍側面，確認髮型貼合頭部並跟隨姿勢。此次追加未重跑上列
29 項完整 CPU 回歸及 v1 畫面比對；那些結果屬於先前人物／手槍修改的驗證。
