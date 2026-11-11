# Runtime 的組合

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §H（多 World 與 schedule 的組合）、§0 用語。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 名詞（spec §0）

- **Runtime**＝一個 World、它的 Registry、它的 schedule 集合，加每個 schedule 的 Executor 策略與執行緒綁定。
- **Host**＝行程的入口程式：建立一或多個 Runtime，擁有執行緒、時鐘、外部服務與交換點。
- 一個 World 一條擁有執行緒；兩個時鐘在不同執行緒就是兩個 World，經 Host 的交換點交換（spec H-2）。

## 2. Client：Frame World 與 Render World 分開（使用者 2026-11-11 決定，spec K.3 第 6 項）

- Client 有三個 Runtime：`ClientSim`（World S，FixedUpdate，runner＝`gyo-client-sim`）、`ClientFrame`（World F，FrameUpdate，runner＝Main）、`ClientRender`（World R，`Extract` → `RenderSubmit`，與 F 同在主執行緒）。
- **Render World 自己的 Registry**（各自凍結；render plugin 只登記到渲染 Registry，主 Registry 不因渲染而變 fingerprint）。
- **渲染只經 Extract 以 const 方式讀來源 World，無反饋**：extract System 的參數 `Extract<Read<T>>`／`ExtractRes<R>` 只讀來源；`Extract<Write<…>>` 不存在（編譯期擋下）；debug 下來源 World 在 extract 期間任何寫入都斷言；渲染 Runtime 不得以任何參數寫回來源，含 boundary Resource；來源 Registry 必須先凍結（spec H-6、J-43；DI-KS 13）。
- 代價：多一個 World 與每幀一次 extract。使用者的理由：合起來看著簡單，代價很高——合併後「渲染不改模擬」只能靠 RenderSubmit 唯讀的存取宣告守，而宣告的真實性在 C++ 攔不到（LIM-3）；分開則由機制守。
- 交換點：意圖 F → S、呈現 S → F 經 Host 擁有的 Mailbox，只在 schedule 之間存取；Link 經 Host 與網路執行緒在 S 的 schedule 前後交換。

## Design Invariants（候選；核准後成為正式條文）

1. **渲染只經 Extract 以 const 方式讀來源 World，無反饋**；渲染 World 有自己的 Registry；來源 Registry 先凍結。守衛：spec J-43（含編譯失敗測試 `Extract<Write>`）、J-16。
2. **一個 World 一條執行緒**：同一 World 的所有 schedule 綁同一執行緒；不同執行緒上的時鐘是不同的 World，只經 Host 交換點交換。守衛：spec J-15、J-6；finish 檢查 H-2。

### Replacement Criteria（候選）

- 渲染需要寫回模擬 World（例如渲染驅動的 LOD 狀態要進判定）→ 不是放寬 DI 1，而是把那份狀態搬到來源 World 由模擬側的 System 寫，渲染仍只讀。
- 第一個真實能力接進機制時仍要改 Host 迴圈或 Kernel 本體，或 Host 的協調邏輯在兩個以上 host 重複 → 「Host 又長回來」，停下（spec K.2 第 7 條）。
