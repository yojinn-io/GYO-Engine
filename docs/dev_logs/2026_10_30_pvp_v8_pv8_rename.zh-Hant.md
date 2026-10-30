# PvP v8：pv8-rename（proto 與套件改名為 v8）

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。計畫的檔位是 low｜Sonnet 5.5；實際由主會話（Opus 5.5）直接做，因為只有機械式的替換，交給子 agent 還要另外說明 prompt。

**狀態：** 完成。版本值仍是 7；改成 8 在 `v8-product-crouch` 的 wire 接入時做（[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md) §1、§8）。

相關文件：[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md)（已定稿，`pv8-final-approve-round2`）、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)。做法照 v7.1 的 `proto-rename-v7`（commit `d5104a0e`）。

## 做了什麼

- `client_v7.proto`／`runtime_v7.proto` 改名為 `client_v8.proto`／`runtime_v8.proto`；package 改為 `object_fps_pvp.client.v8`／`object_fps_pvp.runtime.v8`，`go_package` 改為 `clientv8`／`runtimev8`。
- Go 的 import（Gateway 與 adapter，共 32 個檔）、CMake target（`gyo_object_fps_pvp_client_v8`／`_runtime_v8`）、C++ 的 include 與 `namespace pb`、`IpcHostTests.cpp` 的完整限定名、`test_drop_reasons.py` 的 proto 路徑。
- `protocol/README.md` 與 [network-architecture](../object_fps_pvp/network-architecture.zh-Hant.md) §3 等處的檔名，並註明版本值還是 7。
- 不改：版本值、欄位、條文裡的「pv7 §N」引用（條文仍是 pv7 的）、歷史紀錄（dev_log、v7.1 的計畫文件）、遊戲版本 v7 的驗收腳本（`*_v7.py`）。

## 檢驗與經過

- 重產：先用專案的 protoc 36.2.0（`_deps/protobuf-build/protoc`）與 protoc-gen-go v1.36.11 重產 v7，和已提交的檔案位元一致；再重產 v8。把舊檔的 `client_v7`、`clientv7`、`client.v7`、`v7.proto`（runtime 同）換成 v8 後，和新檔的差異是 0 行。
- Go：`go vet ./...`、`go test ./...` 通過。
- C++：`test` preset 重新設定與全部建置；刪掉建置樹裡殘留的 `*_v7.pb.*` 後再建一次仍成功（沒有地方還引用 v7）。CTest 99／99 通過。
- 權威 digest 的兩樹比對：base `33127ad6`（改名前）、branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 各 1 次：scale 1、scale 3 都是「38 scenarios, 0 differing」。
- 突變（D184）：`mutations.json` 沒有任何 find 落在改名的識別字上，相關突變 0 個，沒有跑。
- 經過：第一次批次替換時 zsh 沒有切開檔案清單，perl 沒有改到檔案；改用 `xargs -0` 重跑。CMake 的 `"gyo_${...}_runtime_v7"` 因 `_` 前面沒有字界，第一輪沒換到，另外補上。兩者都在建置前發現。

## 證據

- 兩樹比對的輸出在會話的工作檔（`rename-s1`、`rename-s3`），不放進 repo；結果行照抄在上面。
