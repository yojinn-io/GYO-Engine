# 2026-11-27 建置層清理（`v8-arch-gyo2-build-cleanup`）

Owner：建置層（GYO2 主會話轉來，任務卡）。作者 Opus 5.5／high。

處理批次表〈後續小事〉98、100、102、109、110 行（apps→games #294 留下的後續；141 行隨 98 行一起解消）。事前宣告寫在 PR 說明。

## 重現（動手前）

`ProjectCompositionTests.cmake` 用預設產生器（Unix Makefiles；macOS 的 GNU Make 3.81 以秒比較時刻）從 repo 根以 `-P` 跑 8 次：4 次失敗，都在 tool 的 `#error` 那一案（`:198`，「Invalid fixture unexpectedly succeeded」）——同一秒內改寫的原始碼沒被重建。紀錄在主 checkout `build/evidence/gyo2-build-cleanup/repro-composition/`。
