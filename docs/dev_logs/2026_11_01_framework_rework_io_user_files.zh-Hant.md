# framework 整改：v8-engine-io-user-files（使用者檔案的缺口補進 engine/io）

日期：2026-11-01。分支 `claude/framework-rework`。檔位 high；路徑包含判定與原子寫一段 xhigh 唯讀審查（引擎繼承）。主會話引擎 Opus 5.5。估 6～8 h，實際約 4.5 h。

**狀態：** 完成（Mac 上的測試、突變、全量 CTest）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[契約、行為變更與條文](../architecture/plans/framework-rework/io-user-files-judgment-change.md)、[io.md](../architecture/io.md)「User files」與 Replacement Criteria 3、[PLAN](../architecture/plans/framework-rework/PLAN.md)。

## 做了什麼

- **AD2（使用者核准）**：engine/io 補上 framework 用到、Engine 還沒有的三項能力。讀檔、判斷存在、轉絕對路徑沿用現有 API，不新增。
  - `Helpers::ProbeFileStamp`（新標頭 `helpers/UserFiles.hpp`）：大小、修改時間、內容雜湊。不存在時成功並回 `exists == false`；不是一般檔案時看狀態就回 InvalidPath，不開檔；讀取上限由呼叫者給。
  - `Helpers::WriteFileAtomically` 的選項 `createParentDirectories`：先建立缺少的上層目錄；磁碟根目錄下的檔案不需要。
  - `FS::IsNativePathWithin`：判斷寫在某個原生路徑的檔案會不會落在根目錄底下。候選路徑的父目錄交 OS 解析（連結先於 `..`），最後一段照寫；根目錄整條解析；結尾分隔符代表它前面的目錄；名稱比較用 `Os::Files::SamePathComponent`。非 UTF-8 與 Win32 命名空間前綴回 InvalidPath。它不改路徑，也不是沙箱。
- **io.md**（使用者核准）：Path contract 加「User files」一項，Replacement Criteria 3 補一句；原子寫那一項補「除非呼叫者要求建立」。Design Invariants 不變。
- **測試**：`tests/common/core/io/UserFilesTests.cpp`（13 條，engine_tests 直接連結 `GYO::Os`，屬 AD2）；`tests/framework/UserFilesParityTests.cpp`（新舊對照）。
- **突變目錄**：`docs/architecture/plans/framework-rework/scripts/io_user_files_mutations.json`（15 條，1 條只在 Windows、1 條只在 macOS／Linux）。
- framework 的 `IsPathWithin`、`ProbeFileStamp` 不動（使用者決定），收尾批刪除；ui_editor 由工具線 T2 改用 engine/io。

## 驗證與經緯

件數の突き合わせ：一致（宣告 15・執行 14・killed 14、Windows 限定 1 條在 macOS 記為 skipped、stale 0）。

- **xhigh 審查推翻了「照舊版移植」**。原本的計畫是把 framework 的判定原樣搬進 Engine，審查指出這會把舊缺陷寫進 Engine 的契約：
  - F1（我新引入的回歸）：開建父目錄選項時，磁碟根目錄下的檔案寫不進去。已修。
  - F2：根目錄以分隔符結尾時，底下什麼都判成不在裡面。ui_editor 只給 `--asset-catalog` 時根目錄是 `.`，「不得匯出到資產根目錄」的守衛因此失效。
  - F3：根目錄是連結，或大小寫和磁碟不同時，同樣全部判成不在裡面。
  - F4：POSIX 上先詞法消去 `..` 再解析連結，和 OS 實際開檔的位置不一致。
  - minor F5～F9：契約文字、解析失敗時退回、命名空間前綴、開檔的副作用、錯誤訊息差異，都已處理或寫進契約。
- **依 §14.3 先 Red 後 Green**。對錯的標準是「OS 實際把檔案放在哪裡」：測試用標準函式庫的 `weakly_canonical` 解析父目錄、再接最後的名稱，當獨立對照，不靠我的理解。修正前 12 條中 6 條失敗，修正後 12/12。原本一條「大小寫依 SamePathComponent」的測試預期是根據我的理解寫的，改成以 OS 對照。
- **新舊對照**：包含判定 20 個情況都和 OS 一致；和 framework 舊版不同的 4 個情況逐條印出，由 T2 接手。檔案戳記 3 個情況和 framework 一致。
- **突變的經緯**（判定都沒有放寬）：
  - 第一次 6/8：缺「連結後接 `..`」的測試；一般檔案檢查是等價突變（engine/os 也擋）。
  - 補測試後第二次 7/8：等價突變仍存活，刪掉多餘的檢查並移除該條。第三次 7/7。
  - xhigh 審查後語意修正，突變擴到 15 條。第四次 13/14：結尾分隔符那步只在 OS 解析失敗時有作用，補「上層目錄沒有執行權限」的測試後 1/1。
  - F8 要求補回一般檔案檢查（避免開具名管道或裝置），它在測試上又是等價的，不設突變，理由記在 HANDOFF。
- `os.boundary_guard` 通過：engine/io 的 `std::filesystem` 只用詞法操作，OS 存取都經 engine/os。
- 範圍的限界：UNC 與長路徑沒有在本機測；Windows 的大小寫與 8.3 短名只靠 CI 與推論；權限退回那條測試在 Windows 與超級使用者下無法觸發。

## 全量回歸

`build/target/_build/test`（test preset，GYO_TOOLS=AUTO）全量建置成功；CTest 115/115 通過（50.3 s，`-j 6`）。證據 `full_build.txt`、`ctest_full.txt`。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-engine-io-user-files/`（git 不追蹤，見該資料夾的 README）。xhigh 審查的原始輸出與探針在會話暫存區，未保存；結論在判定變更文件與 HANDOFF。
